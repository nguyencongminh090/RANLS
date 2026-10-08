// PAL-02 — widget-level: the CommandPalette popover (search, selection, run,
// close) and the real MainWindow wiring (catalog -> palette items).
// Links gtkmm; self-skips with no display server.

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "main_window.h"
#include "model/settings_storage.h"
#include "ui/command_palette.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>

// Test-only accessor — MainWindow declares `friend struct RanlsPal02Probe`.
struct RanlsPal02Probe {
    MainWindow &w;
    std::vector<PaletteItem> items() { return w.paletteItems(); }
    CommandPalette &palette() { return *w.palette_; }
    GameState &gs() { return w.gameState_; }
    void toggle() { w.onCommandPalette(); }
};

namespace {

bool gtkReady() { return gtk_init_check(); }

void pump(int ms = 50)
{
    auto *ctx = g_main_context_default();
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end)
        while (g_main_context_iteration(ctx, FALSE)) {}
}

std::string slurp(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

PaletteItem item(const std::string &id, const std::string &en, const std::string &vi, const std::string &kw,
                 int *counter, bool enabled = true)
{
    PaletteItem it;
    it.entry = {id, id.substr(0, 3), en, vi, kw, "", "Action", enabled};
    it.run   = [counter]() { ++*counter; };
    return it;
}

struct Fixture {
    Gtk::Window win;
    Gtk::Box    root{Gtk::Orientation::VERTICAL};
    int         ranSettings = 0, ranUndo = 0, ranDisabled = 0, ranExt = 0;
    std::vector<PaletteItem> items;
    std::unique_ptr<CommandPalette> palette;
    std::vector<std::string> ran;

    Fixture()
    {
        win.set_child(root);
        win.set_default_size(800, 600);
        win.present();
        pump();
        items = {item("act.settings", "Settings", "Cài đặt", "preferences;options", &ranSettings),
                 item("act.nav-undo", "Undo", "Hoàn tác", "back;take back", &ranUndo),
                 item("act.save-game", "Save Game", "Lưu ván cờ", "write", &ranDisabled, false)};
        palette = std::make_unique<CommandPalette>(root, slurp(PALETTE_LEXICON_PATH));
        palette->setItemsProvider([this]() { return items; });
        palette->signal_item_run.connect([this](const std::string &id) { ran.push_back(id); });
    }
};

}  // namespace

TEST_CASE("PAL-02 palette: typing ranks results; Enter runs the selection after closing")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    Fixture f;
    f.palette->open();
    CHECK(f.palette->isOpen());

    f.palette->setQuery("cai dat");     // no diacritics, Vietnamese
    REQUIRE_FALSE(f.palette->resultIds().empty());
    CHECK(f.palette->resultIds().front() == "act.settings");

    CHECK(f.palette->runSelected());
    CHECK_FALSE(f.palette->isOpen());
    pump();                              // the action runs from an idle
    CHECK(f.ranSettings == 1);
    CHECK(f.ran == std::vector<std::string>{"act.settings"});
}

TEST_CASE("PAL-02 palette: Up/Down move the selection")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    Fixture f;
    f.palette->open();
    f.palette->setQuery("settings undo");   // OR of terms: two enabled items match
    const auto ids = f.palette->resultIds();
    REQUIRE(ids.size() >= 2);
    f.palette->moveSelection(+1);
    CHECK(f.palette->runSelected());
    pump();
    REQUIRE(f.ran.size() == 1);
    CHECK(f.ran[0] == ids[1]);
}

TEST_CASE("PAL-02 palette: a disabled item is shown but never runs")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    Fixture f;
    f.palette->open();
    f.palette->setQuery("save game");
    REQUIRE_FALSE(f.palette->resultIds().empty());
    CHECK(f.palette->resultIds().front() == "act.save-game");
    CHECK(f.palette->runSelected());     // row consumed, but...
    pump();
    CHECK(f.ranDisabled == 0);           // ...the disabled item did not run
    CHECK(f.ran.empty());
}

