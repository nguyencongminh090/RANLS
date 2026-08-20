---
name: ui-ux-review
description: Reviews YixinBoard's GTK4 desktop UI for usability — board legibility, analysis/PV display, move-tree navigation, engine feedback, settings ergonomics. Use when asked to review, critique, or improve the UI/UX of this app, evaluate a specific screen/widget's usability, or before/after a UI change to check it against the app's own interaction patterns. Not for GTK4 implementation mechanics (see gtk-ui-design) or raw rendering performance (see perf-optimization).
---

# UI/UX review — YixinBoard

YixinBoard is a desktop tool for one job: watching and steering a Gomoku/Renju engine's analysis
in real time. Every review question reduces to "does this surface make the engine's thinking
legible and the board controllable, without friction." Judge against that, not generic UI taste.

## The actual surfaces (review each against its own job)

| Widget | File | Job | Review for |
|---|---|---|---|
| Board | `src/ui/board_view.cpp`, `board_renderer.cpp` | Show position + accept moves | Stone/grid contrast in both GTK themes, last-move marker visibility, click-target size at small board sizes (5×5 up to 22×22 per `onBoardSize`), coordinate label legibility at `kCoordMargin` |
| Analysis panel | `src/ui/analysis_panel.cpp` | Surface engine thinking | Depth/nodes/nps/winrate readability at a glance, update rate (does it flicker on every `INFO` line?) |
| PV view | `src/ui/pv_view.cpp` | Show MultiPV lines | Line count vs. panel height (does `multiPV` config produce a scrollable mess?), how a PV maps back onto the board on hover/click |
| Win-rate graph | `src/ui/win_graph_view.cpp` | Show eval trend over the game | Axis legibility, current-move marker, mate-score handling (`mateStep` sign convention — does the graph do something sane at ±mate?) |
| Tree explorer | `src/ui/tree_explorer.cpp` | Browse variation tree as a table | Column widths at deep trees, current-path highlighting, click-to-jump latency |
| Tree node view | `src/ui/tree_node_view.cpp` | Browse variation tree as a graph | Branch-point legibility (`hasBranches()`), hover feedback, does layout stay stable as the tree grows (nodes shouldn't jump around on unrelated updates) |
| Engine status | `src/ui/engine_status.cpp` | Show engine running/analyzing state | Is "engine not started" vs "engine crashed" (`signal_process_died`) distinguishable, not just silent? |
| Settings dialog | `src/ui/settings_dialog.cpp` | Configure engine path/rule/view | Does changing engine path mid-session make it obvious a restart happened (`EngineController::reloadEngine`)? |
| Bottom panel | `src/ui/bottom_panel.cpp` | (secondary controls) | Discoverability vs. clutter |

## Review checklist

1. **Engine-state honesty.** `GameState` emits `signal_engine_analysis`, `signal_analyzing_state`,
   `signal_engine_state` — for any screen showing engine output, confirm the UI has a *visible* idle
   / thinking / stopped / crashed state, not just data that stops updating silently.
2. **MultiPV scaling.** `EngineConfig::multiPV` is user-configurable — check PV view and board
   overlay (if any) at multiPV=1 vs a high value; a design that only got reviewed at multiPV=1 often
   breaks at multiPV=8+.
3. **Board size range.** The board supports 5–22 (see `MainWindow::onBoardSize`) — a layout tuned
   only at 15×15 (the common case) often breaks at the extremes; check stone size, coordinate
   labels, and window resize behavior at both ends.
4. **Theme parity.** GTK4 apps inherit light/dark from the desktop theme (`prefers-color-scheme`
   equivalent) — anything hand-painted in `BoardRenderer`'s Cairo drawing (stone colors, last-move
   highlight, grid lines) needs checking in both, not just whichever the reviewer's desktop uses.
5. **Move-tree consistency.** Both `TreeExplorer` (table) and `TreeNodeView` (graph) render the same
   `VariationTree` — when reviewing one, check the other agrees on current-path highlighting and
   click-to-jump target (`signal_node_selected` / `signal_node_clicked` both carry a `path`).
6. **Rule-dependent UI.** `GameRule` (Freestyle/Standard/Renju) changes what's legal — confirm
   forbidden-move indication (Renju) or overline handling (Standard) is visible on the board itself,
   not just in a config dropdown the user has to remember.
7. **Database overlay.** `DatabaseEntry::boardText`/`label` (W/L/D/VCF) render on the board when a
   database is loaded — check it doesn't collide with the PV-hover overlay or last-move marker.

## What NOT to flag here

- Raw draw-call cost, redraw scope, main-thread blocking → `perf-optimization` skill.
- Whether a widget is built the idiomatic GTK4 way (draw_func vs on_draw, ListStore vs manual
  TreeView) → `gtk-ui-design` skill.
- Whether a UI change belongs in `ui/` vs leaking domain logic from `model/` → `software-architecture`.

## Universal UX laws (framework-agnostic, adapted to desktop/GTK)

Inherited from the `ui-ux-pro-max` skill's `ux-guidelines.csv` — specifically its 53 rows tagged
`Platform=All` (as opposed to `Web`/`Mobile`/`VisionOS`), i.e. the subset that isn't CSS/Tailwind/
React-specific. Its color-palette and font-pairing databases were **not** pulled in: they're tuned
for SaaS/e-commerce web products and don't fit a wood-board game renderer — the *methodology*
(contrast ratios, type-scale theory) transfers, the literal hex/font database doesn't. Grouped by
category with the GTK4/desktop equivalent mechanism named where one exists in this codebase.

