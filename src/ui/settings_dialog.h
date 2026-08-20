#pragma once

#include "model/game_state.h"

#include <gtkmm.h>

/// Settings dialog for engine configuration.
class SettingsDialog : public Gtk::Window {
public:
    explicit SettingsDialog(Gtk::Window &parent, const EngineConfig &eConfig, const ViewConfig &vConfig);

    /// Signal emitted when the user clicks Apply.
    sigc::signal<void(EngineConfig, ViewConfig)> signal_applied;

private:
    void onApply();
    void onChooseEngine();

    Gtk::Entry          entryEnginePath_;
    Gtk::DropDown       dropTheme_;
    Gtk::DropDown       dropWinGraphMode_;
    Gtk::DropDown       dropProfile_;
    Gtk::CheckButton    checkMoveNumbers_;
    Gtk::CheckButton    checkCoordinates_;
    Gtk::Entry          entryHotkeyAnalyze_;
    Gtk::Entry          entryHotkeyStop_;
    Gtk::Entry          entryHotkeyUndo_;
    Gtk::Entry          entryHotkeyRedo_;
    Gtk::Entry          entryHotkeyNewGame_;
    Gtk::SpinButton     spinTimeoutTurn_;
    Gtk::SpinButton     spinTimeoutMatch_;
    Gtk::SpinButton     spinIncrement_;
    Gtk::SpinButton     spinMaxDepth_;
    Gtk::SpinButton     spinMaxNodes_;
    Gtk::SpinButton     spinThreads_;
    Gtk::SpinButton     spinHash_;
};
