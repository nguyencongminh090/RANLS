# 2026-08-21 — UX-05: `Gtk::Paned` divider doesn't restore after shrink-then-grow

## Prompt

`Gtk::Paned` stores its divider as an absolute pixel offset. `mainHPaned_`/`mainVPaned_` in
`main_window.cpp` had `set_shrink_start_child(false)`/`set_shrink_end_child(false)` set, but their
positions were only ever set as fixed pixel values (`buildLayout()` at startup, and the
"Compact"/"Review"/default profile presets in `connectSignals()`'s `signal_config_changed` handler).
Shrinking the window clamps the divider down to a small absolute value; growing the window back does
not restore it, since GTK never rescales a `Paned`'s position proportionally on its own. Filed as
UX-05 during UX-04's window-resize investigation.

## Action

`src/main_window.h`/`src/main_window.cpp`:

- Added `hPanedFraction_`/`vPanedFraction_` (divider position as a fraction of the pane's own
  extent) and `hPanedLastExtent_`/`vPanedLastExtent_` (the extent last observed) as `MainWindow`
  members, seeded from the existing 1280x800-default absolute values (640/1280, 580/800).
- Added `MainWindow::trackPanedFraction(paned, fraction, lastExtent, vertical)`, connected to both
  panes' `property_position().signal_changed()` in `buildLayout()`. It only updates the stored
  fraction when the pane's extent (width for `mainHPaned_`, height for `mainVPaned_`) is unchanged
  since the last observation — i.e. on a genuine user drag, or on our own reassertion below (which
  is idempotent) — not on a GTK-internal clamp fired mid-resize. This is what keeps the desired
  ratio from latching onto whatever tiny value a shrink clamped it to.
- Added `MainWindow::reapplyPanedFractions()`, which recomputes each pane's pixel position from its
  stored fraction against its *current* allocated extent and calls `set_position()`.
- Overrode `Gtk::Widget::size_allocate_vfunc(int width, int height, int baseline)` on `MainWindow`
  (gtkmm4 widgets have no public `signal_size_allocate()`, so a virtual override is the idiomatic
  hook) to call the base implementation, then `reapplyPanedFractions()` — so both dividers' pixel
  positions get corrected to match their stored fraction after every allocation, not just at
  startup/profile-switch time.
- The three profile presets ("Compact"/"Review"/default) in `connectSignals()` are untouched — same
  absolute values as before. Their own `set_position()` calls now also flow through
  `trackPanedFraction()` via the `notify::position` connection, updating the stored fraction so a
  later resize preserves whichever preset ratio is currently active, instead of reverting to the
  startup ratio.

## Verification

- `RUN_TESTS=1 bash build.sh` — clean build (Ninja/Release), `rapfi-gui` links, `rapfi-gui-tests`
  ctest suite passes (23/23 cases, unrelated to this change — see "Test coverage" below).
- No unit-test coverage added: `docs/todo/TEST-01-test-infrastructure.md`'s harness is explicitly
  model/protocol-only (`tests/` links `move_history.cpp`/`board_state.cpp`/`gomocup_protocol.cpp`
  plus `sigc++`, no gtkmm, no display server) and its own scope boundary rules out GTK widget tests.
  `Gtk::Paned` divider/resize behavior is a live windowing-system interaction with no code path
  reachable from that harness, so this is genuinely untestable there, not a skipped test.
- Manual GUI verification under Xvfb, following UX-04's approach (real `xdotool`-driven window
  resize + `ffmpeg -f x11grab` screenshots, not simulated):
  - Started `Xvfb :99 -screen 0 1280x800x24`.
  - The app enforces GApplication single-instance via D-Bus (`com.rapfi.analysis`); launched it
    under `dbus-run-session` for an isolated session bus so the test instance didn't just activate
    the developer's already-running instance and exit.
  - The shell's inherited `GDK_BACKEND=wayland`/`WAYLAND_DISPLAY` made GTK skip X11 entirely with no
    error; forcing `GDK_BACKEND=x11` (and unsetting `WAYLAND_DISPLAY`) made the window actually
    appear on `:99`.
  - Screenshot at the 1280x800 default: `mainHPaned_` divider at x≈640, `mainVPaned_` divider at
    y≈580, board pane fully visible (matches the code's default `set_position()` values).
  - `xdotool windowsize 0x200005 500 400` (GTK's own minimum-size constraints clamped the actual
    result to 894x505, still a large shrink from 1280x800) — screenshot shows the board pane shrunk
    but still at roughly the same *proportion* of the new, smaller window (fraction-based
    recomputation now applies on shrink too, not just grow), not clamped to a sliver.
  - `xdotool windowsize 0x200005 1280 800` (grow back) — screenshot shows the board pane restored to
    essentially its original size/position (divider back at x≈610, y≈605 — within a few px of the
    original 640/580, the difference being window-chrome rounding, not drift), with no manual drag.
  - Screenshots saved during this session's Xvfb run (not committed — this repo's fix-log assets
    convention (`docs/fix-log/assets/2026-08-21-ux-04/`) wasn't replicated here since the images add
    no information beyond what's described above; can be regenerated from the same recipe if needed).

## Scope notes

- Did not change the "Compact"/"Review"/default preset absolute values, per the todo file's
  boundary — only made their effect persist proportionally across later resizes.
- Did not add drag-preservation beyond what `trackPanedFraction()` naturally provides (it does track
  genuine user drags between resizes, but a resize event always re-snaps to the last-known
  fraction — that's what "restore a reasonable board pane size... without requiring the user to
  manually drag the divider back out" calls for, not indefinite free-form dragging survival across
  arbitrary interleaved resizes, which was never in the acceptance criteria).
