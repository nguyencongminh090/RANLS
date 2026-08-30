#include "board_view.h"
#include "board_geometry.h"

#include <algorithm>
#include <cmath>

BoardView::BoardView(BoardViewModel &viewModel)
    : vm_(viewModel)
    , renderer_(viewModel)
{
    set_hexpand(true);
    set_vexpand(true);

    // Drawing callback.
    set_draw_func(sigc::mem_fun(*this, &BoardView::onDraw));

    // ── Mouse click ─────────────────────────────────────────────────────────
    auto clickCtrl = Gtk::GestureClick::create();
    clickCtrl->set_button(GDK_BUTTON_PRIMARY);
    clickCtrl->signal_released().connect(
        [this](int /*nPress*/, double x, double y) {
            Coord c = pixelToCoord(x, y, get_width(), get_height());
            if (c.isValid(vm_.boardSize)) {
                signal_move_clicked.emit(c);
            }
        });
    add_controller(clickCtrl);

    // ── Mouse motion (hover) ────────────────────────────────────────────────
    auto motionCtrl = Gtk::EventControllerMotion::create();
    motionCtrl->signal_motion().connect(
        [this](double x, double y) {
            Coord c = pixelToCoord(x, y, get_width(), get_height());
            if (c != vm_.hoverMove) {
                vm_.hoverMove = c;
                queue_draw();
            }
        });
    motionCtrl->signal_leave().connect(
        [this]() {
            vm_.hoverMove = Coord{};
            queue_draw();
        });
    add_controller(motionCtrl);
}

void BoardView::queueRedraw()
{
    queue_draw();
}

void BoardView::onDraw(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height)
{
    // Background handled by GTK native theme transparently.

    // UX-06: hand the renderer the widget's themed foreground colour so the
    // coordinate labels (drawn on the widget background, outside the wood
    // board) stay legible in both light and dark themes.
    Gdk::RGBA fg = get_color();
    renderer_.setCoordinateColor(fg.get_red(), fg.get_green(), fg.get_blue());

    renderer_.draw(cr, width, height);
}

Coord BoardView::pixelToCoord(double px, double py, int width, int height) const
{
    int bs = vm_.boardSize;
    if (bs <= 0) return {};

    // UX-04: geometry comes from the same BoardRenderer::computeGeometry()
    // call that draw() uses, so hit-testing can never silently disagree with
    // rendering (previously this function kept its own copy of the
    // cell-size/margin formula and its own kCoordMargin constant).
    BoardRenderer::Geometry geo = renderer_.computeGeometry(width, height);
    if (geo.cellSize <= 0.0) return {};

    int x = static_cast<int>((px - geo.marginLeft) / geo.cellSize);
    int y = static_cast<int>((py - geo.marginTop)  / geo.cellSize);

    if (x >= 0 && y >= 0 && x < bs && y < bs)
        return {x, y};
    return {};
}