TEST_CASE("PAL-02 palette: empty query lists recent items; items are re-queried on every open")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    Fixture f;
    f.palette->setRecentProvider([]() { return std::vector<std::string>{"act.nav-undo", "act.settings"}; });
    f.palette->open();
    CHECK(f.palette->resultIds() == std::vector<std::string>{"act.nav-undo", "act.settings"});

    f.palette->close();
    pump();
    f.items.push_back(item("cmd.mycmd", "!mycmd — An extension command", "", "extension", &f.ranExt));
    f.palette->open();                   // provider re-queried
    f.palette->setQuery("!mycmd");
    REQUIRE_FALSE(f.palette->resultIds().empty());
    CHECK(f.palette->resultIds().front() == "cmd.mycmd");
}

TEST_CASE("PAL-02 palette: Escape path closes without running")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    Fixture f;
    f.palette->open();
    f.palette->close();
    pump();
    CHECK_FALSE(f.palette->isOpen());
    CHECK(f.ran.empty());
}

TEST_CASE("PAL-02 MainWindow: every catalog action has a handler and Ctrl+K path toggles the palette")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
    MainWindow window;
    window.present();   // a popover only shows under a mapped parent
    pump(200);
    RanlsPal02Probe p{window};

    const auto items = p.items();
    int act = 0, set = 0, cmd = 0;
    for (const auto &it : items) {
        CHECK_MESSAGE(static_cast<bool>(it.run), it.entry.id);
        act += it.entry.kind == "act";
        set += it.entry.kind == "set";
        cmd += it.entry.kind == "cmd";
    }
    CHECK(act == 22);   // no dead rows dropped: every act.* has a handler
    CHECK(set == 20);
    CHECK(cmd >= 19);   // built-ins (+ any extension commands)

    CHECK_FALSE(p.palette().isOpen());
    p.toggle();
    CHECK(p.palette().isOpen());
    p.toggle();
    pump();
    CHECK_FALSE(p.palette().isOpen());
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}

TEST_CASE("PAL-02 MainWindow: running a palette item records it as recent and persists it")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
    MainWindow window;
    window.present();
    pump(200);
    RanlsPal02Probe p{window};

    p.palette().open();
    p.palette().setQuery("undo");
    REQUIRE_FALSE(p.palette().resultIds().empty());
    const std::string id = p.palette().resultIds().front();
    CHECK(p.palette().runSelected());
    pump();
    REQUIRE_FALSE(p.gs().viewConfig().paletteRecent.empty());
    CHECK(p.gs().viewConfig().paletteRecent.front() == id);
    CHECK(SettingsStorage::load().view.paletteRecent.front() == id);
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}

TEST_CASE("PAL-04 palette: scope prefixes filter by kind, the chip and empty state show them")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    Fixture f;
    f.items.push_back(item("cmd.undo", "!undo — Take back the last move", "", "undo", &f.ranExt));
    f.items.push_back(item("set.hotkey-undo", "Hotkey: Undo", "", "shortcut", &f.ranExt));
    f.palette->open();

    f.palette->setQuery("undo");                 // default: actions + settings, never cmd.*
    CHECK(f.palette->scopeText() == "Actions + settings");
    CHECK(f.palette->resultIds() == std::vector<std::string>{"act.nav-undo", "set.hotkey-undo"});

    f.palette->setQuery(">undo");
    CHECK(f.palette->scopeText() == "Actions");
    CHECK(f.palette->resultIds() == std::vector<std::string>{"act.nav-undo"});

    f.palette->setQuery("@undo");
    CHECK(f.palette->scopeText() == "Settings");
    CHECK(f.palette->resultIds() == std::vector<std::string>{"set.hotkey-undo"});

    f.palette->setQuery("!undo");
    CHECK(f.palette->scopeText() == "Commands");
    CHECK(f.palette->resultIds() == std::vector<std::string>{"cmd.undo"});

    // Only a console command answers: the default scope stays empty and hints at '!'.
    f.items.push_back(item("cmd.zzzonly", "!zzzonly — Console only", "", "zzzonly", &f.ranExt));
    f.palette->close();
    pump();
    f.palette->open();
    f.palette->setQuery("zzzonly");
    CHECK(f.palette->resultIds().empty());
    CHECK(f.palette->emptyStateText().find("! to search console commands") != std::string::npos);

    // The empty (no-query) state documents every prefix.
    f.palette->setQuery("");
    const std::string hint = f.palette->emptyStateText();
    for (const char *p : {"!  commands", ">  actions", "@  settings"})
        CHECK(hint.find(p) != std::string::npos);
}
