# Instruction — ARCH-02

Detail: [docs/todo/ARCH-02-extract-analysis-coordination-from-mainwindow.md](../todo/ARCH-02-extract-analysis-coordination-from-mainwindow.md)

## Approach

Design is resolved (`features/extract-mainwindow-orchestration/planning.md`). Do after ARCH-04 (characterization tests). One extraction per PR.

## Pitfalls

Coalescing uses `Glib::signal_idle().connect_once` (tests have no main loop) — inject it as `postIdle`; keep the Glib adapter in `MainWindow`. The idle callback captures `this`: the coordinator must not outlive `EngineController`/`GameState`. Do not reorder the ANLZ-05/07 guards.

## Verification before done

Existing analyze/eng/rt tests green; new coordinator tests run without GTK.

## Boundaries

No behavior or UI change.
