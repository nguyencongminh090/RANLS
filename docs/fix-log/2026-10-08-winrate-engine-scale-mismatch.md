# Winrate % on status bar / PV panel disagreed with the board tag (2026-10-08)

**Report:** winrate shown on the board does not match the winrate in the log/panels.

**Root cause:** the cp -> winrate scale is engine/config dependent. `yixin-net/pbrain-rapfi` reports
`INFO WINRATE` with an implied scale of ~116.37 (checked on 6 samples: eval -221 -> 0.130211), not 200.
The board tag uses `INFO WINRATE`, but `MESSAGE Depth … | Eval <cp>`, `(n) <cp> | …` and UCI-like `ev <cp>`
lines were re-derived through `cpToWinrate` (hardcoded `1/(1+e^(-cp/200))` = 0.2488) and overwrote
`currentStatus_.winrate` / `PVLine::score`. The original Yixin-Board (`main.c:6596`) reads `INFO WINRATE`
and uses `INFO EVAL` for mate detection only.

**Fix (`src/engine/gomocup_protocol.{h,cpp}`):** record the engine's winrate per PV (with its INFO EVAL
text) at `PV DONE`; the MESSAGE eval parsers go through new `parseMessageEval`, which keeps that winrate
when the eval text matches, else falls back to the sigmoid (MESSAGE-only engines) and mates are unchanged.
`INFO EVAL` no longer clobbers an `INFO WINRATE` that arrived first.

**Regression test:** `tests/test_fix_winrate_engine_scale.cpp` (5 cases, real yixin-net values). 3 failed
before (0.2488 vs 0.1302), pass after. ctest 5/5.

**Not fixed (separate):** database markers still use sigmoid(value/200) (`board_view_model.cpp`); the board
tag clamps to 1..99 where the original clamps only the top at 99. Not verified in the live GUI.
