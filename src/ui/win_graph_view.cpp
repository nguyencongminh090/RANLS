#include "win_graph_view.h"

#include "empty_state.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

static constexpr double kPadL = 40.0, kPadR = 10.0, kPadT = 10.0, kPadB = 20.0;
static constexpr double kBlackR = 0.20, kBlackG = 0.45, kBlackB = 0.85;
static constexpr double kWhiteR = 0.85, kWhiteG = 0.80, kWhiteB = 0.25;
static constexpr double kHighR = 0.85, kHighG = 0.20, kHighB = 0.20;

WinGraphView::WinGraphView()
{
    set_hexpand(true);
    set_vexpand(false);
    set_size_request(-1, 120);

    set_draw_func(sigc::mem_fun(*this, &WinGraphView::onDraw));

    // Click → jump to move.
    auto click = Gtk::GestureClick::create();
    click->set_button(GDK_BUTTON_PRIMARY);
    click->signal_released().connect(
        [this](int, double x, double) {
            if (blackData_.empty()) return;
            int w = get_width();
            double graphW = w - kPadL - kPadR;
            if (graphW <= 0) return;
            double step = graphW / std::max(1, static_cast<int>(blackData_.size()) - 1);
            int idx = std::clamp(static_cast<int>((x - kPadL) / step + 0.5),
                                 0, static_cast<int>(blackData_.size()) - 1);
            signal_move_jumped.emit(idx);
        });
    add_controller(click);

    // Hover → show tooltip.
    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_motion().connect(
        [this](double x, double) {
            if (blackData_.empty()) { hoverIndex_ = -1; queue_draw(); return; }
            int w = get_width();
            double graphW = w - kPadL - kPadR;
            if (graphW <= 0) return;
            double step = graphW / std::max(1, static_cast<int>(blackData_.size()) - 1);
            hoverIndex_ = std::clamp(static_cast<int>((x - kPadL) / step + 0.5),
                                     0, static_cast<int>(blackData_.size()) - 1);
            queue_draw();
        });
    motion->signal_leave().connect([this]() { hoverIndex_ = -1; queue_draw(); });
    add_controller(motion);
}

void WinGraphView::setData(const std::vector<double> &blackSeries,
                           const std::vector<double> &whiteSeries,
                           int currentMoveIndex,
                           WinGraphMode mode)
{
    blackData_    = blackSeries;
    whiteData_    = whiteSeries;
    currentIndex_ = currentMoveIndex;
    mode_         = mode;
    queue_draw();
}

