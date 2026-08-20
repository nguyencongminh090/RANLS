#include "pv_view.h"

#include <iomanip>
#include <sstream>

static std::string coordStr(Coord c, int boardSize)
{
    if (!c.isValid(boardSize)) return "?";
    return std::string(1, 'A' + c.x) + std::to_string(boardSize - c.y);
}

static std::string evalText(const PVLine &pv)
{
    std::ostringstream scoreStr;
    scoreStr << std::fixed << std::setprecision(1) << (pv.score * 100.0) << "%";
    const std::string wr = scoreStr.str();
    if (!pv.evalText.empty()) return pv.evalText + " / " + wr;
    if (pv.mateStep > 0) return "+M" + std::to_string(pv.mateStep) + " / " + wr;
    if (pv.mateStep < 0) return "-M" + std::to_string(-pv.mateStep) + " / " + wr;
    return wr;
}


PVView::PVView()
{
    add_css_class("pv-view");
    set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    set_vexpand(true);

    listBox_.set_selection_mode(Gtk::SelectionMode::NONE);
    set_child(listBox_);

    // When the mouse leaves the list box, emit the hover-left signal.
    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_leave().connect([this]() {
        signal_pv_hover_left.emit();
    });
    listBox_.add_controller(motion);
}

void PVView::update(const std::vector<PVLine> &pvLines, int boardSize)
{
    // Remove all existing rows.
    while (auto *child = listBox_.get_first_child()) {
        listBox_.remove(*child);
    }

    for (size_t i = 0; i < pvLines.size(); ++i) {
        const auto &pv = pvLines[i];

        auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);

        // PV index.
        int displayPvIndex = pv.pvIndex > 0 ? pv.pvIndex : static_cast<int>(i + 1);
        auto *idxLabel = Gtk::make_managed<Gtk::Label>("PV #" + std::to_string(displayPvIndex));
        idxLabel->add_css_class("pv-index");
        row->append(*idxLabel);

        // Score.
        auto *scoreLabel = Gtk::make_managed<Gtk::Label>(evalText(pv));
        scoreLabel->add_css_class("pv-score");
        row->append(*scoreLabel);

        // Depth.
        std::string depthText = "d" + std::to_string(pv.depth);
        if (pv.selDepth > 0) depthText += "/" + std::to_string(pv.selDepth);
        auto *depthLabel = Gtk::make_managed<Gtk::Label>(depthText);
        depthLabel->add_css_class("pv-score");
        row->append(*depthLabel);

        // Move sequence.
        std::string movesStr;
        for (size_t j = 0; j < pv.moves.size() && j < 12; ++j) {
            if (j > 0) movesStr += " → ";
            movesStr += coordStr(pv.moves[j], boardSize);
        }
        if (pv.moves.size() > 12) movesStr += " …";
        auto *movesLabel = Gtk::make_managed<Gtk::Label>(movesStr);
        movesLabel->add_css_class("pv-moves");
        movesLabel->set_ellipsize(Pango::EllipsizeMode::END);
        movesLabel->set_hexpand(true);
        movesLabel->set_halign(Gtk::Align::START);
        row->append(*movesLabel);

        listBox_.append(*row);

        // Hover → emit PV preview signal.
        auto motion = Gtk::EventControllerMotion::create();
        std::vector<Coord> pvMoves = pv.moves;
        motion->signal_enter().connect(
            [this, pvMoves](double, double) {
                signal_pv_hovered.emit(pvMoves);
            });
        row->add_controller(motion);
    }
}
