#pragma once

#include <string>
#include <cstdint>
#include <unordered_map>

/// UI Theme preset.
enum class AppTheme {
    System = 0,
    Light  = 1,
    Dark   = 2
};

enum class WinGraphMode {
    SingleSide = 0,   ///< Show side-to-move specific values as black/white lines.
    BothSide   = 1    ///< Show one normalized black-scale line.
};

/// Configuration for View/UI options.
struct ViewConfig {
    AppTheme theme           = AppTheme::Dark;
    bool     showMoveNumbers = true;
    bool     showCoordinates = true;
    bool     showDatabase    = true;
    WinGraphMode winGraphMode = WinGraphMode::BothSide;

    // UI customization profile + hotkeys.
    std::string uiProfile      = "Default";
    std::string hotkeyAnalyze  = "F5";
    std::string hotkeyStop     = "Escape";
    std::string hotkeyUndo     = "Ctrl+Z";
    std::string hotkeyRedo     = "Ctrl+Y";
    std::string hotkeyNewGame  = "Ctrl+N";
};

/// Engine configuration.
struct EngineConfig {
    std::string enginePath = "engine/pbrain-rapfi";
    int64_t     timeoutTurn   = 0;          ///< ms per turn (0 = unlimited)
    int64_t     timeoutMatch  = 0;          ///< ms per match (0 = unlimited)
    int         increment     = 0;          ///< time increment ms
    int         maxDepth      = 100;
    int64_t     maxNodes      = 0;          ///< 0 = unlimited
    int         threads       = 1;
    int         hashSizeMB    = 256;
    int         multiPV       = 1;          ///< Set via commands (e.g. !analyze N)
    std::unordered_map<std::string, std::string> customParams; ///< For unknown INFO keys
};
