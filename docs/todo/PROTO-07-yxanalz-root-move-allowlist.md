# PROTO-07 — native `!yxAnalz` console command: analyze an explicit root-move allow-list

**Status:** 🔲 OPEN (Backlog)
**Area:** `src/command/command_dispatcher.{cpp,h}`, `src/engine/engine_controller.{cpp,h}`, `src/engine/gomocup_protocol.{cpp,h}` (send helper only), reuses `parseMovesText` + the analyze/`SearchIntent` pipeline
**Priority:** P2
**Source:** User request 2026-09-09 — port Rapfi_V2's `YXANALZ` extension (`Rapfi_V2/rapfi/Rapfi/docs/rules/YXANALZ-user-root-candidates.md`) to YixinBoard. Feasibility + sequence + code-mapping discussion held in this session (see "Design" below).
**Design:** No `features/yxanalz/` folder — worked through inline this session (user opted to file the task directly). Route decision below is **provisional, resolve with the user before implementing.**
**Depends on / relates to:** PROTO-03 (`.ptc` extension — established this is *not* the right vehicle: no `group="analysis"`, single-line `on_reply` only, numeric-coord-only `repeat(coord)`), PROTO-05 / PROTO-06 (the `INFO PV` / `REALTIME` render pipeline this reuses), ANLZ-06 (`SearchIntent` gate on the search-completion coordinate — the pattern this must follow), CONS-01/02 (console command registry this registers into)

## Problem

Rapfi's `YXANALZ <coord> <coord> … DONE` replaces the engine's root-move generation with an
**explicit client-supplied allow-list** of depth-0 root moves (the inverse of `YXBLOCK`). Each
listed move is searched as its own PV line, reported through the normal multi-PV output, and the
best one is returned as a bare `x,y` line and played on the engine's internal board. It is a
**full search**: it takes time, honours the configured search limits, and is interruptible with
`STOP`/`YXSTOP`.

YixinBoard cannot currently drive it. The `.ptc` extension mechanism (PROTO-03) can express the
*send* (`send.open`/`repeat`/`close` block) but not the rest:

1. **No `group = "analysis"`** — an extension command never transitions `EngineState`, shows no
   "thinking" state, and Stop cannot cancel it (`protocol_extension.cpp:942` hard-rejects the
   group; deferred whole in PROTO-03 Q9).
2. **`on_reply` is strictly one-line-in / one-action-out** (PROTO-03 Q8) — YXANALZ output is a
   multi-line stream (`INFO PV … INFO PV DONE` blocks, `MESSAGE (k)` lines, summary, per depth).
3. **`GomocupProtocol::parseLine` consumes every useful line before the extension runs** — every
   `MESSAGE`, `INFO`, and valid bare coordinate returns early (`gomocup_protocol.cpp:389-421`);
   only `ERROR …` and genuinely-unclaimed lines ever reach `tryExtensionReply` (`:426`). A `.ptc`
   `on_reply` for the PV data or the best move can never fire.
4. **`.ptc` `repeat(coord)` is numeric-only** — `isCoordText` (`protocol_extension.cpp:61`) and
   `generateSend` (`:1131-1146`) parse `x,y` integers; the TU is deliberately free of `GameState`
   / board size, so it cannot do alphabetic (`h3`) coords, which need the `boardSize - num` row
   flip that `CommandDispatcher::parseMovesText` (`command_dispatcher.cpp:720`) already implements.

The good news: YXANALZ emits the **same wire lines** PROTO-05/06 already parse and render
(`parseInfo` / `parseRealtimePV` / `onPVDone` → multi-PV panel + per-cell board overlay). So the
rendering mostly exists — the gap is a *native command + search-state coordination* gap, not a
parsing gap.

## Provisional route (confirm before implementing)

**Route A — first-class console command** (recommended in-session): a native `!yxAnalz` in
`CommandDispatcher` that parses its args with the existing `parseMovesText` (alphabetic **and**
numeric coords), hands the `std::vector<Coord>` to a new `EngineController::analyzeMoves(list)`
which sends the `YXANALZ … DONE` block **and** sets `EngineState` / `SearchIntent` the way
`analyze()` does, reuses the existing `INFO`/`REALTIME` rendering, and handles the trailing
completion coordinate via the ANLZ-06 `SearchIntent` gate (search-completion only, not a played
move — or a deliberate "engine advanced its board" if the user wants that; open question).

Route B (lift PROTO-03 Q8 + Q9 to support `group = "analysis"` `.ptc` commands) is explicitly
**not** chosen — PROTO-03's planning defers it as risky surgery on the PROTO-04/ANLZ-06 state
machine, and this input would still need C++ changes for both display and alphabetic input.

## Open design questions (resolve with the user first)

- **Console syntax.** `!yxAnalz h3 h2 h9` (space-separated, like `!pos`'s move text) vs a
  `done`-terminated session (`PosSession`-style) for long lists. Recommend space-separated inline,
  no session.
- **Coordinate orientation.** `parseMovesText` yields display-orientation `Coord{x,y}`; the wire
  wants the engine's `x,y` via `coordToEngine()` (which already honours Rapfi's
  `coord_conversion_mode`). Confirm `coordToEngine` is the single conversion point — no
  axis special-casing in the new path (same rule PROTO-06 followed).
