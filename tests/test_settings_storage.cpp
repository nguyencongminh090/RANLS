// Regression test for STATE-02: SettingsDialog::onApply used to build a
// fresh, default-constructed EngineConfig/ViewConfig and only assign the
// fields the dialog exposed controls for, so EngineConfig::multiPV and
// EngineConfig::customParams (set only via console commands, never through
// the dialog) were silently reset to their struct defaults every time the
// user pressed Apply -- and MainWindow::onSettings persists that reset to
// disk unconditionally via SettingsStorage::save, making the loss permanent.
//
// SettingsDialog::onApply itself is gtkmm UI code and out of reach of this
// test binary (tests/CMakeLists.txt deliberately excludes gtkmm -- see its
// header comment and docs/audit/2026-08-21-test-framework-choice.md), so
// this test instead pins the underlying persistence-layer contract the fix
// depends on: SettingsStorage::save()/load() must round-trip a non-default
// multiPV and a custom param placed in EngineConfig::customParams. Before
// this fix, customParams was never written to disk at all, so this
// round-trip was unsatisfiable regardless of what onApply did.
//
// The onApply merge-from-current-config mechanism itself (the actual fix
// for the reported bug) is verified by source reading: onApply now does
// `EngineConfig eConfig = baseEngineConfig_;` / `ViewConfig vConfig =
// baseViewConfig_;` (copies of the config the dialog was opened with)
// instead of default-constructing fresh structs, then only overwrites the
// fields its own controls own -- see src/ui/settings_dialog.cpp.

#include "vendor/doctest.h"

#include "model/settings_storage.h"

#include <cstdio>
#include <fstream>

TEST_CASE("SettingsStorage: multiPV and customParams survive a save/load round-trip") {
    EngineConfig engine;
    engine.enginePath = "/tmp/some-engine";
    engine.multiPV    = 8; // non-default (struct default is 1)
    engine.customParams["some_unknown_info_key"] = "some_value";

    ViewConfig view;

    REQUIRE(SettingsStorage::save(engine, view));

    SettingsStorage::SettingsBundle loaded = SettingsStorage::load();

    CHECK(loaded.engine.multiPV == 8);
    REQUIRE(loaded.engine.customParams.count("some_unknown_info_key") == 1);
    CHECK(loaded.engine.customParams.at("some_unknown_info_key") == "some_value");

    // Clean up the settings file this test wrote so repeated runs start from
    // a clean slate and don't leak state into other tests/executions.
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}

TEST_CASE("SettingsStorage: load() on a missing file returns EngineConfig/ViewConfig defaults") {
    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    SettingsStorage::SettingsBundle loaded = SettingsStorage::load();
    EngineConfig defaultEngine;

    CHECK(loaded.engine.multiPV == defaultEngine.multiPV);
    CHECK(loaded.engine.customParams.empty());
}

// UI-06: MatchConfig::enginePlays must round-trip through save()/load().
TEST_CASE("SettingsStorage: MatchConfig::enginePlays survives a save/load round-trip") {
    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    // Default is Off, and a missing file yields the default.
    CHECK(MatchConfig{}.enginePlays == EnginePlaysSide::Off);
    CHECK(SettingsStorage::load().match.enginePlays == EnginePlaysSide::Off);

    // A set-to-Black value survives save + load.
    EngineConfig engine;
    ViewConfig   view;
    MatchConfig  match;
    match.enginePlays = EnginePlaysSide::Black;

    REQUIRE(SettingsStorage::save(engine, view, match));
    CHECK(SettingsStorage::load().match.enginePlays == EnginePlaysSide::Black);

    // And a save with the default MatchConfig reads back as Off.
    REQUIRE(SettingsStorage::save(engine, view));
    CHECK(SettingsStorage::load().match.enginePlays == EnginePlaysSide::Off);

    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}

// UX-06: `ui_profile` was removed. An old settings file that still carries the
// key must load without error (the key is silently ignored), and every other
// field in that file must still be honoured.
TEST_CASE("SettingsStorage: legacy ui_profile key is ignored, other fields still load") {
    auto path = SettingsStorage::settingsFilePath();
    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream f(path, std::ios::trunc);
        f << "# legacy file\n";
        f << "ui_profile=Compact\n";
        f << "theme=1\n";
        f << "show_coordinates=false\n";
        f << "win_graph_mode=0\n";
        f << "max_depth=42\n";
    }

    SettingsStorage::SettingsBundle loaded = SettingsStorage::load();
    CHECK(loaded.view.theme == AppTheme::Light);
    CHECK(loaded.view.showCoordinates == false);
    CHECK(loaded.view.winGraphMode == WinGraphMode::SingleSide);
    CHECK(loaded.engine.maxDepth == 42);

    std::remove(path.string().c_str());
}

