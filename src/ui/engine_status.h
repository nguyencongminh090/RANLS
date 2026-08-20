#pragma once

#include "model/game_state.h"

#include <gtkmm.h>

/// Displays live engine statistics + engine on/off state.
class EngineStatusView : public Gtk::Box {
public:
    EngineStatusView();

    /// Update displayed values from an EngineStatus snapshot.
    void update(const EngineStatus &status, const std::vector<PVLine> &pvLines = {});

    /// Set engine state indicator.
    void setEngineState(bool running);

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
