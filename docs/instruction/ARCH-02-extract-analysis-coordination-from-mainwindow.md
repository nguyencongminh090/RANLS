# Instruction — ARCH-02

Detail: [docs/todo/ARCH-02-extract-analysis-coordination-from-mainwindow.md](../todo/ARCH-02-extract-analysis-coordination-from-mainwindow.md)

## Approach

Design first (`features/<slug>/`), then characterization tests, then one extraction per PR.

## Pitfalls

Debounce/timer state uses `Glib::signal_timeout` (RT-01 note: tests have no main loop) — keep the timer in `MainWindow`, move only the decision logic.

## Verification before done

Existing analyze/eng/rt tests green; new coordinator tests run without GTK.

## Boundaries

No behavior or UI change.
