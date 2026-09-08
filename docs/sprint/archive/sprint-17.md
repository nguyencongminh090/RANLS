# Sprint 17 (closed 2026-09-08)

**Goal:** Enable the engine incremental analysis stream — per-depth PV / Multi-PV / value /
board-PV updates reach the GUI during a search, not just once at the end.
**Dates:** 2026-09-07 to 2026-09-08.

## Final state — all items shipped (1 of 1)

| CODE | Summary | Status |
|---|---|---|
| PROTO-05 | enable engine incremental analysis stream (YXSHOWINFO + configurable `INFO SHOW_DETAIL`) | ✅ DONE |

Single-item sprint, opened from the Backlog the same day PROTO-05 was filed (from the
2026-09-07 diagnosis note). Points not estimated (consistent with Sprints 3–16).

## What shipped

- **PROTO-05** (PR #32, squash `ffbefb3`): the GUI was telling a stock Rapfi to stay silent —
  `generateConfig()` hardcoded `INFO SHOW_DETAIL 0` and the GUI never sent `YXSHOWINFO`, so the
  engine stayed at `messageMode BRIEF` with both the `REALTIME` and `INFO` detail feeds off and
  reported PV / Multi-PV / value / board markers exactly once, at search end, one line only.
  Fix (Mechanism C, decided with the user): (1) `generateStart()` prepends `YXSHOWINFO`
  unconditionally — auto-bumps Rapfi BRIEF→NORMAL so per-depth `Depth …` and per-candidate
  `(n) …` lines flow; tradeoff — unknown-command errors are silenced for the session. (2) New
  `EngineConfig::showDetail` (default 3) emitted as `INFO SHOW_DETAIL <n>` before the
  `customParams` loop, replacing the hardcoded `0`; a user `!set show_detail N` still wins.
  (3) `parseMessage` `"Speed "` branch now parses the leading speed token into `nps` (was
  dropped). (4) `"(n)"` branch reads PV moves from the trailing pipe field + parses `SD <n>` so
  Rapfi's 4-part `printRootMoves` ranked list parses (moves were being read from `"SD 14"` and
  lost); `commitPV` gained a `roundStart` flag so the `"Depth …"` / `"Bestline …"` summary lines
  no longer truncate the `(n)`-stream Multi-PV list #2..#N. SettingsDialog "Analysis Detail" 0–3
  spin added (Search tab) + persisted. Regression test
  `tests/test_proto05_incremental_stream.cpp` (2 cases) replays a NORMAL + `SHOW_DETAIL 3`
  transcript through `GomocupProtocol → GameState → BoardViewModel` and asserts depth 10→11→12,
  3 non-empty PV lines each carrying their own depth, 3 `candidateMoves` markers mid-search, and
  NPS from the summary line. `ctest` 4/4 green. **Deferred / outstanding:** the live-engine
  progressive-update smoke (analyse ~5 s against a real engine, watch the readout climb, confirm
  Multi-PV rows 2..N) is still a **human** step — no engine binary or display on the build host.
  Detail: `docs/fix-log/2026-09-07-proto-05-enable-incremental-analysis-stream.md`.

## Lessons

- **The GUI was architected for a streaming feed it never subscribed to.** The whole RT-01
  throttle, STATE-03 "new round" truncation, `onPVDone`, `parseRealtimePV`, UCILIKE parser — all
  dead paths against a stock engine, because the config commands turned the feed off. When a
  parser has elaborate handling that "never fires", check what the other side was actually told
  to send before assuming the parser is wrong.
- **Reading the engine's own source settled it.** `Rapfi/search/searchoutput.cpp` +
  `docs/protocol.md` made the BRIEF-vs-NORMAL / `SHOW_DETAIL` matrix unambiguous — worth doing
  before proposing any protocol fix (this repo's rule already says so; PROTO-05 confirmed it).
- **Follow-on discovered during review, not during planning:** PROTO-05 fixed *whether* the data
  arrives; it deliberately left *rendering* alone. Comparing against `RefYXB/Yixin-Board` after
  the merge showed the reference's live board overlay (per-cell winrate tags + `REALTIME`
  POS/LOST/BEST feed) was never ported — filed as **PROTO-06** with the design fully resolved.
- **Carried from Sprint 16, still true:** the build host has no engine binary and no display, so
  any "watch it work in a running app" acceptance is a human step; the automated regression must
  run off a captured/constructed transcript and assert real behaviour (depth actually climbing,
  `pvLines()` actually reaching `multiPV`), not "the parse path exists".
- **Carried from Sprint 16, still true:** `sigc::mem_fun` on a recurring `Glib::signal_timeout` /
  `signal_idle` needs `sigc::track_obj` or an explicit `disconnect()`. PROTO-05's heavier
  `SHOW_DETAIL 3` engine-log stream went through `bottom_panel` unchanged — RT-02 bounding and
  UI-10/UI-15 sticky-scroll still hold, no new untracked timer added. An audit sweep of every
  `signal_timeout` / `signal_idle` connect in `src/ui/` is still owed a future `CLEAN` item.

## Rolled over to Backlog

Nothing rolled over — the one committed item finished.

## Next sprint

Sprint 18 — run `/sprint open 18 "<goal>" PROTO-06` to commit it. PROTO-06 (replace the analysis
board overlay with the Yixin-Board model) is in the Backlog with its design fully resolved with
the user (decision (a): implement only what Rapfi's output actually supports). Release `v0.6.0`
cut at close (see `CHANGELOG.md`).
