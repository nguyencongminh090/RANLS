# Database marker heat colour used a hardcoded winrate scale (2026-10-08)

**Report:** follow-up to `2026-10-08-winrate-engine-scale-mismatch.md` -- "fix database markers scale too".

**Root cause:** `BoardViewModel::update()` set `Marker::eval = sigmoid(entry.value / 200)`. Rapfi's own marker
label for an exact record is `clamp(int(valueToWinRate(-value) * 100), 0, 99) + "%"` (`database/dbtypes.cpp`
`displayLabel`): the engine's scale (~116.37 for yixin-net) **and** the opposite sign. So the marker colour
disagreed with the "NN%" text on the same marker (e.g. value 221: text 13%, colour from 75%).

**Fix (`src/model/board_view_model.cpp`):** `eval` is read from the engine's display label like the original
Yixin-Board (`main.c` ~766/823): `W..` -> 1.0, `L..` -> 0.0, `NN%` -> NN/100, anything else (`D`, empty
label for non-exact bounds) -> -1 (default marker colour; previously a sigmoid of the value).

**Regression test:** `tests/test_fix_database_marker_winrate.cpp` (2 cases); both failed before the fix. ctest 5/5.

**Not fixed / not verified:** `isBest` still picks the highest raw `value` (UX-07) -- given the engine labels
use `-value` this may favour the wrong side; not investigated. Not verified in the live GUI.
