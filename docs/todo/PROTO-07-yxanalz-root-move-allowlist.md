# PROTO-07 — native `!yxAnalz` console command: analyze an explicit root-move allow-list

**Status:** 🔲 OPEN (Backlog) — design resolved 2026-09-09, ready to pull into a sprint
**Area:** `src/command/command_dispatcher.{cpp,h}`, `src/engine/engine_controller.{cpp,h}`, `src/engine/gomocup_protocol.{cpp,h}` (send helper only), reuses `parseMovesText` + the analyze/`SearchIntent` pipeline
**Priority:** P2
**Source:** User request 2026-09-09 — port Rapfi_V2's `YXANALZ` extension (`Rapfi_V2/rapfi/Rapfi/docs/rules/YXANALZ-user-root-candidates.md`) to YixinBoard. Feasibility + sequence + code-mapping discussion held in this session (see "Design" below).
**Design:** No `features/yxanalz/` folder — worked through inline this session; the 8 open questions were resolved with the user 2026-09-09 (see "Resolved design" below). Route A confirmed.
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

## Route (confirmed 2026-09-09)

**Route A — first-class console command.** Native `!yxAnalz` in `CommandDispatcher` parses its
args with the existing `parseMovesText` (alphabetic **and** numeric coords), hands the
`std::vector<Coord>` to a new `EngineController::analyzeMoves(list)` which sends the `YXBOARD …
DONE` + `YXANALZ … DONE` blocks **and** sets `EngineState` / `SearchIntent::Analysis` the way
`analyze()` does, reuses the existing `INFO`/`REALTIME` rendering, and treats the trailing
completion coordinate as search-completion only via the ANLZ-06 `SearchIntent` gate.

Route B (lift PROTO-03 Q8 + Q9 to support `group = "analysis"` `.ptc` commands) is explicitly
**not** chosen — PROTO-03's planning defers it as risky surgery on the PROTO-04/ANLZ-06 state
machine, and this input would still need C++ changes for both display and alphabetic input.

## Resolved design (2026-09-09, with the user)

1. **Console syntax — inline, space-separated.** `!yxAnalz h3 h2 h9` (and numeric `7,7`), one
   line, parsed by the shared `CommandDispatcher::parseMovesText(blob, boardSize())`. **No
   `done`-terminated session.**
2. **Coordinate orientation — `coordToEngine()` is the single conversion point.** `parseMovesText`
   yields display-orientation `Coord{x,y}`; every coordinate that goes on the wire (the `YXBOARD`
   path *and* the `YXANALZ` move list) is converted through `coordToEngine()`, which already
   honours Rapfi's `coord_conversion_mode`. No axis special-casing (same rule PROTO-06 followed).
3. **Position sent — `YXBOARD <currentPath> DONE` then `YXANALZ <moves> DONE`.** From
   `GameState::currentPath()`; the command mutates no game state. Same block shape as
   `generateAnalyzeRequest`.
4. **Completion coordinate — discarded (analysis only).** The trailing best-move `x,y` is
   search-completion only; **no stone is placed** on YixinBoard's board. The per-move PVs stay on
   screen for the user to act on. (Matches ANLZ-06's handling of `analyze()`; YixinBoard's board
   is the user's, distinct from the engine's internal board.)
5. **`SearchIntent` — reuse `SearchIntent::Analysis`.** Completion-coord handling is identical to
   `analyze()`, so no new intent value; the existing ANLZ-06 gate already does the right thing.
6. **Whitelist lifetime — no client-side state.** `YXANALZ`'s `rootMovesWhitelist` is one-shot
   engine-side (cleared by the next `TURN`/`YXNBEST`/`BOARD`). YixinBoard holds no mirror; a
   following `!analyze` / engine move request behaves exactly as before.
7. **Validation echo — engine `ERROR` lines via `signal_log` are sufficient.** Per-coord
   `ERROR Coord is not valid…` and `ERROR No valid analyze move` already reach the console log as
   `Error`. No special toast/inline-banner. The client additionally rejects an empty / all-invalid
   list **before sending** (no engine round-trip).
8. **Analyze Mode — `!yxAnalz` is refused while Analyze Mode is ON.** If
   `gameState_.viewConfig().analyzeMode`, the console prints a clear error ("Turn off Analyze Mode
   first") and nothing is sent. No suspend/resume interaction with the ANLZ-01/05 restart loop is
   built.

Command group: registered under `analysis` in `!help` (it is a real search).

## Scope (in order)

1. `GomocupProtocol::generateAnalyzeMovesRequest(path, moves)` — returns the line vector
   `YXBOARD` / `<path coord>,<color>` … / `DONE` / `YXANALZ` / `<move coord>` … / `DONE`, every
   coordinate via `coordToEngine()`. Colour alternates on the path like `generateAnalyzeRequest`.
2. `EngineController::analyzeMoves(const std::vector<Coord>& moves)` — send the block via the new
   helper, set `EngineState` + `SearchIntent::Analysis` exactly as `analyze()` does.
   `stopAnalysis()` first if a search is already running (the `analyze()` early-return rule).
3. Completion-coordinate handling: no new code expected — verify the existing `SearchIntent`
   gate treats the inbound coord under `Analysis` intent as completion-only (ANLZ-06).
4. `CommandDispatcher`: register `!yxAnalz` (group `analysis`); parse args with `parseMovesText`;
   reject (a) Analyze Mode ON, (b) empty list, (c) all coords off-board/occupied — each with a
   clear console message and **no send**; otherwise forward the `vector<Coord>` to `analyzeMoves`.
5. Verify the multi-PV panel + PROTO-06 board overlay render the `YXANALZ` stream unchanged (same
   wire lines as `YXNBEST`); fix only genuine gating bugs (e.g. overlay keyed off `isAnalyzing()`).
6. Regression tests (see Scope boundary for the harness): `generateAnalyzeMovesRequest` wire
   output (alphabetic + numeric in → correct engine `x,y` out, `YXBOARD` block precedes `YXANALZ`,
   both `DONE`-terminated); `analyzeMoves` sets `EngineState`/`SearchIntent`; **inbound**
   completion coordinate does not emit `signal_engine_move` under the analysis intent; console
   layer rejects Analyze-Mode-ON / empty / all-invalid without sending.

## Acceptance criteria

- `!yxAnalz h3 h2 h9` (and the numeric equivalent) on a running engine, Analyze Mode OFF, sends a
  `YXBOARD … DONE` block for the current position followed by `YXANALZ <x,y> … DONE` in engine
  orientation, and the engine's per-move PV stream renders in the multi-PV panel + board overlay
  like a normal analyze.
- While the `YXANALZ` search runs, YixinBoard shows its normal "analyzing" state and Stop / `!stop`
  cancels it (prints the current best move, like any search).
- The trailing best-move coordinate places **no** stone on YixinBoard's board.
- With Analyze Mode ON, `!yxAnalz …` prints "Turn off Analyze Mode first" (or similar) and sends
  nothing.
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
