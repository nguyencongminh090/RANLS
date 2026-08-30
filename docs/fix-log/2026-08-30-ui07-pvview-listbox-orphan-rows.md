# UI-07 (second pass) — `PVView` leaked one orphan `GtkListBoxRow` per PV clear

**Timestamp:** 2026-08-30

This entry **continues and corrects**
`docs/fix-log/2026-08-30-ui07-pv-panel-accumulates-across-positions.md`, which is left as written
(append-only). That entry's fix (commit `60640c6`) was necessary but did not fix the reported bug;
its stated root cause was incomplete. Nothing in it is reverted.

## Prompt

The user re-tested branch `fix/ui-07-pv-cross-position` after `60640c6` and reported that the PV
panel **still accumulates a stale row per analysed position**. Re-dispatched for a deeper
root-cause diagnosis with an explicit instruction to reproduce in a test first rather than add
another view refresh.

## Root cause

One layer below where the first pass looked: inside `PVView`'s own widget bookkeeping.

`src/ui/pv_view.cpp` appended each PV row as a bare `Gtk::Box`:

```cpp
listBox_.append(*row);            // GTK implicitly wraps `row` in a GtkListBoxRow
```

and the shrink path removed that same `Gtk::Box`:

```cpp
listBox_.remove(*rows_.back().row);   // ← a *grandchild* of listBox_, not a child
```

`Gtk::ListBox::append()` wraps a non-`ListBoxRow` child in an implicitly-created `GtkListBoxRow`,
so `row`'s parent is that wrapper, not the list box. GTK 4 answers `gtk_list_box_remove()` on a
grandchild with

```
Gtk-WARNING **: Tried to remove non-child 0x…
```

and **removes nothing**. Confirmed directly against the installed GTK 4.22.4 / gtkmm 4.20.0.

Consequences, all matching the report exactly:

- `rows_.size()` correctly dropped to zero on every clear, but the row widget stayed parented to
  `listBox_` and on screen — **one orphan leaked per clear**. The user's "~12 accumulated `PV #1`
  rows" is twelve position changes, one orphan each.
- The next analysis then `append()`ed its row *below* the orphan, so the stale previous-position
  line rendered **above** the current one — the screenshot's row order.
- Both rows read `PV #1` because both came from `commitPV(0, …)`, which sets `pvIndex = 1`.
- The same defect made a MultiPV shrink (N → M, e.g. STATE-03's lowered-multiPV round) leave all N
  rows rendered.
- Orphan rows kept their `Gtk::EventControllerMotion` alive, so hovering one emitted a PV preview
  for a position that is no longer on the board.

### Why `60640c6` did not fix it

`60640c6` made `AnalysisPanel::signal_board_changed` call `pvView_.update(gameState_.pvLines(), …)`
on every position change. That is correct and is kept — but "update the view with an empty vector"
is precisely the operation that was broken. Making it happen reliably on every position change made
the leak *deterministic* (one orphan per position change) rather than fixing it.

It also explains why every existing test stayed green: `tests/test_ui07_pv_cross_position.cpp`,
`tests/test_ui04_pv_reset.cpp` and the rest assert `GameState::pvLines()`, and the model layer was
genuinely correct the whole time — `pvLines()` really is empty after each position change, and
`GomocupProtocol::currentPVs_` really does stay at size 1 with MultiPV=1. Nothing asserted what the
widget tree actually held.

`GameState::resetAnalysisState()` was also re-examined per the task's scope item 3 (the
`alreadyEmpty` / `analyzing_` early-returns and a possibly-armed `analysisDirty_`): it is correct
as written and was **not** changed. RT-01's and STATE-01's guards are intact.

## Fix

`src/ui/pv_view.h` / `src/ui/pv_view.cpp` — create the `Gtk::ListBoxRow` explicitly and keep a
pointer to it, so the shrink path removes a real child of `listBox_`:

```cpp
auto *listRow = Gtk::make_managed<Gtk::ListBoxRow>();
listRow->set_child(*row);
listBox_.append(*listRow);
...
listBox_.remove(*rows_.back().listRow);
```

Two lines of behaviour change, no restructuring.

### Boundaries respected

- `60640c6`'s `AnalysisPanel::signal_board_changed` refresh is **kept** — the panel stays
  authoritative on position change.
- RT-03 hover preservation intact: the motion controller still lives on the inner `Gtk::Box`, which
  is still reused in place across a content-only refresh. Only the *count-changed* path touches
  widgets, exactly as before.
- STATE-03's empty-`moves` filter in `PVView::update`, UI-04's `isAnalyzing()` gate +
  `clearAnalysisState()` wiring, STATE-01/RT-01's `resetAnalysisState()` guards: all untouched.
- No change to the wire protocol or to `generateAnalyzeRequest` / `generateStop` /
  `generateMoveRequest`.
- Menu bar, `MatchConfig`, settings dialog, win-graph mode logic (UI-06/UX-06): untouched.

## Tests

