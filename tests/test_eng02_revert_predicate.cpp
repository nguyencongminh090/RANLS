// Regression tests for ENG-02 — interrupting engine auto-play reverts
// "Engine plays" to Off.
//
// The MainWindow / CommandDispatcher call sites that perform the revert cannot
// be unit-tested here (tests/CMakeLists.txt forbids test sources from pulling
// in src/main_window.* / src/application.*, enforced by construction). What is
// testable — and is the load-bearing logic of the fix — is the pure predicate
// extracted from MainWindow::maybeStartAutoMove:
//
//   bool isEnginesTurn(EnginePlaysSide plays, Stone sideToMove)   [model/config.h]
//
// Both the auto-move path and the revert-on-manual-intervention path (toolbar
// Stop, analysis-panel Stop, Analyze-on-engine's-turn, dispatcher analyze /
// !play) gate on this single predicate, so covering it here guards the whole
// transition. Never delete this test (see /CLAUDE.md bug-fix workflow).

#include "vendor/doctest.h"

#include "model/board_state.h"  // Stone
#include "model/config.h"       // EnginePlaysSide, isEnginesTurn

TEST_CASE("ENG-02: isEnginesTurn — Off is never the engine's turn")
{
    CHECK_FALSE(isEnginesTurn(EnginePlaysSide::Off, Stone::Black));
    CHECK_FALSE(isEnginesTurn(EnginePlaysSide::Off, Stone::White));
    CHECK_FALSE(isEnginesTurn(EnginePlaysSide::Off, Stone::Empty));
}

TEST_CASE("ENG-02: isEnginesTurn — engine assigned Black")
{
    CHECK(isEnginesTurn(EnginePlaysSide::Black, Stone::Black));        // its turn
    CHECK_FALSE(isEnginesTurn(EnginePlaysSide::Black, Stone::White));  // opponent
    CHECK_FALSE(isEnginesTurn(EnginePlaysSide::Black, Stone::Empty));
}

TEST_CASE("ENG-02: isEnginesTurn — engine assigned White (symmetric)")
{
    CHECK(isEnginesTurn(EnginePlaysSide::White, Stone::White));        // its turn
    CHECK_FALSE(isEnginesTurn(EnginePlaysSide::White, Stone::Black));  // opponent
    CHECK_FALSE(isEnginesTurn(EnginePlaysSide::White, Stone::Empty));
}
