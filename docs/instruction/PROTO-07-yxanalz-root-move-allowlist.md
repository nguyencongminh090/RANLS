# PROTO-07 — yxanalz-root-move-allowlist

## Approach

Feature, **not** a `.ptc` command. Resolve the open design questions in
`docs/todo/PROTO-07-*.md` with the user *first* (fold answers in place). Then, Route A:

- New `EngineController::analyzeMoves(std::vector<Coord>)` mirrors `analyze()` — same
  `EngineState` transition, same `SearchIntent`, same `stopAnalysis()`-before-send guard.
- New `GomocupProtocol::generateAnalyzeMovesRequest(path, moves)` returns the line vector
  `YXBOARD` / `<path coords>,<color>` / `DONE` / `YXANALZ` / `<move coords>` / `DONE` — reuse
  `coordToEngine()` for **every** coordinate (path and move list); do not hand-roll `x,y`.
- `CommandDispatcher` registers `!yxAnalz`, parses args via the existing `parseMovesText(blob,
  boardSize())`, rejects empty / all-off-board lists at the console (nothing sent), forwards the
  `vector<Coord>` to `analyzeMoves`.
- Completion coordinate: handled by the ANLZ-06 `SearchIntent` gate — under an analysis intent the
  inbound coord is search-completion only, `signal_engine_move` is **not** emitted, no stone
  placed.
- Rendering: none new. YXANALZ emits the same `INFO PV` / `MESSAGE REALTIME` lines PROTO-05/06
  already parse. Verify, fix only real gating bugs.

## Pitfalls

- **`parseLine` ordering (PROTO-01).** Built-in parsing claims every `MESSAGE` / `INFO` / valid
  bare-coord line and returns before `tryExtensionReply`. This is why a `.ptc` `on_reply` can't
  see the YXANALZ result — and why the native path must go through `signal_move` +
  `SearchIntent`, not a new parse hook. Do not reorder `parseLine`.
- **Coordinate orientation.** `parseMovesText` yields **display** orientation (`y = boardSize -
  num`, row 1 at bottom). The wire wants the **engine's** `x,y`. `coordToEngine()` is the single
  conversion point and already honours Rapfi's `coord_conversion_mode` — route both the `YXBOARD`
  path and the `YXANALZ` move list through it, exactly as `generateAnalyzeRequest` /
  `generateMoveRequest` do. No axis special-casing (the rule PROTO-06 followed with
  `parseEngineCoord`).
- **`parseMovesText` is shared with `!pos`.** Only call it. Its "try both `i`-skip mappings" and
  1-based-from-bottom row logic is deliberate — do not fork or "fix" it here.
- **`protocol_extension.{h,cpp}` is off-limits.** Its `isCoordText` / `generateSend` are
  numeric-only *by design* (the TU has no `GameState` / board size). Adding alphabetic support
  there is the deferred PROTO-03 Q9 work, not this task.
- **Whitelist lifetime.** `rootMovesWhitelist` is one-shot engine-side; the next
  `TURN`/`YXNBEST`/`BOARD` clears it. YixinBoard must hold no mirror state — a following
  `!analyze` or engine move must work unchanged. Don't add a "last analyze-moves list" member.
- **Continuous Analyze Mode (ANLZ-01/05).** `!yxAnalz` is one-shot. If analyze-mode's restart loop
  is active it will re-issue a plain `analyze()` right after — needs the same
  `stopAnalysis()`-first discipline and likely a bail in `maybeStart…`/`scheduleAnalyzeModeRestart`
  while a `yxAnalz` search is the active intent. Check against ANLZ-05's `MainWindow` guards.
- **Empty / all-invalid list.** The engine answers `ERROR No valid analyze move` and does not
  search — but YixinBoard should reject *before* sending (no engine round-trip for an obviously
  empty list). Still surface a real engine `ERROR` line if one slips through.
- **STATE-01 / UI-04 position reset.** A `YXANALZ` run changes no position, but if the user
  navigates mid-search the existing reset path must still disarm the overlay/PV — verify it keys
  off `isAnalyzing()` / the search-state the new path sets, not off `analyze()` specifically.

## Verification before done

- `generateAnalyzeMovesRequest` unit test: alphabetic + numeric input → correct engine `x,y`,
  `YXBOARD` block precedes `YXANALZ` block, both `DONE`-terminated, color alternates on the path.
- `analyzeMoves` sets `EngineState` + `SearchIntent` like `analyze()` (assert via the fake engine).
- Inbound completion coordinate under the analysis intent does **not** emit `signal_engine_move`
  (feed an *inbound* coord line — the gap ANLZ-06's test closed).
- Console: empty list and all-off-board list both rejected, `sendRawCommand` / `sendLine` never
  called.
- Regression: `ctest` green; PROTO-05/06 render tests, ANLZ-05/06/07 unchanged.
- Live smoke (needs a human + real Rapfi_V2 build): `!yxAnalz` renders per-move PVs in the panel
  and tags the candidate cells; `!stop` cancels; no stray stone appears.

## Boundaries

- No `.ptc` / `protocol_extension.*` changes. No `IEngineProtocol` method additions.
- No `parseLine` reordering, no `PROTO-01` bounds-check changes, no `parseInfo`/`parseMessage`/
  `parseRealtimePV`/`onPVDone` edits.
- No `YXNBEST` request change; `generateAnalyzeRequest` / `analyze()` untouched.
- No new UI affordance — console only.
- No change to `parseMovesText` behaviour.