// UX-06: theme, showCoordinates and winGraphMode must survive a save/load
// round-trip (they are what the Settings dialog's UI tab writes).
TEST_CASE("SettingsStorage: theme / coordinates / winGraphMode round-trip") {
    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    EngineConfig engine;
    ViewConfig   view;
    view.theme           = AppTheme::Light;
    view.showCoordinates = false;
    view.showMoveNumbers = false;
    view.winGraphMode    = WinGraphMode::SingleSide;

    REQUIRE(SettingsStorage::save(engine, view));

    SettingsStorage::SettingsBundle loaded = SettingsStorage::load();
    CHECK(loaded.view.theme == AppTheme::Light);
    CHECK(loaded.view.showCoordinates == false);
    CHECK(loaded.view.showMoveNumbers == false);
    CHECK(loaded.view.winGraphMode == WinGraphMode::SingleSide);

    view.winGraphMode = WinGraphMode::BothSide;
    view.theme        = AppTheme::Dark;
    REQUIRE(SettingsStorage::save(engine, view));
    loaded = SettingsStorage::load();
    CHECK(loaded.view.winGraphMode == WinGraphMode::BothSide);
    CHECK(loaded.view.theme == AppTheme::Dark);

    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}

// STATE-04: GameSetupConfig (selected rule + board size) must round-trip
// through save()/load(), fall back to the struct defaults on out-of-range
// values, and never corrupt the other config blocks.
TEST_CASE("SettingsStorage: GameSetupConfig defaults on a missing file") {
    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    CHECK(GameSetupConfig{}.rule == GameRule::Freestyle);
    CHECK(GameSetupConfig{}.boardSize == DEFAULT_BOARD_SIZE); // 15

    SettingsStorage::SettingsBundle loaded = SettingsStorage::load();
    CHECK(loaded.setup.rule == GameRule::Freestyle);
    CHECK(loaded.setup.boardSize == 15);
}

TEST_CASE("SettingsStorage: GameSetupConfig set-and-reload round-trip") {
    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    EngineConfig    engine;
    ViewConfig      view;
    MatchConfig     match;
    GameSetupConfig setup;
    setup.rule      = GameRule::Renju;
    setup.boardSize = 20;

    REQUIRE(SettingsStorage::save(engine, view, match, setup));

    SettingsStorage::SettingsBundle loaded = SettingsStorage::load();
    CHECK(loaded.setup.rule == GameRule::Renju);
    CHECK(loaded.setup.boardSize == 20);

    // A save with a default GameSetupConfig reads back as Freestyle / 15.
    REQUIRE(SettingsStorage::save(engine, view, match));
    loaded = SettingsStorage::load();
    CHECK(loaded.setup.rule == GameRule::Freestyle);
    CHECK(loaded.setup.boardSize == 15);

    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}

TEST_CASE("SettingsStorage: out-of-range rule / board_size fall back to defaults") {
    auto path = SettingsStorage::settingsFilePath();
    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream f(path, std::ios::trunc);
        f << "# hand-written\n";
        f << "rule=9\n";        // not in {0,1,2}
        f << "board_size=2\n";  // below the 5..MAX_BOARD_SIZE range
        f << "max_depth=42\n";  // an unrelated block must still load
    }

    SettingsStorage::SettingsBundle loaded = SettingsStorage::load();
    CHECK(loaded.setup.rule == GameRule::Freestyle);
    CHECK(loaded.setup.boardSize == DEFAULT_BOARD_SIZE);
    CHECK(loaded.engine.maxDepth == 42);

    // An over-large board size is rejected the same way.
    {
        std::ofstream f(path, std::ios::trunc);
        f << "board_size=" << (MAX_BOARD_SIZE + 1) << "\n";
    }
    CHECK(SettingsStorage::load().setup.boardSize == DEFAULT_BOARD_SIZE);

    // Boundary values are accepted.
    {
        std::ofstream f(path, std::ios::trunc);
        f << "board_size=5\n";
    }
    CHECK(SettingsStorage::load().setup.boardSize == 5);
    {
        std::ofstream f(path, std::ios::trunc);
        f << "board_size=" << MAX_BOARD_SIZE << "\n";
    }
    CHECK(SettingsStorage::load().setup.boardSize == MAX_BOARD_SIZE);

    std::remove(path.string().c_str());
}

TEST_CASE("SettingsStorage: saving GameSetupConfig does not corrupt the other blocks") {
    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    EngineConfig engine;
    engine.enginePath = "/tmp/state04-engine";
    engine.multiPV    = 5;
    engine.customParams["k"] = "v";

    ViewConfig view;
    view.theme        = AppTheme::Light;
    view.winGraphMode = WinGraphMode::SingleSide;

    MatchConfig match;
    match.enginePlays = EnginePlaysSide::White;

    GameSetupConfig setup;
    setup.rule      = GameRule::Standard;
    setup.boardSize = 17;

    REQUIRE(SettingsStorage::save(engine, view, match, setup));

    SettingsStorage::SettingsBundle loaded = SettingsStorage::load();
    CHECK(loaded.engine.enginePath == "/tmp/state04-engine");
    CHECK(loaded.engine.multiPV == 5);
    REQUIRE(loaded.engine.customParams.count("k") == 1);
    CHECK(loaded.engine.customParams.at("k") == "v");
    CHECK(loaded.view.theme == AppTheme::Light);
    CHECK(loaded.view.winGraphMode == WinGraphMode::SingleSide);
    CHECK(loaded.match.enginePlays == EnginePlaysSide::White);
    CHECK(loaded.setup.rule == GameRule::Standard);
    CHECK(loaded.setup.boardSize == 17);

    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}