- **Position sent.** YXANALZ analyses "whatever position is current" — the new command must send
  `YXBOARD <currentPath> DONE` first (same block as `generateAnalyzeRequest`), then
  `YXANALZ … DONE`. Confirm it uses `GameState::currentPath()` and does not mutate game state.
- **Completion coordinate.** Discard as search-completion-only (safest, matches ANLZ-06 for
  `analyze()`), or optionally play it on YixinBoard's board to mirror the engine's internal board
  (Rapfi plays it internally). Recommend discard for v1; note the divergence.
- **`SearchIntent`.** Does this need a new `SearchIntent::AnalysisMoves` value, or does
  `SearchIntent::Analysis` suffice? (The completion-coord handling is identical to `analyze()`, so
  `Analysis` likely suffices.)
- **Multi-PV / whitelist lifetime.** `YXANALZ`'s `rootMovesWhitelist` is one-shot on the engine
  side (cleared by the next `TURN`/`YXNBEST`/`BOARD`). YixinBoard must not assume it persists — a
  later `analyze()` or engine move request works unchanged. Confirm no client-side state needed.
- **Validation echo.** The engine emits `ERROR …` per rejected coord and `ERROR No valid analyze
  move` when nothing survives. These already reach `signal_log` as `Error` — confirm that surfacing
  is enough (no special toast/inline-banner handling required).
- **Interaction with continuous Analyze Mode (ANLZ-01/05).** `!yxAnalz` is a one-shot request;
  confirm it does not fight the analyze-mode restart loop (likely needs the same
  `stopAnalysis()`-before-send discipline).

## Scope (in order)

1. Resolve the open design questions above with the user; record the resolutions in this file
   (fold-in-place, like `features/*/planning.md`).
2. `EngineController::analyzeMoves(const std::vector<Coord>& moves)` — send `YXBOARD <currentPath>
   DONE` + `YXANALZ <moves> DONE` (new `GomocupProtocol::generateAnalyzeMovesRequest` helper,
   reusing `coordToEngine`), set `EngineState` + `SearchIntent` exactly as `analyze()` does.
3. Completion-coordinate handling through the existing `SearchIntent` gate (ANLZ-06 pattern) —
   no stray move played (per the resolved question).
4. `CommandDispatcher`: register `!yxAnalz` (group `analysis` or `engine` — decide), parse args
   with `parseMovesText`, reject empty / all-invalid lists with a clear console error, forward to
   `analyzeMoves`.
5. Confirm the multi-PV panel + PROTO-06 board overlay render the `YXANALZ` stream unchanged
   (they should — same wire lines); fix only genuine gating bugs (e.g. overlay drawn only while
   `isAnalyzing()`).
6. Regression tests (see Scope boundary for the harness): `generateAnalyzeMovesRequest` wire
   output (alphabetic + numeric in, correct `x,y` out, `YXBOARD` block precedes `YXANALZ`);
   `analyzeMoves` sets the search state; inbound completion coordinate does **not** emit
   `signal_engine_move` under the analysis intent; empty / all-invalid arg list rejected at the
   console layer.

## Acceptance criteria

- `!yxAnalz h3 h2 h9` (and the numeric equivalent) on a running engine sends a `YXBOARD … DONE`
  block for the current position followed by `YXANALZ 2,12 … DONE` (engine orientation), and the
  engine's per-move PV stream renders in the multi-PV panel + board overlay like a normal analyze.
- While the `YXANALZ` search runs, YixinBoard shows its normal "analyzing" state and Stop / `!stop`
  cancels it (prints the current best move, like any search).
- The trailing best-move coordinate is treated as search-completion only — no stray stone is
  placed on YixinBoard's board (matches ANLZ-06 for `analyze()`).
- An empty arg list, or one where every coord is off-board / occupied, is rejected at the console
  with a clear message and **nothing is sent to the engine**.
- A subsequent `!analyze` / engine move request behaves exactly as before (no leaked whitelist
  state on the client).
- All new tests pass; `PROTO-01` parsing order, `PROTO-05/06` rendering, and `ANLZ-05/06/07`
  analyze-mode behaviour are not regressed.

## Scope boundary

- Do **not** implement this as a `.ptc` extension command (PROTO-03) — see Problem §1-4.
- Do **not** add `group = "analysis"` support to `protocol_extension.{h,cpp}` (that is the deferred
  PROTO-03 Q9 follow-up, out of scope here).
- Do **not** change `GomocupProtocol::parseLine`'s built-in-first ordering or `PROTO-01` bounds
  checks; do not touch `parseInfo` / `parseMessage` / `parseRealtimePV` / `onPVDone` parsing.
- Do **not** change the `YXNBEST` request path used by `generateAnalyzeRequest` / `analyze()`.
- Do **not** add a UI affordance (button, board context menu) — console-only, like every other
  extension/diagnostic command.
- Do **not** touch `parseMovesText`'s existing behaviour (it is shared with `!pos`); only *call*
  it.
- If the test harness cannot exercise a live search completion, state that explicitly and cover
  what the fake-engine-line tier can (per `CLAUDE.md` bug-fix workflow).
