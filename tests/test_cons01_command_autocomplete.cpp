// CONS-01 — dispatcher accessors + BottomPanel AutoComplete wiring.
//
// Two layers:
//   * CommandDispatcher::registeredNames() / commandUsage() — read-only
//     accessors over the existing specs. No display server needed.
//   * BottomPanel's popover/ghost-text glue, driven against a REAL widget
//     (mirrors test_anlz05_no_automove_action.cpp). Self-skips with no
//     display server; main() lives in test_ui07_pv_view_rows.cpp.

#include "vendor/doctest.h"

#include <gtkmm.h>
#include <giomm.h>

#include "command/command_dispatcher.h"
#include "engine/engine_controller.h"
#include "engine/engine_process.h"
#include "model/game_state.h"
#include "ui/bottom_panel.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

bool gtkReady() { return gtk_init_check(); }

bool pumpUntil(const std::function<bool()> &done, int timeoutMs = 3000)
{
    auto *ctx = g_main_context_default();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!done()) {
        g_main_context_iteration(ctx, FALSE);
        if (std::chrono::steady_clock::now() >= deadline) return false;
        g_usleep(1000);
    }
    return true;
}

bool contains(const std::vector<std::string> &v, const std::string &s)
{
    return std::find(v.begin(), v.end(), s) != v.end();
}

CommandContext makeCtx(GameState &gs, EngineProcess &eng, EngineController &ctrl)
{
    return CommandContext{
        .gameState = gs,
        .engine = eng,
        .controller = ctrl,
        .print = [](const std::string &) {},
        .clearConsole = [] {},
    };
}

}  // namespace

// ─── Dispatcher accessors ──────────────────────────────────────────────────

TEST_CASE("CONS-01: registeredNames() exposes every built-in, sorted")
{
    GameState gs;
    EngineProcess eng;
    EngineController ctrl(gs, eng);
    CommandDispatcher disp(makeCtx(gs, eng, ctrl));

    auto names = disp.registeredNames();
    REQUIRE_FALSE(names.empty());
    CHECK(std::is_sorted(names.begin(), names.end()));

    // Every name registerBuiltins() installs must be present. The canonical
    // built-in list is pinned == registerBuiltins() by test_proto03.
    for (const auto &b : protoext::builtinCommandNames())
        CHECK_MESSAGE(contains(names, b), b);

    CHECK(disp.commandUsage("analyze") == "!analyze [n]");
    CHECK(disp.commandUsage("rule") == "!rule <freestyle|standard|renju>");
    CHECK(disp.commandUsage("nonexistent-cmd").empty());

    // R5: no engine, no .ptc → syncExtensionCommands() is a quiet no-op and
    // the built-in set is unchanged.
    auto before = disp.registeredNames();
    disp.syncExtensionCommands();
    CHECK(disp.registeredNames() == before);
}

TEST_CASE("CONS-01: registeredNames() grows after syncExtensionCommands() picks up a .ptc")
{
    if (!gtkReady()) return;  // startEngine() needs a GLib main loop pump

    // A one-command extension table on disk.
    const std::string ptcPath =
        std::string(std::filesystem::temp_directory_path() / "cons01_stub.ptc");
    {
        std::ofstream out(ptcPath);
        out << "ptc_version = 1\n"
               "extends = \"gomocup\"\n"
               "[[command]]\n"
               "name = \"yxstubcmd\"\n"
               "group = \"info\"\n"
               "send = \"YXSTUB\"\n";
    }

    GameState gs;
    EngineProcess eng;
    EngineController ctrl(gs, eng);
    CommandDispatcher disp(makeCtx(gs, eng, ctrl));

    CHECK_FALSE(contains(disp.registeredNames(), "yxstubcmd"));

    EngineConfig ec = gs.engineConfig();
    ec.enginePath = MOCK_ENGINE_PATH;              // PORT-03 portable stand-in
    ec.protocolExtensionPath = ptcPath;
    gs.setEngineConfig(ec);

    ctrl.startEngine();                            // loads the .ptc table
    REQUIRE(pumpUntil([&] { return eng.isRunning(); }));

    disp.syncExtensionCommands();
    auto names = disp.registeredNames();
    CHECK(contains(names, "yxstubcmd"));
    CHECK(std::is_sorted(names.begin(), names.end()));
    CHECK(disp.commandUsage("yxstubcmd") == "!yxstubcmd [args...]");

    ctrl.stopEngine([] {});
    pumpUntil([&] { return !eng.isRunning(); });
    std::remove(ptcPath.c_str());
}

// ─── BottomPanel AutoComplete glue ─────────────────────────────────────────

struct RanlsCons01Probe {
    BottomPanel &bp;
    Gtk::Entry &entry() { return bp.commandEntry_; }
    const std::vector<std::string> &matches() const { return bp.suggestionMatches_; }
    bool open() const { return bp.suggestOpen_; }
    // Mirrors the production guard in the entry's EventControllerKey lambda:
    // the suggestion handler only sees a key while the popover is open.
    bool key(unsigned int keyval)
    {
        return bp.suggestOpen_ && bp.handleSuggestionKey(keyval);
    }
    void type(const std::string &s)
    {
        bp.commandEntry_.set_text(s);
        bp.commandEntry_.set_position(-1);
    }
};

TEST_CASE("CONS-01: BottomPanel popover model + Tab rewrites the entry")
{
    if (!gtkReady()) return;

    BottomPanel bp;
    RanlsCons01Probe p{bp};

    GameState gs;
    EngineProcess eng;
    EngineController ctrl(gs, eng);
    CommandDispatcher disp(makeCtx(gs, eng, ctrl));
    bp.setCommandNameProvider([&] { return disp.registeredNames(); });
    bp.setCommandUsageProvider([&](const std::string &n) { return disp.commandUsage(n); });

    SUBCASE("'!an' suggests analyze and Tab completes to '!analyze '")
    {
        p.type("!an");
        CHECK(p.open());
        CHECK(p.matches() == std::vector<std::string>{"analyze"});

        CHECK(p.key(GDK_KEY_Tab));
        CHECK(std::string(p.entry().get_text()) == "!analyze ");
    }

    SUBCASE("raw protocol line: no popover, key handler is inert")
    {
        p.type("YXBOARD");
        CHECK_FALSE(p.open());
        CHECK_FALSE(p.key(GDK_KEY_Tab));
    }

    SUBCASE("Esc hides the popover and leaves the half-typed text")
    {
        p.type("!an");
        REQUIRE(p.open());
        CHECK(p.key(GDK_KEY_Escape));
        CHECK_FALSE(p.open());
        CHECK(std::string(p.entry().get_text()) == "!an");
    }

    SUBCASE("Enter with the popover open accepts into the entry, does not submit")
    {
        bool submitted = false;
        bp.signal_command_sent.connect([&](std::string) { submitted = true; });
        p.type("!an");
        REQUIRE(p.open());
        CHECK(p.key(GDK_KEY_Return));
        CHECK(std::string(p.entry().get_text()) == "!analyze ");
        CHECK_FALSE(submitted);
    }
}
