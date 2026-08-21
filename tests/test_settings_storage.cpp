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
