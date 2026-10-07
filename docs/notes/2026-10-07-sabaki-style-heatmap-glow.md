# Sabaki-style winrate heatmap: stone-sized disc, centred text, best-move glow (2026-10-07)

**Status:** Expiring note (size S, one file `src/ui/board_renderer.cpp`).

Request: "modify Sabaki heatmap when display winrate, the best move is glow and text (winrate%) display at center."

Change: the engine Tag mark is now a stone-sized heat disc (0.92 × stone radius, alpha 0.85) with the
label centred on the disc using text-extent bearings, font shrunk (min 8 px) to fit. The best move
(`isBest`) gets a cyan radial glow drawn *behind* the disc plus a light-cyan rim, replacing the hard UI-18/UI-19
ring, so it can never cut the text. Non-Tag best cells (lost/examined/examining) get the same glow.
Layering unchanged (`ui` only). `test_ui19_best_ring_clears_tag_text` still passes (glow/rim are not near-white).
Not verified in the live window (offscreen tests only).
