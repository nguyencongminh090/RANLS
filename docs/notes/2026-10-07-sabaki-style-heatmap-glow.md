# Sabaki-style winrate heatmap: stone-sized disc, centred text, best-move glow (2026-10-07)

**Status:** Expiring note (size S, one file `src/ui/board_renderer.cpp`).

Request: "modify Sabaki heatmap when display winrate, the best move is glow and text (winrate%) display at center."

Source read: Sabaki `src/modules/analysis.js` (`getAnalysisHeatMapCell`, strength 1..9) + Shudan `css/goban.css`
(`.shudan-heat_N`, `.shudan-heatlabel`).

Change (`src/ui/board_renderer.cpp`, `drawSearchOverlay`): each engine Tag is a soft blurred blob (Cairo radial
gradient mimicking the CSS `box-shadow` spread/blur) coloured by strength 1..9 (red/purple/blue/green, best = 9
= widest green glow), with a bold white centred label (shadow, 0.36 x cell). Replaces the hard disc + ring
(UI-18/UI-19), so nothing can cover the text. Deviation: Sabaki's strength uses visits x winrate; the view model
has no visits, so strength = winrate relative to the best tag. Layering unchanged.
Glow radii scaled by 0.6 (kGlowScale) after review: Sabaki's are ~2 cells wide.
Strength later made steeper: 1 step per 4 winrate points below the best tag (was ratio-based, left 44% vs 51% green).
Checked via an offscreen PNG (not committed) and `RUN_TESTS=1 ./build.sh` 5/5; not verified in the live window.
