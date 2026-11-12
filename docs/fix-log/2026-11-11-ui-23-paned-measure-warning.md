# GtkPaned "Trying to measure ... for width of N, but it needs at least N+1" (2026-11-11)

**Status:** Active
**Related:** UI-23

## Prompt

Found during UI-23 development (not a shipped defect). After the engine stats were moved into a
wrapping `Gtk::FlowBox`, the app logged
`Gtk-WARNING **: Trying to measure gtkmm__GtkBox 0x… for width of 362, but it needs at least 363`,
first on every startup frame, then after each PV update while analysing.

## Root cause

Bisected step by step (baseline binary from HEAD: 0 warnings):

1. Startup warnings: caused by the FlowBox sitting inside the *horizontal* EngineStatusView box (removing
   the FlowBox → 0). Moving to a vertical layout (controls row, then the stats FlowBox) → 0 at startup.
2. The remaining warnings needed both played moves and analysis. Matching the warned pointer to logged
   widget addresses showed it was the `AnalysisPanel` itself (end child of `mainHPaned_`). A tick logger
   showed it was always allocated 598px with min 363px. Each warning followed a change in its *natural*
   width (PV text growing), so the 362 is a re-measure probe, not an allocation.
3. With a forced panel min of 380, the probe moved to 379: always min − 1. Calling
   `mainHPaned_.measure(VERTICAL, for_size = paned_min)` directly reproduced the warning (paned min
   364 = start 0 + handle 1 + end 363), and `paned_min + 1` did not.
4. With a non-zero start-child minimum (BoardView `set_size_request(200,200)`) the same probe and the
   full moves+analysis repro both gave 0.

So: GtkPaned (GTK 4.22.4) hands a height-for-width end child 1px less than its minimum when measured
at its own minimum width **and the start child's minimum is 0**. BoardView had no minimum. The baseline
never showed it because the old analysis panel was constant-size, and GTK only checks `for_size`
against the minimum for height-for-width widgets. UI-23's FlowBox made the panel height-for-width.

## Fix

`BoardView` sets `set_size_request(kBoardMinPx, kBoardMinPx)` with `kBoardMinPx = 240`
(`src/ui/board_geometry.h`): coordinate margins plus a usable grid. This is also the UX intent, since the board can
no longer be squeezed to nothing. The GTK paned internals were not patched or worked around elsewhere.

## Verification

- New `tests/test_ui23_paned_min_measure.cpp`: BoardView min size > 2×`kCoordMargin`; the board|analysis
  paned configured like `MainWindow` and measured at its exact min width logs no "Trying to measure".
  Both failed before the fix (`0 > 76`; warning at 322/323) and pass after.
- The GLib log writer capture was moved from `test_ui11_about_dialog.cpp` into the shared
  `tests/gtk_log_capture.h`, because `g_log_set_writer_func` may be installed only once per process.
- `ctest` 5/5 under Xvfb. Live runs (light/en analysing, dark/vi analysing, 22×22, 5×5) show 0 GTK warnings.

## Knowledge

`systematic-debugging` skill; `.claude/rules/bugfix.md`; `gtk-ui-design` skill.
