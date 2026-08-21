#include "engine_status.h"

#include <iomanip>
#include <sstream>

static std::string formatNodes(int64_t n)
{
    if (n >= 1'000'000'000) return std::to_string(n / 1'000'000'000) + "." + std::to_string((n / 100'000'000) % 10) + "B";
    if (n >= 1'000'000)     return std::to_string(n / 1'000'000) + "." + std::to_string((n / 100'000) % 10) + "M";
    if (n >= 1'000)         return std::to_string(n / 1'000) + "." + std::to_string((n / 100) % 10) + "K";
    return std::to_string(n);
}

static std::string formatTime(int64_t ms)
{
    if (ms < 1000)  return std::to_string(ms) + "ms";
    if (ms < 60000) {
        std::ostringstream s;
        s << std::fixed << std::setprecision(1) << (ms / 1000.0) << "s";
        return s.str();
    }
    int sec = static_cast<int>(ms / 1000);
    return std::to_string(sec / 60) + "m " + std::to_string(sec % 60) + "s";
}

static std::string coordStr(Coord c, int boardSize)
{
    if (!c.isValid(boardSize)) return "-";
    return std::string(1, 'A' + c.x) + std::to_string(boardSize - c.y);
}

static std::string evalSummary(double winrate, int mateStep, const std::string &evalText)
{
    std::ostringstream wrStr;
    wrStr << std::fixed << std::setprecision(1) << (winrate * 100.0) << "%";
    if (!evalText.empty()) {
        return evalText + " / " + wrStr.str();
    }
    if (mateStep > 0) {
        return "+M" + std::to_string(mateStep) + " / " + wrStr.str();
    }
    if (mateStep < 0) {
        return "-M" + std::to_string(-mateStep) + " / " + wrStr.str();
    }
    return wrStr.str();
}

// ═════════════════════════════════════════════════════════════════════════════
EngineStatusView::EngineStatusView()
    : Gtk::Box(Gtk::Orientation::HORIZONTAL, 8)
{
    add_css_class("engine-status");

    // ── Engine state indicator + controls ────────────────────────────────────
    auto *stateBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
    stateBox->add_css_class("linked");

    labelState_.set_text("● OFF");
    labelState_.add_css_class("engine-off");
    labelState_.set_margin_end(4);

    btnStart_.set_label("▶");
    btnStart_.set_tooltip_text("Start Engine");
    btnStart_.signal_clicked().connect([this]() { signal_start.emit(); });

    btnStop_.set_label("■");
    btnStop_.set_tooltip_text("Stop Engine");
    btnStop_.signal_clicked().connect([this]() { signal_stop.emit(); });

    btnReload_.set_label("↻");
    btnReload_.set_tooltip_text("Reload Engine");
    btnReload_.signal_clicked().connect([this]() { signal_reload.emit(); });

    stateBox->append(labelState_);
    stateBox->append(btnStart_);
    stateBox->append(btnStop_);
    stateBox->append(btnReload_);
    append(*stateBox);

    // ── Separator ────────────────────────────────────────────────────────────
    auto *sep = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::VERTICAL);
    sep->set_margin_start(4);
    sep->set_margin_end(4);
    append(*sep);

    // ── Stats ────────────────────────────────────────────────────────────────
    auto addPair = [this](Gtk::Label &key, Gtk::Label &val, const char *name) {
        key.set_text(name);
        key.add_css_class("label-key");
        val.add_css_class("label-value");
        val.set_text("-");

        auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
        box->append(key);
        box->append(val);
        append(*box);
    };

    addPair(labelDepth_, valueDepth_, "D:");
    addPair(labelNodes_, valueNodes_, "N:");
    addPair(labelNPS_,   valueNPS_,   "NPS:");
    addPair(labelTime_,  valueTime_,  "T:");
    addPair(labelEval_,  valueEval_,  "Eval:");
    addPair(labelBest_,  valueBest_,  "Best:");
}

void EngineStatusView::update(const EngineStatus &s, const std::vector<PVLine> &pvLines, int boardSize)
{
    const PVLine *bestPv = !pvLines.empty() && !pvLines[0].moves.empty() ? &pvLines[0] : nullptr;

    std::string depthText = std::to_string(s.depth);
    if (s.selDepth > 0) depthText += "/" + std::to_string(s.selDepth);
    valueDepth_.set_text(depthText);
    valueNodes_.set_text(formatNodes(s.nodes));
    valueNPS_.set_text(formatNodes(s.nps) + "/s");
    valueTime_.set_text(formatTime(s.timeMs));

    valueEval_.set_text(bestPv ? evalSummary(bestPv->score, bestPv->mateStep, bestPv->evalText)
                               : evalSummary(s.winrate, s.mateStep, s.evalText));

    Coord bestMove = bestPv ? bestPv->moves.front() : s.bestMove;
    valueBest_.set_text(coordStr(bestMove, boardSize));
}

void EngineStatusView::setEngineState(EngineController::EngineState state)
{
    static const char *kAllClasses[] = {
        "engine-off", "engine-starting", "engine-on", "engine-thinking",
        "engine-stopping", "engine-crashed",
    };
    for (const char *cls : kAllClasses) labelState_.remove_css_class(cls);

    switch (state) {
        case EngineController::EngineState::NotStarted:
            labelState_.set_text("● OFF");
            labelState_.add_css_class("engine-off");
            break;
        case EngineController::EngineState::Starting:
            labelState_.set_text("● STARTING");
            labelState_.add_css_class("engine-starting");
            break;
        case EngineController::EngineState::Idle:
            labelState_.set_text("● ON");
            labelState_.add_css_class("engine-on");
            break;
        case EngineController::EngineState::Analyzing:
            labelState_.set_text("● THINKING");
            labelState_.add_css_class("engine-thinking");
            break;
        case EngineController::EngineState::Stopping:
            labelState_.set_text("● STOPPING");
            labelState_.add_css_class("engine-stopping");
            break;
        case EngineController::EngineState::Crashed:
            labelState_.set_text("● CRASHED");
            labelState_.add_css_class("engine-crashed");
            signal_crashed.emit();
            break;
    }
}
