# Open Protocol Extension — user-defined commands for Gomocup-family engines

## Overview (as of 2026-09-06, Q1–Q9 resolved)

A `.ptc` file is TOML an engine developer writes to add console-triggered commands to YixinBoard
without touching its C++ source or rebuilding. It only *extends* `GomocupProtocol` — no new
`IEngineProtocol` implementation. Every command declares:

- **`group`** — one of the console's own existing categories (`board`, `config`, `database`,
  `debug`, `engine`, `info` — see `!help`). Fixes the command as **instant**: no `EngineState`
  transition, not integrated with Stop/`pendingStopFlush_`. A `group = "analysis"` kind (real
  search) is deferred — see planning.md Q9.
- **`args`** — typed console parameters: `int`, `float`, `string`, `coord`, or a
  `repeat(...)`-terminated variadic group (console syntax: `done`-terminated, mirroring
  `!pos <moveText...>`/`PosSession`).
- **`send`** — either one wire line, or an open/repeat/close block mirroring the real
  `YXBOARD`/`BOARD ... DONE` pattern (`GomocupProtocol::generateAnalyzeRequest`/
  `generateMoveRequest`). The repeat step can iterate a `repeat(...)` arg **or** `$currentPath` —
  the live board position, host-trusted, read straight from `GameState::currentPath()` — exposing
  precomputed `i.x`/`i.y`/`i.color` per item (color alternates by index, same formula the real code
  uses); a template omits whichever of these it doesn't need.
- **`on_reply`** — one pattern (single line only, v1 — see Q8), typed named captures, and an
  `action`: a small `if/elif/else` DSL (comparisons + `and`/`or`/`not` only — no loops, no
  arithmetic, no free variables) calling one of three whitelisted sinks: `set_status_field`,
  `toast`, `log`. All reuse existing widgets; `set_status_field` additionally needs one small new
  dynamic-row addition to `EngineStatusView` since its 6 stat fields are otherwise fixed members.

Schema skeleton:

```toml
ptc_version = 1
extends = "gomocup"

[[command]]
name  = "<consoleCommandName>"        # registered as !<name>, same namespace as built-ins
group = "board"                        # board | config | database | debug | engine | info
args  = ["<name>:<int|float|string|coord|repeat(...)>", ...]

# send: single line, OR an open/repeat/close block —
send = "<line template using {argName}>"
# send.open  = "<line>"
# send.repeat_source = "<argName>"     # or "$currentPath"
# send.repeat = "<per-item line, {i.x}/{i.y}/{i.color} for $currentPath>"
# send.close = "<line>"
# send.after = "<optional trailing line, e.g. the real command after a sync block>"

[[command.on_reply]]
pattern = "<line template with {name:type} captures>"
action = """
if <expr> then
    <sink_call>(...)
elif <expr> then
    <sink_call>(...)
else
    <sink_call>(...)
end
"""
```

See [`examples/yxAnalyzeOne.ptc`](examples/yxAnalyzeOne.ptc) for a complete worked file and
[planning.md](planning.md) for the Q1–Q9 rationale behind each piece.

## Background

Discussion 2026-09-05 (see architecture report on protocol layering produced the same session):
today, adding a new engine command (e.g. Rapfi/Yixin-style `YXANALYZEONE` — analyze one
user-chosen candidate move instead of searching for best move) requires a C++ change to
`GomocupProtocol`/`IEngineProtocol` and a YixinBoard rebuild. Request: let an **engine developer**
declare new commands in a `.ptc` config file — send template, reply pattern, and how the result is
rendered — loaded by YixinBoard at runtime, layered on top of the existing Gomocup protocol. No
new protocol *implementation class*; every engine YixinBoard targets already speaks the
Gomocup/Yixin wire format (per root `CLAUDE.md`), so "new protocol" in practice means "extended
Gomocup", not a different framing.

## Actors

- **Engine developer** — writes a `.ptc` file describing extension commands for their engine build.
- **Reviewer/User** — points `EngineConfig` at an optional `.ptc` file, triggers extension commands
  via the console, sees results rendered through existing UI surfaces.
- **`GomocupProtocol`** (existing class, unchanged in kind) — loads and merges the `.ptc` command
  table with its hardcoded built-in commands; still the only `IEngineProtocol` implementation.

## User stories

1. As an engine developer, I write a `.ptc` file declaring `yxAnalyzeOne(x, y)` — a send template
   and a reply pattern — without touching YixinBoard's C++ source, and it becomes usable in the app.
2. As a reviewer, I trigger `yxAnalyzeOne` from the console; YixinBoard sends the engine's raw
   command and, when the reply line arrives, renders the result via existing UI primitives
   (status field / log / toast) exactly as declared in the `.ptc`.
3. As an engine developer, I want conditional rendering (`if`/`elif`/`else`) so one reply pattern
   can show different feedback depending on field values (e.g. a toast only when win% crosses a
   threshold) — without asking YixinBoard's maintainer to hardcode a new UI rule.
4. As a reviewer with no `.ptc` loaded, the app behaves exactly as today — no extension commands
   exist, built-in Gomocup/Yixin behavior is unchanged.