**Interaction states**
- Every clickable widget needs a visible focus indicator for keyboard nav (Tab/arrow-key board
  navigation, if supported) — GTK4 draws this automatically for standard widgets via the theme's
  `:focus-visible` CSS, but **custom-drawn widgets don't get it for free**: `BoardRenderer`/
  `BoardView` and `TreeNodeView` paint everything themselves in `onDraw`, so a keyboard-focused cell
  or tree node needs its own explicit focus ring drawn in the Cairo callback — check whether one exists.
- Pressed/active feedback on click, not just hover — relevant to board cell click, tree node click.
- Disabled state must be visually distinct from normal (not just non-functional) — check "Analyze"
  controls while `analyzing_` is already true (`GameState::isAnalyzing()`), not just that clicking
  again is a no-op in `EngineController::analyze()`.
- Confirm before destructive/irreversible actions — new game clearing the board, board-size change
  mid-game, `YXDELETEDATABASE` operations. Check `MainWindow::onBoardSize` and any "clear database"
  path for a confirmation step before the state is gone.

**Feedback**
- Loading/busy indication for anything taking >300ms — engine analysis already has depth/nodes
  counters (serves this role); check engine *startup* (`startEngine`) and the 500ms+ blocking waits
  in `EngineController::stopEngine`/`EngineProcess::stop` (see `perf-optimization` skill) for a gap
  between "user clicked" and "visible feedback appears."
- Empty states need a message, not a blank panel — PV view/tree explorer before any analysis has
  run; check they show something like "no analysis yet," not an empty table.
- Error feedback near the problem, not just a console line — an invalid engine path in
  `SettingsDialog` should surface inline, not only as a `signal_engine_output` message the user has
  to notice in a log panel.
- Toast/transient notification for non-critical status changes (engine crashed, database loaded) —
  GTK4/libadwaita has `Adw::Toast` for this if libadwaita is linked; check what currently happens on
  `signal_process_died` (per the `ui-ux-review` "engine-state honesty" checklist above) — a missed
  console line is not equivalent to a toast a user will actually notice.

**Accessibility**
- Color contrast minimum 4.5:1 for text — applies to `style.css` colors and any Cairo-drawn text
  (coordinate labels, PV line text) same as web; verify in both light and dark theme, not just one.
- Never encode meaning by color alone — Renju forbidden-move marking, database `W`/`L`/`D` labels
  (`DatabaseEntry::label`), and the win-rate graph should each pair color with a shape/icon/text, not
  rely on hue alone (also fails for colorblind users regardless of platform).
- Icon-only buttons need an accessible name — GTK4 equivalent of `aria-label` is
  `Gtk::Accessible::update_property(Gtk::Accessible::Property::LABEL, ...)` or a tooltip via
  `set_tooltip_text()`; check toolbar/icon buttons in `MainWindow` and `BottomPanel`.
- Custom-drawn widgets need a real accessible role — `TreeNodeView`/`BoardRenderer` being pure
  `DrawingArea` means a screen reader sees nothing meaningful by default; GTK4's `Gtk::Accessible`
  interface exists precisely for this gap. Flag as a known limitation if not implemented, don't
  assume it's handled because the widget "looks like" a normal one.

**Animation/motion**
- 150–300ms for micro-interactions, longer reads as sluggish; use ease-out entering / ease-in
  exiting, not linear.
- Respect reduced-motion — GTK's equivalent of `prefers-reduced-motion` is the
  `gtk-enable-animations` setting (`Gtk::Settings::property_gtk_enable_animations()`); any
  hand-rolled animation (tree node transitions, hover effects) should check it, not just rely on GTK
  built-in widget animations honoring it automatically.
- Animate 1–2 elements per view at most; infinite/continuous animation reserved for loading
  indicators only, never decorative.

**Typography**
- Line-height 1.5–1.75 for body text, clear size/weight jump between headings and body, consistent
  type scale rather than ad hoc sizes — applies to `style.css` the same as any CSS.
- Darker text on light backgrounds and vice versa, never low-contrast gray-on-gray — same rule as
  the contrast item above, called out separately because it's the most commonly violated one.

**Forms (Settings dialog)**
- Every input needs a visible label, not placeholder-only.
- Validate and show errors near the field (engine path exists? multiPV in valid range?), not only
  on submit / not only in a console log.
- Mark required fields, use the right input type (numeric spinner for `multiPV`/board size rather
  than free text) — `MainWindow::onBoardSize` already does this right with `Gtk::SpinButton`; use it
  as the reference pattern for other numeric settings.

**Content formatting**
- Truncate long content gracefully (long PV move sequences, long file paths) with an expand option,
  not overflow/broken layout.
- Format large numbers for readability — `EngineStatus::nodes`/`nps` are `int64_t` and can reach
  the billions during deep search; check whether they're rendered with thousands separators or as a
  raw digit string.

## Delivering a review

State findings against the table above (surface → job → what's actually wrong), not as a generic
heuristic checklist. If a finding is speculative (untested at an extreme board size, e.g.), say so
explicitly rather than asserting it as observed. Follow the repo's tracking convention for anything
non-trivial: file it as a `TODO.md` Backlog item (`docs/todo/<CODE>-<slug>.md`) rather than fixing it
inline unless the user asked for the fix directly — see `/CLAUDE.md`.
