# Protocol Extension v2 — planning

See [user_story.md](user_story.md). Baseline: [../protocol-extension/planning.md](../protocol-extension/planning.md)
(Q1–Q9, resolved 2026-09-06) and `src/engine/protocol_extension.{h,cpp}`.

**Status: DRAFT — questions OPEN (drafted 2026-11-04). Not authorization to implement.** Resolve with the
user, fold answers in place (same pattern as the v1 planning), then formalize into
`docs/instruction/PROTO-08-*.md`. Size **L** (changes the `.ptc` format; extends the trust boundary).

## Current gaps (from the review)

| # | Gap | Evidence |
|---|---|---|
| G1 | Only 3 sinks; display-only | `kSinks = {set_status_field, toast, log}` (`protocol_extension.cpp`) |
| G2 | No value helpers; no arithmetic (v1 Q2 dropped it on purpose) | Q2 |
| G3 | One reply line → one action | Q8 |
| G4 | An action cannot call another command | — |
| G5 | `group = "analysis"` / Stop not supported | Q9 |
| G6 | `on_reply` only fires for a command's own reply, not unsolicited engine lines | `GomocupProtocol::parseLine` |

This round targets **G1, G2, G4**. G3/G5/G6 stay deferred (they touch the PROTO-04/ANLZ-06 state machine).

## Open questions — all OPEN

| # | Question | Recommended answer | Why this one |
|---|---|---|---|
| Q10 | Which **pure functions** (return a value, no side effect) join the DSL? | Closed whitelist: `round(v, n)`, `abs(v)`, `min(a,b)`, `max(a,b)`, `fmt(template, v…)`, `concat(a,b…)`, `len(s)`. Fixed arity checked at load; result string capped (e.g. 256 B). | Covers E1 with zero state; each is a total function, trivially unit-testable. |
| Q11 | Add **arithmetic** (`+ - * /`)? | Yes, but only inside expressions over typed captures/literals; max expression depth 4; division by 0 → skip action + Debug log (never throw/UB); no assignment, no variables. | Reverses v1 Q2's "no arithmetic" **narrowly**; needs an audit entry recording the reversal and why. Alternative: functions only (`add(a,b)`) — rejected as clumsier with the same risk. |
| Q12 | New **board sinks**: names, args, lifetime? | `highlight(x, y)` and `mark(x, y, "label")`; reuse the board-marks layer (`features/board-marks/`), not a new renderer. Cleared on next move/new game. v1's deferred `highlight_cell` follow-up is superseded. | Marks already exist (UI-16/17/18, UX-07); a sink should feed them, not duplicate rendering. Needs the model/UI split to respect `ui → command/engine → model`. |
| Q13 | **Chaining**: `run("cmd", args…)`? | Defer to a later phase. If included: only commands from the *same* `.ptc` (never built-ins), recursion depth ≤ 2, ≤ 4 `run` per action, and the call is queued (not synchronous). | Highest risk (loops, command storms). Splitting it out lets Q10–Q12 ship first. |
| Q14 | **Versioning**: how does a file declare it needs v2 features? | `ptc_version = 2` for files using v2 functions/sinks; the loader still accepts `1`; a v1 file using a v2 name is a load error. Unknown version stays fail-closed. | Keeps old files working and gives authors a clear error instead of silent no-ops. |
| Q15 | **Limits** for the new surface? | Keep Q5 numbers; add: max expression depth 4, max string result 256 B, max `highlight/mark` calls per action 16 (shared with the sink cap). | Same "round, defensible, revisit only if a real `.ptc` hits it" stance as Q5. |
| Q16 | **Type system**: do functions accept/return `int`/`float`/`string` implicitly? | No implicit coercion except `int→float`; `fmt` is the only function that stringifies. Type errors are **load-time** where statically known. | Avoids surprising runtime behavior in a semi-trusted file. |
| Q17 | **Phasing**? | PROTO-08a: Q10+Q11+Q14+Q15+Q16 (pure functions, arithmetic, versioning). PROTO-08b: Q12 (board sinks). PROTO-08c: Q13 (`run`), only after 08a/08b ship. | Each phase is size-M-reviewable and independently shippable. |
| Q18 | **Docs/examples**: where do authors learn it? | Update `features/protocol-extension/examples/` with a v2 example, add a `.ptc` reference to `docs/` (Diataxis *reference*), and surface function names in `!help` under `[extension]`. | Functions nobody can discover are not "basic functions for users". |

## Implementation sequencing (draft — do not start until Q10–Q18 are resolved)

1. Extend the expression parser (arithmetic, calls) + type check at load; unit tests incl. malformed input.
2. Add the pure-function table (`map<string, fn>` like the sink table) + per-function arity/type tests.
3. `ptc_version = 2` gating + clear load errors.
4. (08b) Board sinks → board-marks layer; UI wiring behind the existing `signal_custom_action`.
5. (08c) `run()` with the recursion/queue bounds, only if still wanted.
6. Example `.ptc`, reference doc, `!help` entries.

Testing: extend `tests/test_proto03_protocol_extension.cpp` style (no display, no subprocess); one case per
function, per limit, per rejection path.

## Follow-ups (explicitly deferred)

G3 (multi-line replies), G5 (`analysis` group / Stop), G6 (unsolicited-line patterns), hot-reload,
UI-triggered custom commands (button/context menu).
