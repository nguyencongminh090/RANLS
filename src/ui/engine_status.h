#pragma once

#include "model/game_state.h"
#include "engine/engine_controller.h"

#include <gtkmm.h>

/// Displays live engine statistics + engine on/off state.
class EngineStatusView : public Gtk::Box {
public:
    EngineStatusView();

    /// Update displayed values from an EngineStatus snapshot.
    void update(const EngineStatus &status, const std::vector<PVLine> &pvLines = {});

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

private:
    Gtk::Label labelState_;
    Gtk::Button btnStart_;
    Gtk::Button btnStop_;
    Gtk::Button btnReload_;

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
