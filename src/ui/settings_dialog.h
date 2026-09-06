#pragma once

#include "model/game_state.h"

#include <gtkmm.h>

namespace settings_dialog_detail {

/// PORT-01: true if `path` ends in a Windows executable extension
/// (`.exe` / `.bat` / `.cmd` / `.com`), matched case-insensitively. Used on
/// `_WIN32` in place of the POSIX `access(path, X_OK)` execute-bit check, which
/// the Windows CRT cannot express (`_access` has no `X_OK` mode and the execute
/// bit is meaningless there). Pure string logic — identical on every platform,
/// so it is unit-tested directly.
bool hasExecutableExtension(const std::string &path);

} // namespace settings_dialog_detail

/// Settings dialog for engine configuration.
class SettingsDialog : public Gtk::Window {
public:
    explicit SettingsDialog(Gtk::Window &parent, const EngineConfig &eConfig, const ViewConfig &vConfig);

    /// Signal emitted when the user clicks Apply.
    sigc::signal<void(EngineConfig, ViewConfig)> signal_applied;

private:
    void onApply();
    void onChooseEngine();
    void onChoosePtc();
    void onEnginePathChanged();
    void updateApplySensitivity();

    // Snapshot of the config the dialog was opened with. onApply() starts
    // from these (not a fresh default-constructed struct) and mutates only
    // the fields the dialog exposes controls for, so any struct field the
    // dialog doesn't own (e.g. multiPV set via console command, customParams,
    // showDatabase) is preserved instead of silently reset to its default
    // (see STATE-02).
    EngineConfig        baseEngineConfig_;
    ViewConfig          baseViewConfig_;

    Gtk::Entry          entryEnginePath_;
    Gtk::Label          lblEnginePathStatus_;
    Gtk::Entry          entryPtcPath_;   ///< PROTO-03: optional .ptc extension file
    Gtk::Button        *btnApply_ = nullptr;
    bool                enginePathValid_ = false;
    Gtk::DropDown       dropTheme_;
    Gtk::DropDown       dropWinGraphMode_;
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
    Gtk::SpinButton     spinMultiPV_;
    Gtk::SpinButton     spinThreads_;
    Gtk::SpinButton     spinHash_;
};
