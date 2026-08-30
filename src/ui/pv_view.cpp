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
    overlay_.setContent(listBox_);
    overlay_.setEmpty(true);  // No PV rows yet at construction.
    set_child(overlay_);

    // When the mouse leaves the list box, emit the hover-left signal.
    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_leave().connect([this]() {
        signal_pv_hover_left.emit();
    });
    listBox_.add_controller(motion);
}

void PVView::update(const std::vector<PVLine> &pvLinesIn, int boardSize)
{
    // Never expose a PV slot with no moves (STATE-03): currentPVs_ can still
    // carry a default-constructed filler entry (e.g. an index the engine
    // jumped ahead of within the current round) even after the source-side
    // fix in GomocupProtocol. Filtering here means a row like
    // "PV #2  50.0%  d0" with nothing after it can never render, and the
    // row count always agrees with BoardViewModel's own `!moves.empty()`
    // filter for the board's candidate markers.
    std::vector<PVLine> pvLines;
    pvLines.reserve(pvLinesIn.size());
    for (const auto &pv : pvLinesIn) {
        if (!pv.moves.empty()) pvLines.push_back(pv);
    }

    const size_t newCount = pvLines.size();

    // UX-01: show/hide the placeholder as the row count crosses zero. This
    // reacts to whatever cleared pvLinesIn back to empty (New Game/undo via
    // STATE-01, or simply no analysis run yet) without duplicating that
    // clearing logic here.
    overlay_.setEmpty(newCount == 0);

    // Only remove/add row widgets when the PV *count* changes (RT-03): row
    // widgets — and the Gtk::EventControllerMotion attached to each — must
    // stay alive across a content-only refresh, or an in-progress hover gets
    // torn down (synthetic `leave`) and the replacement widget under a
    // stationary pointer never receives a fresh `enter`.

    // Shrink: drop rows from the tail.
    //
    // UI-07: remove the GtkListBoxRow, not the Box inside it. Gtk::ListBox
    // wraps a non-row child in an implicitly-created GtkListBoxRow on
    // append(), so the Box is the list box's *grandchild*; passing it to
    // remove() makes GTK 4 log "Tried to remove non-child" and do nothing.
    // That left one orphaned row widget on screen per clear — the reported
    // "PV panel accumulates a stale row per analysed position" — while
    // rows_.size() happily went to zero and every model-layer assertion
    // stayed green. `listRow` is now created explicitly (below) so this
    // removal targets a real child of listBox_.
    while (rows_.size() > newCount) {
        listBox_.remove(*rows_.back().listRow);
        rows_.pop_back();
    }

    // Grow: append new rows (with their motion controller) for the tail.
    while (rows_.size() < newCount) {
        const size_t i = rows_.size();

        auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);

        auto *idxLabel = Gtk::make_managed<Gtk::Label>();
        idxLabel->add_css_class("pv-index");
        row->append(*idxLabel);

        auto *scoreLabel = Gtk::make_managed<Gtk::Label>();
        scoreLabel->add_css_class("pv-score");
        row->append(*scoreLabel);

        auto *depthLabel = Gtk::make_managed<Gtk::Label>();
        depthLabel->add_css_class("pv-score");
        row->append(*depthLabel);

        auto *movesLabel = Gtk::make_managed<Gtk::Label>();
        movesLabel->add_css_class("pv-moves");
        movesLabel->set_ellipsize(Pango::EllipsizeMode::END);
        movesLabel->set_hexpand(true);
        movesLabel->set_halign(Gtk::Align::START);
        row->append(*movesLabel);

        // UI-07: wrap explicitly instead of relying on Gtk::ListBox's implicit
        // wrapping, so the shrink path above has a real child to remove.
        auto *listRow = Gtk::make_managed<Gtk::ListBoxRow>();
        listRow->set_child(*row);
        listBox_.append(*listRow);

        // Hover → emit PV preview signal. Looks up the row's *current* moves
        // by index at hover time (rather than capturing them by value here)
        // so a later content-only refresh of this same row can't leave a
        // stale PV attached to an in-progress hover.
        auto motion = Gtk::EventControllerMotion::create();
        motion->signal_enter().connect(
            [this, i](double, double) {
                if (i < rows_.size())
                    signal_pv_hovered.emit(rows_[i].moves);
            });
        row->add_controller(motion);

        rows_.push_back({listRow, row, idxLabel, scoreLabel, depthLabel, movesLabel, {}});
    }

    // Update content in place for every row (existing rows are reused as-is;
    // newly appended rows get their initial content here too).
    for (size_t i = 0; i < newCount; ++i) {
        const auto &pv = pvLines[i];
        auto &rw = rows_[i];

        int displayPvIndex = pv.pvIndex > 0 ? pv.pvIndex : static_cast<int>(i + 1);
        rw.idxLabel->set_text("PV #" + std::to_string(displayPvIndex));
        rw.scoreLabel->set_text(evalText(pv));

        std::string depthText = "d" + std::to_string(pv.depth);
        if (pv.selDepth > 0) depthText += "/" + std::to_string(pv.selDepth);
        rw.depthLabel->set_text(depthText);

        std::string movesStr;
        for (size_t j = 0; j < pv.moves.size() && j < 12; ++j) {
            if (j > 0) movesStr += " → ";
            movesStr += coordStr(pv.moves[j], boardSize);
        }
        if (pv.moves.size() > 12) movesStr += " …";
        rw.movesLabel->set_text(movesStr);

        rw.moves = pv.moves;
    }
}