void WinGraphView::onDraw(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height)
{
    double graphW = width  - kPadL - kPadR;
    double graphH = height - kPadT - kPadB;

    // Background is now handled by GTK+ CSS / Native theme.

    if (graphW <= 0 || graphH <= 0) return;

    // 50% center line.
    cr->set_source_rgba(0.4, 0.4, 0.4, 0.5);
    cr->set_line_width(1.0);
    cr->move_to(kPadL, kPadT + graphH * 0.5);
    cr->line_to(kPadL + graphW, kPadT + graphH * 0.5);
    cr->stroke();

    // Y-axis labels (0%, 50%, 100%). UX-03: a fixed mid-gray (0.5,0.5,0.5)
    // measures ~4.0:1 against both the light and dark Adwaita background --
    // just under the 4.5:1 text minimum, and no single fixed gray can clear
    // 4.5:1 against a near-white AND a near-black background at once. Unlike
    // the board (which paints its own fixed wood color and can't follow the
    // theme), this widget's background *does* track GTK theme via CSS, so
    // its text should track the theme's own foreground color too -- the
    // same mechanism Gtk::Label uses, and covered by the theme's own
    // contrast guarantees in both light and dark.
    Gdk::RGBA fg = get_color();
    cr->set_source_rgba(fg.get_red(), fg.get_green(), fg.get_blue(), fg.get_alpha());
    cr->set_font_size(9.0);
    cr->move_to(4, kPadT + 8);            cr->show_text("100%");
    cr->move_to(4, kPadT + graphH / 2 + 4); cr->show_text("50%");
    cr->move_to(4, kPadT + graphH - 2);    cr->show_text("0%");

    // UX-01: no series yet — the axis scaffold above already reads as an
    // empty chart rather than a void; add a centered placeholder explaining
    // what will populate it and stop (nothing further to plot).
    if (blackData_.empty()) {
        EmptyState::drawPlaceholder(cr, *this, width, height,
                                     "No analysis yet — press Analyze (F5)");
        return;
    }

    // Plot line.
    int n = static_cast<int>(blackData_.size());
    double step = (n > 1) ? graphW / (n - 1) : 0;

    // UI-01: an unevaluated position is encoded as NaN (see GameState::
    // evalHistory / toDisplayWinrate). Break the line into disjoint
    // segments around any NaN run instead of interpolating through it or
    // silently plotting it as a confident 50%.
    cr->set_source_rgb(kBlackR, kBlackG, kBlackB);
    cr->set_line_width(1.5);
    {
        bool penDown = false;
        for (int i = 0; i < n; ++i) {
            if (std::isnan(blackData_[i])) { penDown = false; continue; }
            double x = kPadL + i * step;
            double y = kPadT + graphH * (1.0 - std::clamp(blackData_[i], 0.0, 1.0));
            if (!penDown) { cr->move_to(x, y); penDown = true; }
            else          cr->line_to(x, y);
        }
        cr->stroke();
    }

    // UX-06: BothSide draws a second (White) perspective-correct line on top
    // of the Black line above. SingleSide passes an empty whiteData_ and only
    // the single Black-slot line is drawn. The size guard is kept so a
    // transient mismatch never indexes out of range.
    if (mode_ == WinGraphMode::BothSide && !whiteData_.empty()
        && whiteData_.size() == blackData_.size()) {
        // UX-03: the black/white series were told apart by hue alone
        // (blue vs. yellow). Dash the white series so the two lines also
        // differ by shape, not just color.
        cr->set_source_rgb(kWhiteR, kWhiteG, kWhiteB);
        cr->set_line_width(1.2);
        cr->set_dash(std::vector<double>{4.0, 3.0}, 0.0);
        bool penDown = false;
        for (int i = 0; i < n; ++i) {
            if (std::isnan(whiteData_[i])) { penDown = false; continue; }
            double x = kPadL + i * step;
            double y = kPadT + graphH * (1.0 - std::clamp(whiteData_[i], 0.0, 1.0));
            if (!penDown) { cr->move_to(x, y); penDown = true; }
            else          cr->line_to(x, y);
        }
        cr->stroke();
        cr->unset_dash();
    }

    // Current move highlight.
    if (currentIndex_ >= 0 && currentIndex_ < n) {
        double x = kPadL + currentIndex_ * step;
        // Vertical line.
        cr->set_source_rgba(kHighR, kHighG, kHighB, 0.6);
        cr->set_line_width(1.5);
        cr->move_to(x, kPadT);
        cr->line_to(x, kPadT + graphH);
        cr->stroke();

        // Dot — only for an actually-evaluated position; an unevaluated
        // current move gets no dot (its gap already reads as "no data").
        if (!std::isnan(blackData_[currentIndex_])) {
            double y = kPadT + graphH * (1.0 - std::clamp(blackData_[currentIndex_], 0.0, 1.0));
            cr->set_source_rgb(kBlackR, kBlackG, kBlackB);
            cr->arc(x, y, 4.0, 0, 2 * M_PI);
            cr->fill();
        }
    }

    // Hover tooltip.
    if (hoverIndex_ >= 0 && hoverIndex_ < n) {
        double x = kPadL + hoverIndex_ * step;
        bool hasEval = !std::isnan(blackData_[hoverIndex_]);
        double y = hasEval
                        ? kPadT + graphH * (1.0 - std::clamp(blackData_[hoverIndex_], 0.0, 1.0))
                        : kPadT + graphH * 0.5;

        // Vertical guideline.
        cr->set_source_rgba(0.7, 0.7, 0.7, 0.3);
        cr->set_line_width(1.0);
        cr->move_to(x, kPadT);
        cr->line_to(x, kPadT + graphH);
        cr->stroke();

        // Tooltip box.
        std::ostringstream tip;
        tip << "Move " << (hoverIndex_ + 1) << "  ";
        if (hasEval)
            tip << std::fixed << std::setprecision(1) << (blackData_[hoverIndex_] * 100.0) << "%";
        else
            tip << "(no eval)";
        std::string text = tip.str();

        cr->set_font_size(10.0);
        Cairo::TextExtents ext;
        cr->get_text_extents(text, ext);

        double tx = std::min(x + 6.0, width - ext.width - 10.0);
        double ty = std::max(y - 6.0, kPadT + ext.height + 4.0);

        cr->set_source_rgba(0.1, 0.1, 0.1, 0.85);
        cr->rectangle(tx - 3, ty - ext.height - 2, ext.width + 6, ext.height + 4);
        cr->fill();

        cr->set_source_rgb(0.9, 0.9, 0.9);
        cr->move_to(tx, ty);
        cr->show_text(text);
    }
}
