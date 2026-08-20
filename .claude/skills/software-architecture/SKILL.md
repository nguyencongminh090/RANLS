---
name: software-architecture
description: Module layering and dependency direction for YixinBoard — model/engine/ui/command separation, signal-based decoupling, protocol abstraction. Use when deciding where new code should live, adding a new module/class, reviewing whether a change respects layer boundaries, or planning a larger feature that touches multiple layers. Not for GTK widget implementation details (see gtk-ui-design) or domain data shape (see data-architecture).
---

# Software architecture — YixinBoard

Four layers, one dependency direction, decoupled by `sigc::signal`. Read this before adding a class
or deciding which folder new code belongs in.

## The layers and the dependency rule

```
src/model/    — domain state (GameState, BoardState, MoveHistory, VariationTree, config).
              — NO gtkmm includes, NO engine subprocess knowledge. Pure data + sigc::signal.
src/engine/   — engine subprocess (EngineProcess) + protocol translation (IEngineProtocol,
              — GomocupProtocol) + orchestration (EngineController). Depends on model/ (reads/
              — writes GameState) but NOT on ui/.
src/command/  — text-command bus (CommandDispatcher) for the console/scripting surface. Depends on
              — model/ + engine/ via CommandContext, not on ui/ internals (only UI *hooks* — print/
              — clearConsole callbacks — cross into command/, never a UI widget type).
src/ui/       — GTK4 widgets. Depends on model/ + engine/ + command/. Nothing depends on ui/.
```

**Dependency direction is one-way: `ui` → `command`/`engine` → `model`.** `model/` must never
`#include <gtkmm.h>` or know about `EngineProcess`. If you're adding a method to `GameState` and find
yourself wanting to reference a GTK type or send an engine command directly from it, that logic
belongs in `ui/` or `engine/` instead, driven by a signal `GameState` emits.

## Decoupling mechanism: `GameState` as a signal-emitting hub

`GameState` (`src/model/game_state.h`) is the single source of truth. It never calls into `ui/` or
`engine/` directly — every state change emits a `sigc::signal<...>` (`signal_board_changed`,
`signal_tree_updated`, `signal_analyzing_state`, etc.), and the composition root
(`main_window.cpp`/`application.cpp`) wires those signals to whichever widgets or the engine
controller need to react. **A new feature that needs "when X happens, do Y" almost always means: add
or reuse a `GameState` signal, connect it in the composition root — not a direct call from the place
X happens to the place Y should happen.**

## `engine/`: protocol abstraction is deliberate, don't bypass it

`EngineController` talks to the engine only through `IEngineProtocol` (`i_engine_protocol.h`), with
`GomocupProtocol` as the concrete implementation (`gomocup_protocol.cpp`, per the `README.md`
YixinBoard/Rapfi protocol extension of Gomocup — see `docs/protocol.md` in the sibling `Rapfi` repo
for the wire format this class implements). This exists specifically so a different engine protocol
could be swapped in without touching `EngineController`'s lifecycle logic (`startEngine`,
`stopEngine`, `analyze`, etc.) or any UI code. **New protocol-specific parsing/generation goes in
`GomocupProtocol` (or a new `IEngineProtocol` implementation), never inline in `EngineController` or
`EngineProcess`.** `EngineProcess` itself knows nothing about the protocol — it's a raw line-based
subprocess I/O wrapper; keep it that way.

## `command/`: the text-command bus pattern

`CommandDispatcher` (`command_dispatcher.h`) is a registry: `registerCommand(spec, handler)` maps a
command name to a `std::function<void(const Command&)>`, with `CommandContext` bundling the
references a handler needs (`GameState&`, `EngineProcess&`, `EngineController&`, plus UI print
callbacks). **A new console/scripting command is a new `registerCommand` call in
`registerBuiltins()`, not a new special-cased branch elsewhere.** Handlers should stay thin — reuse
`GameState`/`EngineController` methods rather than duplicating logic that also needs to work from the
GUI.

## Where new code goes — decision guide

| Adding... | Goes in | Because |
|---|---|---|
| A new piece of persistent game state | `model/game_state.h` + a signal | Single source of truth |
| A new derived/computed view of existing state | `model/` (a method) or `ui/` (a view-model, see `board_view_model.h`) depending on whether other consumers need it | Don't duplicate derivation logic per-widget |
| A new engine command or output parser | `engine/gomocup_protocol.cpp` (or a new `IEngineProtocol` impl) | Protocol abstraction boundary |
| A new GTK screen/widget | `ui/`, constructed from `model/`+`engine/` state, signals wired in `main_window.cpp` | UI depends inward, nothing depends on UI |
| A new console command | `command/command_dispatcher.cpp` `registerBuiltins()` | Single command registry |
| Config that's user-editable | `model/config.h` (`EngineConfig`/`ViewConfig`) + `SettingsStorage` persistence + `SettingsDialog` UI | Existing three-part pattern (model/persistence/UI) |

## Scope boundary

This skill is layering/dependency-direction only. Widget-level GTK4 implementation choices are
`gtk-ui-design`; the shape of `VariationTree`/`BoardState` themselves is `data-architecture`.
