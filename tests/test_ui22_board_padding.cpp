// UI-22: the coordinate labels must keep clear space from the widget edges
// (previously 24px reserve left the column letters hugging the header bar).
#include "vendor/doctest.h"

#include "model/board_view_model.h"
#include "model/game_state.h"
#include "ui/board_geometry.h"
#include "ui/board_renderer.h"

TEST_CASE("UI-22: board keeps coordinate-label padding on every side") {
    GameState gs{15};
    BoardViewModel vm{gs};
    BoardRenderer r(vm);
    for (int size : {500, 800}) {
        auto g = r.computeGeometry(size, size);
        CHECK(g.marginTop  >= kCoordMargin - 1.0);
        CHECK(g.marginLeft >= kCoordMargin - 1.0);
        CHECK(g.marginTop  >= 36.0);   // room for a 16px label + gap + breathing space
    }
}
