#pragma once

/// Pure logic for UI-02's current-path highlight, factored out of
/// TreeExplorer (src/ui/tree_explorer.cpp) so it can be unit-tested without
/// gtkmm (see tests/test_ui02_tree_row_highlight.cpp).
///
/// TreeExplorer::update() lists MoveHistory::moves() up to
/// MoveHistory::moveCount() — i.e. only the current line, up to and
/// including the current cursor position. There is no "future"/redo-able
/// row in that list, so the *last* row is always the current game position.
///
/// `rowIndex` is 0-based; `historyCount` is MoveHistory::moveCount() (the
/// number of rows TreeExplorer::update() builds).
inline bool isCurrentHistoryRow(int rowIndex, int historyCount)
{
    return historyCount > 0 && rowIndex == historyCount - 1;
}
