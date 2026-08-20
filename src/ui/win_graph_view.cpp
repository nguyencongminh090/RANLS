#include "win_graph_view.h"

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

    if (blackData_.empty() || graphW <= 0 || graphH <= 0) return;

    // 50% center line.
    cr->set_source_rgba(0.4, 0.4, 0.4, 0.5);
    cr->set_line_width(1.0);
    cr->move_to(kPadL, kPadT + graphH * 0.5);
    cr->line_to(kPadL + graphW, kPadT + graphH * 0.5);
    cr->stroke();

    // Y-axis labels (0%, 50%, 100%).
    cr->set_source_rgb(0.5, 0.5, 0.5);
    cr->set_font_size(9.0);
    cr->move_to(4, kPadT + 8);            cr->show_text("100%");
    cr->move_to(4, kPadT + graphH / 2 + 4); cr->show_text("50%");
    cr->move_to(4, kPadT + graphH - 2);    cr->show_text("0%");

    // Plot line.
    int n = static_cast<int>(blackData_.size());
    double step = (n > 1) ? graphW / (n - 1) : 0;

    cr->set_source_rgb(kBlackR, kBlackG, kBlackB);
    cr->set_line_width(1.5);
    for (int i = 0; i < n; ++i) {
        double x = kPadL + i * step;
        double y = kPadT + graphH * (1.0 - std::clamp(blackData_[i], 0.0, 1.0));
        if (i == 0) cr->move_to(x, y);
        else        cr->line_to(x, y);
    }
    cr->stroke();

    if (mode_ == WinGraphMode::SingleSide && whiteData_.size() == blackData_.size()) {
        cr->set_source_rgb(kWhiteR, kWhiteG, kWhiteB);
        cr->set_line_width(1.2);
        for (int i = 0; i < n; ++i) {
            double x = kPadL + i * step;
            double y = kPadT + graphH * (1.0 - std::clamp(whiteData_[i], 0.0, 1.0));
            if (i == 0) cr->move_to(x, y);
            else        cr->line_to(x, y);
        }
        cr->stroke();
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

        // Dot.
        double y = kPadT + graphH * (1.0 - std::clamp(blackData_[currentIndex_], 0.0, 1.0));
        cr->set_source_rgb(kBlackR, kBlackG, kBlackB);
        cr->arc(x, y, 4.0, 0, 2 * M_PI);
        cr->fill();
    }

    // Hover tooltip.
    if (hoverIndex_ >= 0 && hoverIndex_ < n) {
        double x = kPadL + hoverIndex_ * step;
        double y = kPadT + graphH * (1.0 - std::clamp(blackData_[hoverIndex_], 0.0, 1.0));

        // Vertical guideline.
        cr->set_source_rgba(0.7, 0.7, 0.7, 0.3);
        cr->set_line_width(1.0);
        cr->move_to(x, kPadT);
        cr->line_to(x, kPadT + graphH);
        cr->stroke();

        // Tooltip box.
        std::ostringstream tip;
        tip << "Move " << (hoverIndex_ + 1) << "  "
            << std::fixed << std::setprecision(1) << (blackData_[hoverIndex_] * 100.0) << "%";
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
