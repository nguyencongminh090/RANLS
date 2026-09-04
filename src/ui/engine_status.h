#pragma once

#include "model/game_state.h"
#include "engine/engine_controller.h"

#include <gtkmm.h>

/// Displays live engine statistics + engine on/off state.
class EngineStatusView : public Gtk::Box {
public:
    EngineStatusView();

    /// Update displayed values from an EngineStatus snapshot. `boardSize`
    /// must be the real current board size (PROTO-02) -- it drives the
    /// "Best:" coordinate label, which is otherwise wrong on any non-15
    /// board.
    void update(const EngineStatus &status, const std::vector<PVLine> &pvLines, int boardSize);

    /// Set engine state indicator. Renders not-started / starting / idle /
    /// thinking / stopping / crashed as visibly distinct states (ENG-01) —
    /// crashed is additionally announced via signal_crashed so the caller can
    /// raise an active notification (toast/banner), not just flip a label.
    void setEngineState(EngineController::EngineState state);

    /// Emitted whenever setEngineState() is called with EngineState::Crashed.
    /// No libadwaita is linked in this build (see CMakeLists.txt), so the
    /// active crash announcement is an inline banner in the analysis panel
    /// rather than an Adw::Toast — see AnalysisPanel::showEngineCrashBanner().
    sigc::signal<void()> signal_crashed;

    /// Signal emitted when the user clicks Start / Stop / Reload.
    sigc::signal<void()> signal_start;
    sigc::signal<void()> signal_stop;
    sigc::signal<void()> signal_reload;

    /// ANLZ-01: emitted when the user toggles the "Auto" (Analyze Mode) button.
    /// Carries the new desired state. Not emitted by setAnalyzeModeActive().
    sigc::signal<void(bool)> signal_analyze_mode_toggled;

    /// ANLZ-01: reflect the persisted / externally-changed Analyze Mode state
    /// onto the toggle button without firing signal_analyze_mode_toggled.
    void setAnalyzeModeActive(bool active);

private:
    Gtk::Label labelState_;
    Gtk::Button btnStart_;
    Gtk::Button btnStop_;
    Gtk::Button btnReload_;
    Gtk::ToggleButton btnAnalyzeMode_;
    bool suppressAnalyzeModeSignal_ = false;

    Gtk::Label labelDepth_;
    Gtk::Label labelNodes_;
    Gtk::Label labelNPS_;
    Gtk::Label labelTime_;
    Gtk::Label labelEval_;
    Gtk::Label labelBest_;
    Gtk::Label valueDepth_;
    Gtk::Label valueNodes_;
    Gtk::Label valueNPS_;
    Gtk::Label valueTime_;
    Gtk::Label valueEval_;
    Gtk::Label valueBest_;
};