**Caveat added 2026-09-06 (Q9):** every command declares a `group`, reusing the console's own
existing category taxonomy (`board`, `config`, `database`, `debug`, `engine`, `info` — the same
groups shown in `!help`). Only these 6 are in scope for v1: an extension command is always an
*instant* command, never a real search — it does not transition `EngineState`, is not integrated
with `pendingStopFlush_`, and Stop cannot cancel it. **`yxAnalyzeOne` itself must be declared under
a non-`analysis` group for v1** (e.g. `info`) — it works as a quick request/response, but if the
engine takes a long time to answer `YXANALYZEONE`, the UI has no way to show "thinking" state for it
or let Stop interrupt it. A `group = "analysis"` kind (full search integration) is an explicit
follow-up, not part of this feature — see planning.md Q9.

## Rules

- No new `IEngineProtocol` implementation class — `.ptc` only *extends* `GomocupProtocol`.
- Built-in hardcoded parsing always takes precedence in `parseLine()`; extension patterns are tried
  only when nothing built-in matches (exact collision behavior: open, see planning.md Q3).
- Commands are triggered through the console / `CommandDispatcher` only — no new UI affordance
  (button, context menu) in this iteration (decided 2026-09-05).
- The reply-handling DSL (`if`/`elif`/`else`) has **no loops, no recursion, no free variables** —
  only fields declared/typed by the matching `pattern`, and only whitelisted sink calls. This is a
  trust-boundary control: both engine stdout *and* a shared/downloaded `.ptc` file are less-trusted
  input (PROTO-01 precedent — reject malformed input, never rely on UB-free-by-luck parsing).
- Sink whitelist for this iteration (revised 2026-09-06 — dropped `highlight_cell`/
  `clear_highlights`): `set_status_field(name, value)`, `toast(message)`, `log(message)`.
  `toast`/`log` reuse existing widgets exactly as-is (the crash-banner pattern, `EngineLogModel`).
  `set_status_field` takes a **`.ptc`-chosen, arbitrary field name** (per discussion 2026-09-06),
  which `EngineStatusView`'s current 6 label pairs (`labelDepth_`/`valueDepth_`, ... — all fixed
  members, see `src/ui/engine_status.cpp`) cannot host: it needs one small new addition — a dynamic
  row container inside `EngineStatusView` that adds a label pair the first time a given field name
  is seen and updates it thereafter. This is real new UI code, just far smaller than a board-drawing
  layer (no `BoardRenderer`/`BoardViewModel` touched, no Cairo drawing, one widget only). Board-cell
  visualization was considered separately (`highlight_cell(x, y, winrate)`, reusing
  `set_source_from_winrate()`'s winrate→HSV gradient — see `src/ui/board_renderer.cpp`) but dropped
  from v1: it would have required a new `BoardViewModel` field + a new `BoardRenderer` draw layer,
  and the concrete use case (`yxAnalyzeOne`'s win% for one move) is already fully answered by
  `set_status_field`. See planning.md's Follow-ups if a real need for on-board visualization shows
  up later.

## Hard constraints (do not touch)

- `IEngineProtocol` stays exactly as-is (pure abstract, no optional/default methods bolted on). The
  custom-command *send* path is a separate, minimal interface (working name `ICustomCommandSource`)
  that only `GomocupProtocol` implements when a `.ptc` is loaded; `EngineController` reaches it via
  `dynamic_cast`, keeping the core protocol interface undiluted (Interface Segregation).
- `EngineProcess` (raw transport) is untouched — still knows nothing about protocol semantics.
- Existing Gomocup parsing and its hardening (`onPVDone`, `parseInfo`, `parseMessage`,
  `parseDatabase`, PROTO-01 bounds checks, PROTO-02 board-size handling) is untouched by this
  feature — extension patterns are additive, evaluated after built-in parsing fails to match.

## Out of scope (separate follow-ups)

- A brand-new, non-Gomocup protocol family defined purely via `.ptc` (different framing/handshake,
  not just a command table) — deferred indefinitely; not a real near-term need since every target
  engine already speaks Gomocup/Yixin.
- UI-triggered custom commands (context menu on a board move, a generated button) — console-only
  for this iteration; may be reconsidered once the console-only path has real usage.
- Sink types beyond the 5 listed (freeform drawing, arrows, shapes) — would need a generic
  annotation-layer redesign; explicitly deferred (2026-09-05: user chose "reuse existing UI
  primitives" over ".ptc draws whatever it wants").
- Hot-reload of a `.ptc` mid-session — open, see planning.md Q6.
- `group = "analysis"` commands — real search integration with `EngineState`/Stop (Q9,
  2026-09-06) — deferred whole; v1 ships only the 6 instant-command groups.
- Live-`GameState` context references in `send` (e.g. a `$currentPath` token for board-sync
  blocks) — genuinely open, not yet designed (Q9, 2026-09-06).

## Cross-links

- [planning.md](planning.md) — open questions + implementation sequencing
- [diagram/flow.md](diagram/flow.md) — load / send / parse / render sequence
- `src/engine/i_engine_protocol.h`, `src/engine/gomocup_protocol.{h,cpp}` — existing protocol layer
  this feature extends (see the `software-architecture` skill's "protocol abstraction is
  deliberate, don't bypass it" section)
- `docs/todo/PROTO-01-parser-hardening.md` — trust-boundary precedent this feature's DSL sandboxing
  follows
