#pragma once

#include "config.h"

#include <filesystem>

namespace SettingsStorage {

struct SettingsBundle {
    EngineConfig engine;
    ViewConfig   view;
};

/// Settings path near executable. Falls back to source dir if needed.
std::filesystem::path settingsFilePath();

/// Load settings from disk. Returns defaults on parse/read failures.
SettingsBundle load();

/// Save settings to disk. Returns false if write fails.
bool save(const EngineConfig &engine, const ViewConfig &view);

} // namespace SettingsStorage
