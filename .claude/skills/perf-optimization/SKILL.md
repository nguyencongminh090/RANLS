---
name: perf-optimization
description: Performance analysis for YixinBoard — main-thread blocking in engine lifecycle, redraw/rebuild cost in custom GTK widgets, engine subprocess I/O. Use when asked to optimize performance, investigate UI freezes/stutter/lag, or before adding code to a hot path (draw callbacks, per-move update handlers, engine line parsing). Always profile/measure before changing anything — this skill's job is finding where to look, not assuming a fix.
---

# Performance — YixinBoard

Two performance domains matter here: **main-thread responsiveness** (GTK apps have one UI thread;
anything blocking it freezes the whole window) and **per-update rendering cost** (board/tree/PV
views redraw or rebuild on every engine `INFO` line, which can arrive fast).

## Known main-thread blocking points — verify before assuming these are still live

These were found by reading the code, not by profiling — confirm against current behavior before
reporting them as active bugs, per this repo's audit convention (`/CLAUDE.md` "Audit").

- **`EngineController::stopEngine()`** (`src/engine/engine_controller.cpp:81-94`) calls
  `g_usleep(500000)` — a flat 500ms sleep — after sending `YXSAVEDATABASE`/`END`, "to wait for the
  engine to flush its buffers." If this runs on the GTK main thread (it's reachable from
  `MainWindow::onQuit` and the settings-apply path that restarts the engine — see
  `main_window.cpp:468-472`, `510-528`), the whole window freezes for 500ms on quit or on changing
  the engine path in settings.
- **`EngineProcess::stop()`** (`src/engine/engine_process.cpp:45-72`) runs a manual busy-wait loop:
  `g_main_context_iteration(NULL, FALSE)` + `g_usleep(20000)` for up to 2000ms while waiting for the
  subprocess to exit. Same main-thread-freeze risk, same call sites, and it's a hand-rolled
  reimplementation of what an async subprocess-wait would do — a candidate for using
  `Gio::Subprocess`'s own async wait instead of polling.
- Check whether `stopEngine`/`EngineProcess::stop` are ever invoked from a background thread before
  reporting this as a real freeze — if some caller already dispatches it off the main thread, the
  finding doesn't apply there.

## What's already async and NOT a blocking concern

`EngineProcess::readStdout()`/`readStderr()` (`engine_process.cpp:93-162`) use
`Gio::DataInputStream::read_line_async` with a proper GLib async callback chain — engine output
reading does not block the main thread. Don't flag this path without new evidence.

## Redraw/rebuild cost in custom widgets

- **`TreeNodeView::update()`** (`src/ui/tree_node_view.cpp`) and **`TreeExplorer::update()`**
  (`src/ui/tree_explorer.cpp`) both rebuild their entire visual representation from the model on
  every call — full tree walk (`layoutTree`) or full `Gio::ListStore` repopulation. Fine at typical
  analysis-tree sizes; worth measuring specifically if a user reports lag with a very deep/wide tree
  (long correspondence-style analysis sessions), not assumed a priori.
- **`BoardRenderer`** draws via Cairo on a `Gtk::DrawingArea` — GTK4 repaints on `queue_draw()`,
  which redraws the whole widget by default. If board redraws become a bottleneck, the fix is
  narrowing what triggers `queue_draw()` (e.g. don't redraw the whole board for a hover-only PV
  overlay change) rather than optimizing the Cairo calls themselves — check call sites of
  `queue_draw()` first.
- Engine `INFO`/`MESSAGE` lines can arrive at high frequency during deep search — `GameState`'s
  `signal_engine_analysis`/`signal_analysis` handlers (wired in `EngineController::connectProtocolSignals`)
  fan out to potentially several UI widgets per line. If update rate is the complaint, check whether
  updates are naturally throttled by GTK's own frame clock (redraw coalescing) or whether
  layout/rebuild work happens synchronously per signal emission — the latter is the actual cost, not
  the signal emission itself.

## How to approach a perf task here

1. **Reproduce and measure first** — a stated "feels slow" needs a concrete case (board size, tree
   depth, multiPV setting, engine chosen) before changing code speculatively.
2. **Main-thread blocking beats micro-optimization** — a single blocking `g_usleep`/busy-wait call
   causes a worse user-visible symptom (frozen window) than an inefficient-but-async redraw. Triage
   in that order.
3. **File findings, don't fix silently** — per `/CLAUDE.md`, a perf issue found while working on
   something else goes in `TODO.md` Backlog (or `docs/fix-log.md` if it's an active bug fix), not a
   drive-by change bundled into an unrelated task.
