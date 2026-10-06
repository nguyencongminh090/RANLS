# ARCH-05 — Extract GTK-free GameFileService from MainWindow

**Status:** 🔲 OPEN (Backlog)
**Area:** `src/main_window.cpp` (`onSaveGame`, `onLoadGame`), new `src/engine/` or `src/model/`-adjacent GTK-free service (see Scope 1)
**Priority:** P3
**Source:** `features/extract-mainwindow-orchestration/planning.md` Q5 — resolved 2026-10-06; split from ARCH-02
**Design:** [features/extract-mainwindow-orchestration/](../../features/extract-mainwindow-orchestration/planning.md) — RESOLVED 2026-10-06
**Depends on / relates to:** ARCH-02 (relates; independent code, do after to avoid `main_window.cpp` conflicts)

## Problem

`onSaveGame` builds `GraphMeta` (including the RDB-03 engine entry from `EngineConfig`), converts the tree to a `GameGraph`, defaults the extension and looks up a writer; `onLoadGame` picks a reader and applies the graph — all inside GTK dialog callbacks, so none of it is unit-testable without a display.

## Scope (in order)

1. Place a `GameFileService` (name from the design; layer decided in the instruction file — it must depend only on `model/` and `model/rdb/`, never gtk, and obey rule 8).
2. Move only the GTK-free parts: meta + graph building, default `.rdb` extension, writer lookup, and load via `archiveReaderFor` + `applyGameGraphToState`, returning ok/error text.
3. `MainWindow` keeps `Gtk::FileDialog`, filters, `showErrorDialog`, `confirmDiscardGame` and the `controller_.sendConfig()` after a successful load.
4. Unit-test save (engine entry present/absent, no extension → `.rdb`, non-`.rdb` target → error) and load (bad file → error, success applies graph) without GTK.

## Scope boundary

- No file-format, dialog, filter or message-text change.
- Do not touch the RDB codec or `GameIO`.

## Reasoning

Only the decision/translation logic moves; dialogs cannot. Rejected: skipping it (≈60 lines but the RDB-03 meta logic has no direct test today).

## Knowledge

- `features/extract-mainwindow-orchestration/planning.md`
- `docs/todo/RDB-02-wire-rdb-into-save-open.md`, `docs/todo/RDB-03-persist-restore-node-analysis.md`
- `docs/knowledge/pattern-facade.md`

## Acceptance criteria

- `onSaveGame`/`onLoadGame` shrink to dialog glue; no gtk include in the new service.
- New unit tests pass without a display; existing RDB tests unchanged and green.
- Dialog messages byte-identical.
