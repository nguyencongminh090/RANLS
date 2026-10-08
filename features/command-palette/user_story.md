# Command Palette (Ctrl+K) — user stories

Scope: a global, keyboard-first search box that finds **anything the app can do** — menu actions,
settings, and Engine Log `!` commands — from a typed phrase in English or Vietnamese. Classical NLP
only (no neural net / transformer). Not in scope: translating the UI itself (i18n of labels),
searching game content (moves, comments), argument-level completion (CONS-01 Q6 stays closed).

See also: [planning.md](planning.md) · [diagram/flow.md](diagram/flow.md). Builds on, does not replace,
[console-autocomplete](../console-autocomplete/user_story.md) (Engine Log entry only).

## Actors

- **User** — knows what they want ("đổi cỡ bàn cờ", "undo", "engine settings") but not where it lives.
- **Palette** — the overlay: entry + ranked result list.
- **Registries** — sources of searchable entries: window actions/menus, settings, `CommandDispatcher`
  (built-in + `.ptc` extension `!` commands).

## Stories

- **P1** As a user, I press **Ctrl+K** anywhere in the main window and a search box appears focused;
  **Esc** closes it and returns focus where it was.
- **P2** As a user, I type a few words and see the best matches immediately (as-you-type), each with its
  group, shortcut (if any) and kind (action / setting / command).
- **P3** As a user, I press **Up/Down + Enter** (or click) to run the result: actions fire, a setting
  opens its settings page focused on that item, a `!` command is run (or inserted in the Engine Log entry
  if it needs arguments).
- **P4** As a user, typos and word-form variants still find the item ("setings", "analyse"/"analyze").
- **P5** As a Vietnamese user, I can search in Vietnamese with or without diacritics
  ("cài đặt" = "cai dat"), and by synonyms ("ván mới" → New Game, "hoàn tác" → Undo).
- **P6** As a user, I can mix languages ("mở file", "save ván").
- **P7** As a user, typing `!` first limits results to console commands; items I use often rise.
- **P8** As a user, disabled actions (e.g. Save with no game) are shown dimmed or hidden, never silently fail.
- **P9** As a user, `.ptc` extension commands appear after the engine loads and disappear when it unloads.
