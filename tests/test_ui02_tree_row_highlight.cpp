// Regression tests for UI-02's current-path highlight logic in TreeExplorer
// (the "Table" tab). TreeExplorer itself is gtkmm widget code excluded from
// this headless harness (see tests/CMakeLists.txt), but the row-highlight
// predicate is pure logic and lives in a gtkmm-free header specifically so
// it can be pinned here.

#include "vendor/doctest.h"

#include "model/tree_row_highlight.h"

TEST_CASE("isCurrentHistoryRow: empty history has no current row") {
    CHECK_FALSE(isCurrentHistoryRow(0, 0));
}

TEST_CASE("isCurrentHistoryRow: single-row history marks row 0 current") {
    CHECK(isCurrentHistoryRow(0, 1));
}

TEST_CASE("isCurrentHistoryRow: only the last row is current") {
    int count = 5;
    for (int i = 0; i < count - 1; ++i) {
        CHECK_FALSE(isCurrentHistoryRow(i, count));
    }
    CHECK(isCurrentHistoryRow(count - 1, count));
}

TEST_CASE("isCurrentHistoryRow: does not match an index beyond the row count") {
    // Guards against a stale/out-of-range index (e.g. a row built for a
    // longer history than what's currently displayed) being misreported.
    CHECK_FALSE(isCurrentHistoryRow(5, 3));
}
