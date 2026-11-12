# PROTO-09 — Stable realtime winrate on the board in every Rapfi message mode

**Status:** ✅ FIXED (2026-11-12, branch `proto-09/stable-realtime-winrate`; Sprint 27, pulled in on user request) — see [fix-log](../fix-log/2026-11-12-proto-09-realtime-winrate-flicker.md).
**Area:** `src/engine/gomocup_protocol.{h,cpp}` (MESSAGE/INFO parsing, PV-round bookkeeping), new `src/model/search_overlay_animator.{h,cpp}`, `src/ui/board_view.{h,cpp}`, `src/ui/board_renderer.{h,cpp}`, `src/model/board_view_model.h` (`SearchOverlayMark::alpha`)
**Priority:** P2
**Source:** user request 2026-11-12 — "Improve winrate realtime display on board and message parsing (update smoothly, no flicker). Support all message modes." Rapfi reference: `Rapfi_V2/rapfi/Rapfi/search/searchoutput.cpp`.
**Design:** none — scoped directly from real engine transcripts (alpha-beta and MCTS × normal / brief / ucilike, MultiPV 1 and 3).
**Depends on / relates to:** PROTO-05 (streams on), PROTO-06 (board overlay), STATE-03 (PV list shrink — contract refined here), RT-01 (75 ms tick).

## Problem

Replaying real Rapfi 0.44 transcripts through `GomocupProtocol` showed the analysis state changing
between lines in ways the engine never reported:

1. Every depth round the PV list collapsed to one row and re-grew with empty placeholders.
2. MCTS `ucilike` lines had their draw rate (`d 8.68`) read as the depth; the bouncing depth wiped
   board tags, including the best move's.
3. MCTS reports no depth, so the depth-keyed stale-tag cleanup never ran.
4. With the `INFO` stream off, no board tag was written at all.
5. `selDepth` alternated between two sources; rows alternated between INFO and MESSAGE numbers.
6. Evals with no `INFO WINRATE` of their own were converted with a hardcoded `/200`.
7. On the board, every change was an instant jump (number, appear, disappear).

## What was done

- **One round model for both streams** (`beginReport` / `pruneStale`): rows and tags are stamped with
  the round that last refreshed them; they drop when `INFO NUMPV` says the round is complete, or —
  on a MESSAGE-only stream, which has no count — when the next round opens.
- **INFO wins within a round**; MESSAGE lines only fill what INFO did not write.
- **Tags from every source** (`stampTag`), so MESSAGE-only modes get board tags.
- **MCTS fields parsed**: ucilike `w` / `d` (draw rate) / `v` / `seldepth` / `stdev` / `prior`;
  NORMAL `(W .., D .., S ..)` and `V <visits>`.
- **Monotonic depth/selDepth**; **learned cp→winrate scale** from INFO (EVAL, WINRATE) pairs.
- **Eased overlay** (`SearchOverlayAnimator`, GTK-free): tags fade in/out and winrate numbers count
  to the new value; driven by the widget frame clock only while something moves; snaps when
  unmapped or when `gtk-enable-animations` is off.

## Behaviour change to note

STATE-03's MESSAGE-stream truncation is one round later than before: an immediate cut at the
`(1)` line is exactly what blanked rows 2..N for the whole round. `test_gomocup_protocol.cpp`'s
STATE-03 case was updated to the new contract (list still shrinks; INFO NUMPV still cuts at once).

## Verification

`tests/test_proto09_stable_rounds.cpp` (12 cases) + `tests/test_proto09_overlay_animator.cpp`
(8 cases). ctest 5/5 (310 unit cases + UI suite). Live: `ranls-gui` + the user's `yixin-net`
Rapfi (ucilike, MultiPV 3) under Xvfb — tags fade in, overlay never drops during the search.

## Not done (out of scope)

- MESSAGE-line moves are in letter notation (`I7`), which the GUI maps assuming Rapfi's
  `coord_conversion_mode = "flipY_X"`; with `none` they land on the wrong cell. Pre-existing.
- The engine-status "Eval" text and the PV list are not eased — only the board overlay is.
