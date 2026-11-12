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
    // UI-23: the board can never be squeezed to nothing (see kBoardMinPx).
    set_size_request(kBoardMinPx, kBoardMinPx);

    // PROTO-09: draw the eased overlay, not the raw one.
    renderer_.setAnimatedSearchOverlay(&overlayAnim_.shown());
    signal_unmap().connect([this]() { stopAnimationTick(); overlayAnim_.snap(); });

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

    // ── Tooltip (UX-08) ─────────────────────────────────────────────────────
    // Text is built by the view model; empty -> no tooltip. x/y are
    // widget-relative, same space as the mouse controllers above.
    set_has_tooltip(true);
    signal_query_tooltip().connect(
        [this](int x, int y, bool /*keyboard*/, const Glib::RefPtr<Gtk::Tooltip> &tooltip) {
            Coord c = pixelToCoord(x, y, get_width(), get_height());
            if (!c.isValid(vm_.boardSize)) return false;
            std::string text = vm_.tooltipFor(c);
            if (text.empty()) return false;
            tooltip->set_text(text);
            return true;
        },
        false);
}

void BoardView::queueRedraw()
{
    overlayAnim_.setTarget(vm_.searchOverlay);
    // Nothing may linger over a stone that was just played on a tagged cell.
    overlayAnim_.dropFading([this](Coord c) { return vm_.isOccupied(c); });

    // Ease only on screen and only if the desktop allows animations (a11y
    // "reduce motion"); otherwise show the new state at once.
    bool animate = get_mapped();
    if (animate) {
        if (auto settings = get_settings())
            animate = settings->property_gtk_enable_animations().get_value();
    }
    if (!animate) {
        stopAnimationTick();
        overlayAnim_.snap();
    } else if (overlayAnim_.animating() && animTickId_ == 0) {
        animLastFrame_ = 0;
        animTickId_    = add_tick_callback(sigc::mem_fun(*this, &BoardView::onAnimationTick));
    }
    queue_draw();
}

bool BoardView::onAnimationTick(const Glib::RefPtr<Gdk::FrameClock> &clock)
{
    const gint64 now = clock->get_frame_time();
    // First tick has no previous frame; a long stall (window hidden, debugger)
    // must not be replayed as one giant step either.
    const double dt = animLastFrame_ == 0 ? 0.0 : std::min((now - animLastFrame_) / 1e6, 0.1);
    animLastFrame_ = now;

    const bool more = overlayAnim_.advance(dt);
    queue_draw();
    if (!more) {
        animTickId_ = 0;
        return false;   // G_SOURCE_REMOVE
    }
    return true;
}

void BoardView::stopAnimationTick()
{
    if (animTickId_ != 0) {
        remove_tick_callback(animTickId_);
        animTickId_ = 0;
    }
}

void BoardView::onDraw(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height)
{
    // Background handled by GTK native theme transparently.

    // UX-06: hand the renderer the widget's themed foreground colour so the
    // coordinate labels (drawn on the widget background, outside the wood
    // board) stay legible in both light and dark themes.
    Gdk::RGBA fg = get_color();
    renderer_.setCoordinateColor(fg.get_red(), fg.get_green(), fg.get_blue());
    // UI-23: coordinate labels in the same family as the rest of the UI.
    renderer_.setLabelFontFamily(get_pango_context()->get_font_description().get_family());

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
