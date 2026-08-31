#pragma once

#include "board_state.h"   // GameRule, DEFAULT_BOARD_SIZE, MAX_BOARD_SIZE

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
    SingleSide = 0,   ///< One win-rate line, ALWAYS from Black's perspective, for
                      ///< every position, regardless of MatchConfig::enginePlays
                      ///< (UI-09 — reversed the UX-06 "follows the engine's side").
    BothSide   = 1    ///< Two perspective-correct lines: Black and White, each shown
                      ///< in that colour's own perspective.
};

/// Configuration for View/UI options.
struct ViewConfig {
    AppTheme theme           = AppTheme::Dark;
    bool     showMoveNumbers = true;
    bool     showCoordinates = true;
    bool     showDatabase    = true;
    WinGraphMode winGraphMode = WinGraphMode::BothSide;

    // Hotkeys.
    std::string hotkeyAnalyze  = "F5";
    std::string hotkeyStop     = "Escape";
    std::string hotkeyUndo     = "Ctrl+Z";
    std::string hotkeyRedo     = "Ctrl+Y";
    std::string hotkeyNewGame  = "Ctrl+N";
};

/// Which side, if any, the engine plays automatically (UI-06). `Off` (the
/// default) means the engine never self-moves — the toolbar Analyze/Stop
/// one-shot path is the only way to invoke it.
enum class EnginePlaysSide {
    Off   = 0,
    Black = 1,
    White = 2
};

/// ENG-02: pure predicate — is it the engine's turn to move, given the side it
/// is assigned to play and the current side-to-move? `Off` is never the
/// engine's turn. Single source of truth for the "engine's turn" check, shared
/// by MainWindow's auto-move path and the revert-on-manual-intervention path.
inline bool isEnginesTurn(EnginePlaysSide plays, Stone sideToMove) {
    return (plays == EnginePlaysSide::Black && sideToMove == Stone::Black)
        || (plays == EnginePlaysSide::White && sideToMove == Stone::White);
}

/// Match / play configuration (UI-06). Kept separate from EngineConfig and
/// ViewConfig on purpose — this governs auto-play behaviour, not engine
/// search parameters or UI presentation.
struct MatchConfig {
    EnginePlaysSide enginePlays = EnginePlaysSide::Off;
};

/// Persistence shape for the last-selected game rule and board size (STATE-04).
/// This is the *settings-file* representation only — GameState keeps its own
/// `rule_` / board members and its `setRule()` / `newGame()` API. `rule` is a
/// global preference restored on every launch; `boardSize` is the default size
/// for future launches and New Game.
struct GameSetupConfig {
    GameRule rule      = GameRule::Freestyle;
    int      boardSize = DEFAULT_BOARD_SIZE;
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
