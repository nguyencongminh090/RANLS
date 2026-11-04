#pragma once

// Shared board-geometry constants used by both BoardRenderer (drawing) and
// BoardView (hit-testing/input). The two must stay equal for clicks to land
// where stones are actually drawn — previously this was defined
// independently in board_renderer.cpp and board_view.cpp with nothing
// enforcing they matched (CLEAN-01). See UX-04 for the broader geometry
// unification this may eventually be folded into.

/// Pixel margin reserved around the board for coordinate labels.
/// UI-22: raised from 24 so the column labels no longer hug the header bar.
static constexpr double kCoordMargin = 38.0;
/// Gap between the wood edge and the coordinate labels.
static constexpr double kLabelGap = 10.0;
