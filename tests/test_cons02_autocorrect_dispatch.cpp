// CONS-02 — dispatcher helpers + BottomPanel pre-dispatch AutoCorrect wiring.
//
// Two layers:
//   * CommandDispatcher::nearestNames() + the case-insensitive handlers_
//     lookup / B4 "did you mean" error text in executeLine().
//   * BottomPanel's submit-path AutoCorrect glue, driven against a REAL widget
//     (mirrors test_cons01_command_autocomplete.cpp). Self-skips with no
//     display server; main() lives in test_ui07_pv_view_rows.cpp.

#include "vendor/doctest.h"

#include <gtkmm.h>
#include <giomm.h>

#include "command/command_completer.h"
#include "command/command_dispatcher.h"
#include "engine/engine_controller.h"
#include "engine/engine_process.h"
#include "model/game_state.h"
#include "ui/bottom_panel.h"

#include <string>
#include <vector>

namespace {

bool gtkReady() { return gtk_init_check(); }

}  // namespace

// ─── Dispatcher: nearestNames + case-insensitive lookup + B4 text ───────────

namespace {
struct CaptureCtx {
    GameState        gs;
    EngineProcess    eng;
    EngineController  ctrl{gs, eng};
    std::vector<std::string> out;

    CommandContext ctx()
    {
        return CommandContext{
            .gameState = gs,
            .engine = eng,
            .controller = ctrl,
            .print = [this](const std::string &l) { out.push_back(l); },
            .clearConsole = [] {},
        };
    }
};
}  // namespace

TEST_CASE("CONS-02: nearestNames() wraps didYouMean over the live registry")
{
    CaptureCtx c;
    CommandDispatcher disp(c.ctx());

    CHECK(disp.nearestNames("undoo") == std::vector<std::string>{"undo"});
    CHECK(disp.nearestNames("anlyze") == std::vector<std::string>{"analyze"});
    CHECK(disp.nearestNames("xyzzy").empty());
}

TEST_CASE("CONS-02: executeLine() resolves the handler name case-insensitively")
{
    CaptureCtx c;
    CommandDispatcher disp(c.ctx());

    // '!ANALYZE' must reach the analyze handler (which, with no engine, prints
    // its own "Engine not running" error) — never the unknown-command branch.
    disp.executeLine("!ANALYZE");
    REQUIRE_FALSE(c.out.empty());
    CHECK(c.out.back() == "ERR: Engine not running. Use: !engine start");
}

TEST_CASE("CONS-02: B4 — unknown '!' command offers 'did you mean'")
{
    CaptureCtx c;
    CommandDispatcher disp(c.ctx());

    disp.executeLine("!anlyze");
    REQUIRE_FALSE(c.out.empty());
    CHECK(c.out.back() == "ERR: Unknown internal command 'anlyze'. Did you mean: !analyze?");

    c.out.clear();
    disp.executeLine("!xyzzy");
    REQUIRE_FALSE(c.out.empty());
    CHECK(c.out.back() == "ERR: Unknown internal command: xyzzy (try: !help)");
}

// ─── BottomPanel submit-path AutoCorrect glue ──────────────────────────────

struct RanlsCons02Probe {
    BottomPanel &bp;
    Gtk::Entry &entry() { return bp.commandEntry_; }
    void type(const std::string &s)
    {
        bp.commandEntry_.set_text(s);
        bp.commandEntry_.set_position(-1);
    }
    void submit() { g_signal_emit_by_name(bp.commandEntry_.gobj(), "activate"); }
    std::vector<EngineLogLine> pending() const { return bp.pendingAppend_; }
};

TEST_CASE("CONS-02: BottomPanel rewrites, logs, and dispatches the corrected line")
{
    if (!gtkReady()) return;

    BottomPanel bp;
    RanlsCons02Probe p{bp};

    CaptureCtx c;
    CommandDispatcher disp(c.ctx());
    bp.setCommandNameProvider([&] { return disp.registeredNames(); });

    std::string sent;
    std::string entryAtDispatch;
    bp.signal_command_sent.connect([&](std::string cmd) {
        sent = cmd;
        entryAtDispatch = std::string(p.entry().get_text());
    });

    SUBCASE("'analyze 10' (no bang) → '!analyze 10', visible + logged")
    {
        p.type("analyze 10");
        p.submit();

        CHECK(sent == "!analyze 10");
        CHECK(entryAtDispatch == "!analyze 10");  // visible before the post-dispatch clear

        REQUIRE_FALSE(p.pending().empty());
        const auto line = p.pending().back();  // pending() returns by value — copy, don't bind a ref
        CHECK(line.prefix == "MESSAGE");
        CHECK(line.tag == LogTagKind::RecvMessage);
        CHECK(line.text == "corrected: !analyze 10");
    }

    SUBCASE("'!ANALYZE' → '!analyze', one corrected line")
    {
        p.type("!ANALYZE");
        p.submit();
        CHECK(sent == "!analyze");
        REQUIRE_FALSE(p.pending().empty());
        CHECK(p.pending().back().text == "corrected: !analyze");
    }

    SUBCASE("fullwidth '！analyze' → unchanged, no corrected line")
    {
        const std::string fw = command_completer::fullwidthBang() + "analyze";
        p.type(fw);
        p.submit();
        CHECK(sent == fw);
        CHECK(p.pending().empty());
    }

    SUBCASE("'foo bar' (not registered) → passed through untouched")
    {
        p.type("foo bar");
        p.submit();
        CHECK(sent == "foo bar");
        CHECK(p.pending().empty());
    }
}
