# 2026-11-11 — UI-23 typography and layout decisions

**Status:** Active

## Prompt

"Improve UI of program. Look also to the Font (Typography). Design and apply without my decision."
The user delegated all design calls.

## Scope

Live screenshots under Xvfb (light/en and dark/vi while analysing, 15×15/22×22/5×5) of every main
surface, plus the text-drawing code in `style.css`, `engine_status.cpp`, `win_graph_view.cpp`,
`board_renderer.cpp`, `bottom_panel.cpp` and `pv_view.cpp`. Not examined: settings dialog, command
palette, About dialog (inherit the CSS changes only).

## Findings

1. Engine stats were one long monospace/bold row. Its width became the analysis panel's minimum, so the
   board shrank (vi analysing: ~310px board vs ~450px in en idle). Values changed width on every update.
2. Mixed families: monospace stats/PV score next to proportional PV moves; sizes in px (11/12px), which ignore
   desktop text scaling.
3. Win-graph axis labels were Cairo toy text at a fixed 9px, left-aligned and off the gridlines.
4. Board coordinate labels used fontconfig `sans-serif`, not the desktop UI font.
5. Move log text touched the panel edge (no margins).

## Reasoning

- **Keep the desktop UI font** rather than imposing Inter or similar. It respects user choice, accessibility
  scaling and Vietnamese coverage, and the preferred font isn't installed on this host, so a
  forced family could not be verified. Bundling a font was rejected (asset pipeline, MSYS2, size L).
- **Hierarchy through size and weight, with relative units**: caption 0.85em / body 1em / title 1.15em,
  weights 400/600/700. The em units keep text-scaling working.
- **Tabular figures, not monospace, for numbers.** Width stays fixed (no jitter) without a code-font look.
  Monospace stays for raw logs/console only, with a stack covering Linux and Windows.
- **Wrap the stats** (FlowBox) instead of truncating them. Every value stays visible at any panel width.
- Contrast: keys use opacity 0.7 of the theme foreground, which stays ≥4.5:1 on Adwaita light/dark.
- Rejected: CSS `line-height` on textviews (breaks Engine Log gutter alignment); Pango for stone
  numbers (would re-baseline `test_ui20`, no visible gain).

## Knowledge

(No `docs/knowledge/` note covers typography; `ux-research-usability.md` and `a11y-aria-keyboard-focus.md` were checked and do not apply.)

`ui-ux-review` skill (Typography, Accessibility, Content formatting); `gtk-ui-design` skill;
prior audit `docs/audit/2026-10-06-board-marks-ux-review.md` (Cairo font-state leak context).

## Decision

Implemented as UI-23 (see `docs/todo/UI-23-typography-and-layout-polish.md`). Follow-ups not filed:
verify font fallback on Windows/MSYS2 and on a HiDPI / text-scaling ≠ 1 desktop.
