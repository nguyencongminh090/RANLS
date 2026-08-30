# UI-06 — "Analysis" menu duplicates the toolbar; repurpose to per-player engine assignment

**Status:** ✅ DONE (Sprint 6, 2026-08-30) — design questions resolved with user 2026-08-30, implemented via `/implement-task`, closed by the user after a live-engine smoke pass (that same pass surfaced the separate pre-existing STATE-04 persistence gap, now also fixed).

### Implementation progress (2026-08-30)

Code complete and building clean; unit tests pass. Remaining: a human end-to-end
smoke pass with a real engine (see "Verification" step 3).

**Done:**
- `src/model/config.h`: `enum class EnginePlaysSide { Off, Black, White }` + `struct MatchConfig`.
- `GameState`: `matchConfig_` member, `matchConfig()` / `setMatchConfig()` (emits `signal_config_changed`).
- `src/model/settings_storage.{h,cpp}`: `SettingsBundle::match`; `engine_plays` key
  serialized/deserialized next to the ViewConfig block; `save()` gained a
  `const MatchConfig& = {}` param. `MainWindow::onSettings` passes the live
  `MatchConfig` through so a Settings Apply cannot reset it.
- `tests/test_settings_storage.cpp`: round-trip case — default is `Off`, a
  set-to-Black value survives save+load, a default save reads back `Off`.
- `src/main_window.cpp` `buildMenuBar()`: old "Analysis" submenu (pure toolbar
  duplication) replaced with an **Engine plays** submenu backed by
  `Gio::SimpleAction::create_radio_string("engine-plays", "off")` — entries
  `win.engine-plays::black` / `::white` / `::off`. `onSetEnginePlays()` updates
  `MatchConfig` + persists; `syncEnginePlaysMenu()` seeds the radio state from
  the loaded `MatchConfig` at startup (both directions kept in sync).
- Auto-move: `IEngineProtocol::generateMoveRequest()` (new) → `GomocupProtocol`
  emits `BOARD …/DONE` (or `BEGIN` on the empty board) which *starts a search*
  and returns one committed move, unlike the analysis-only `generateAnalyzeRequest`.
  `EngineController::requestEngineMove()` sends it and transitions to Analyzing;
  the existing `protocol_->signal_move` → `signal_engine_move` → `GameState::makeMove`
  path plays it. `MainWindow::maybeStartAutoMove()` (on `signal_board_changed`
  and on the engine reaching `Idle`) fires it via a coalesced idle callback,
  gated on: `enginePlays != Off`, engine running + `Idle`, and `enginePlays`
  matching `board().sideToMove()`. No loop: the engine's move flips side-to-move.
- `tests/test_gomocup_protocol.cpp`: `generateMoveRequest` BOARD/DONE + BEGIN cases.

**Verification run:** `./build.sh` clean (no new warnings); `ctest` 100% pass
(new MatchConfig + protocol cases included); app launches without crash. NOT
run: interactive menu-click + engine self-move with a live engine — no engine
binary present in this environment and the GUI could not be driven here.

**Smoke feedback (user, 2026-08-30):** the user ran the menu-click + engine
self-move smoke — the "Engine plays" selector and auto-move behave as specified.
That pass also surfaced a separate pre-existing persistence gap — Rule and Board
Size are not written to the settings file and reset on launch. Filed and fixed as
**STATE-04**; not a UI-06 regression — `engine_plays` itself *is* round-tripped in
`settings_storage.cpp`. UI-06 closed by the user 2026-08-30.
**Area:** menu bar (`src/main_window.cpp` menu construction), engine/controller wiring
**Priority:** P3
**Source:** filed 2026-08-30 from a UI review session

## Problem

The menu bar's **Analysis** menu offers the same Analyze / Stop actions already on the toolbar
(`▶ Analyze` / `■ Stop`) — pure duplication, no unique entries.

Proposed replacement: use that menu (or a nearby one) to assign which side the engine plays / auto-
analyses. Per player: **Black**, **White**, **None** (three-state, independent per colour). This
lets the user set up engine-vs-human, engine-vs-engine, or analysis-only.

## Resolved decisions (with user, 2026-08-30)

- **Vocabulary / home.** Rename the existing menu-bar **Analysis** submenu to **Engine plays**,
  containing a radio group: **Black / White / Off**. No "Both" (engine-vs-engine out of scope).
- **Semantics.** "Engine plays <side>" = **auto-move** (engine produces the move and the app plays
  it when it is that side's turn). `Off` (default) = no automatic play.
- **Toolbar.** The existing `▶ Analyze` / `■ Stop` one-shot actions are kept, untouched.
- **Persistence.** New `MatchConfig` struct in `src/model/config.h`
  (`EnginePlaysSide enginePlays = Off`), threaded through `GameState` and
  `src/model/settings_storage.cpp`.

Execution guidance: [docs/instruction/UI-06-analysis-menu-duplicate-repurpose-to-player-assignment.md](../instruction/UI-06-analysis-menu-duplicate-repurpose-to-player-assignment.md)

## Acceptance criteria

- No menu entry duplicates a toolbar action without adding value.
- User can set each side to Human / Engine (or equivalent) and the app behaves accordingly.
- Wording agreed with the user.

## Related

- ENG-01 (engine lifecycle/state honesty), existing **Players** menu.