New binary **`rapfi-gui-ui-tests`** (`tests/CMakeLists.txt`), `tests/test_ui07_pv_view_rows.cpp`,
4 cases. This is deliberately a *second* target rather than an addition to `rapfi-gui-tests`: that
target's "links no gtkmm, enforced by construction" invariant
(`docs/audit/2026-08-21-test-framework-choice.md`) stays enforced, while real widgets finally get
real coverage. It skips its cases with a clean exit 0 when no display server is present, so it is
headless-CI-safe.

Cases (all read the *rendered* `GtkListBox` children, via a widget-tree walk — no production API
widened for tests):

1. *PVView drops its row widgets when the PV list clears* — 1 row → `update({})` → 0 rows → 1 row.
2. *repeated clear/refill cycles never accumulate PV rows* — 12 cycles still end at one row (the
   user's "~12 stale rows").
3. *MultiPV shrink from N to M leaves exactly M rendered rows* — 4 → 2 → 1, and no `PV #2` survives.
4. *real AnalysisPanel, real MESSAGE-depth log, two positions → one row* — a real `AnalysisPanel`
   (so `60640c6`'s handler runs) + real `GameState` + real `GomocupProtocol`, replaying the reported
   two-position `MESSAGE depth …` log through the real `EngineController` wiring. Asserts 1 rendered
   row at `d21/38`, 0 after the engine's own move lands, then exactly 1 at `d22/44` with no
   `d21/38` row.

Before the fix: **4 test cases / 16 assertions, 7 failed** — the AnalysisPanel case rendering
`REQUIRE( 2 == 1 )` rows, the exact reported signature.
After the fix: **4 test cases / 45 assertions, 0 failed**, and no `Tried to remove non-child`
warnings.

`tests/test_ui07_pv_cross_position.cpp` (first pass) is kept unchanged.

## Verification

- `./build.sh` — clean. A from-scratch build into a fresh directory emits only the three
  pre-existing `-Wunused-function` warnings in `gomocup_protocol.cpp`; no new warnings.
- `ctest --test-dir build_cmd --output-on-failure` — `100% tests passed, 0 tests failed out of 2`.
  - `rapfi-gui-tests`: **125 test cases / 1032 assertions, 0 failed**.
  - `rapfi-gui-ui-tests`: **4 test cases / 45 assertions, 0 failed**.
- **Live engine, full reproduction sequence.** The GTK4-Wayland window still cannot be driven by
  script here (no `ydotool`/`wtype`/`dotool`; `xdotool` cannot see the Wayland surface; board clicks
  are not GActions), so instead of clicking, a scratchpad driver wired the **real**
  `gomoku-portal-ui-distribute/pbrain-rapfi` subprocess through the real
  `EngineProcess` → `EngineController` → `GomocupProtocol` → `GameState` → real `AnalysisPanel`
  widget tree, with MainWindow's real 75 ms `tickAnalysis` timer, and ran the reported sequence:
  analyse the 3-stone position → STOP (engine answers with its own move) → analyse the 4-stone
  position. It prints the actual rendered list-box rows.

  Pre-fix (code as of `60640c6`) — the reported bug, verbatim:

  ```
  === B: right after STOP + the engine's own move landed ===
    model pvLines()      : 0
    RENDERED PV rows     : 1
      row 0: PV #1  19 / 52.4%  d28/37  K5 → L4 → J4 → L6 → …
  === C: mid-analysis of the 4-stone position ===
    model pvLines()      : 1
    RENDERED PV rows     : 2
      row 0: PV #1  19 / 52.4%  d28/37  K5 → L4 → J4 → L6 → …   ← stale, 3-stone position
      row 1: PV #1  14 / 51.7%  d28/39  J6 → K6 → K7 → I5 → …   ← current
  ```

  Post-fix, same driver, same engine:

  ```
  === A: mid-analysis of the 3-stone position ===  model 1 / RENDERED 1
  === B: right after STOP + the engine's own move ===  model 0 / RENDERED 0
  === C: mid-analysis of the 4-stone position ===  model 1 / RENDERED 1
  === D: after second STOP + second engine move ===  model 0 / RENDERED 0
  ```

  Re-run with MultiPV=4: rendered row count tracked `pvLines()` exactly at every step (3 rows during
  the multi-PV round, 0 after the position change), and no GTK warnings.

  What was **not** done: no human mouse/keyboard smoke of the shipped `build_cmd/rapfi-gui` window,
  and no screenshot of the panel, for the input-injection reasons above. The live driver exercises
  the identical object graph the GUI builds, driven by the identical signals, with a real engine —
  everything except the pointer events that originate a move.

## Follow-ups deliberately left out of scope

Noted here rather than fixed, per the task's scope discipline:

- Orphaned rows also kept live hover controllers that emitted board previews for stale PVs. This is
  a symptom of the same defect and is gone with it; no separate change made.
- `src/ui/` has no other `Gtk::ListBox` user (checked), so no sibling occurrence of this
  remove-the-wrong-widget pattern exists to fix.
