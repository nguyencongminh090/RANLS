#include "settings_dialog.h"

#include <filesystem>
#include <memory>
#include <unistd.h>

namespace {

/// Checks whether `path` is usable as an engine executable: exists, is a
/// regular file (not a directory/socket/etc.), and has execute permission
/// for the current user. Returns false with a human-readable `reason` set
/// on the first failing check.
bool isValidEnginePath(const std::string &path, std::string &reason)
{
    if (path.empty()) {
        reason = "Path is empty";
        return false;
    }
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        reason = "Path does not exist";
        return false;
    }
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        reason = "Not a regular file";
        return false;
    }
    if (access(path.c_str(), X_OK) != 0) {
        reason = "Not executable";
        return false;
    }
    return true;
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
SettingsDialog::SettingsDialog(Gtk::Window &parent, const EngineConfig &eConfig, const ViewConfig &vConfig)
    : baseEngineConfig_(eConfig), baseViewConfig_(vConfig)
{
    set_title("Settings");
    set_transient_for(parent);
    set_modal(true);
    set_default_size(480, 520);
    // UX-06: the dialog was a single ~25-row flat grid, fixed-size. It is now
    // a tabbed Gtk::Notebook (Engine · Time · Search · UI · Hotkeys), each tab
    // scrollable, and the window is resizable.
    set_resizable(true);

    auto *root = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    root->set_margin(16);

    auto *notebook = Gtk::make_managed<Gtk::Notebook>();
    notebook->set_vexpand(true);

    // Build one tab: a grid of label/widget rows wrapped in a ScrolledWindow.
    struct Tab {
        Gtk::Grid *grid;
        int        row = 0;
    };
    auto makeTab = [&](const char *title) -> std::shared_ptr<Tab> {
        auto *grid = Gtk::make_managed<Gtk::Grid>();
        grid->set_row_spacing(8);
        grid->set_column_spacing(12);
        grid->set_margin(12);
        auto *scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
        scroller->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
        scroller->set_child(*grid);
        scroller->set_vexpand(true);
        notebook->append_page(*scroller, title);
        auto t = std::make_shared<Tab>();
        t->grid = grid;
        return t;
    };
    auto addRow = [](const std::shared_ptr<Tab> &t, const char *label, Gtk::Widget &widget,
                     const char *tooltip = nullptr) {
        auto *lbl = Gtk::make_managed<Gtk::Label>(label);
        lbl->set_halign(Gtk::Align::END);
        t->grid->attach(*lbl, 0, t->row, 1, 1);
        widget.set_hexpand(true);
        t->grid->attach(widget, 1, t->row, 1, 1);
        if (tooltip) {
            lbl->set_tooltip_text(tooltip);
            widget.set_tooltip_text(tooltip);
        }
        t->row++;
    };

    auto engineTab  = makeTab("Engine");
    auto timeTab    = makeTab("Time");
    auto searchTab  = makeTab("Search");
    auto uiTab      = makeTab("UI");
    auto hotkeysTab = makeTab("Hotkeys");

    // ── Engine tab ──────────────────────────────────────────────────────────
    auto *pathBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
    entryEnginePath_.set_text(eConfig.enginePath);
    entryEnginePath_.set_hexpand(true);
    auto *btnBrowse = Gtk::make_managed<Gtk::Button>("Browse…");
    btnBrowse->signal_clicked().connect(sigc::mem_fun(*this, &SettingsDialog::onChooseEngine));
    pathBox->append(entryEnginePath_);
    pathBox->append(*btnBrowse);
    pathBox->set_hexpand(true);

    // Inline validation feedback shown right under the field itself (not a
    // dialog popup on Apply, not console-only) — see UX-02.
    lblEnginePathStatus_.set_halign(Gtk::Align::START);
    lblEnginePathStatus_.set_xalign(0.0f);
    lblEnginePathStatus_.add_css_class("dim-label");
    entryEnginePath_.signal_changed().connect(sigc::mem_fun(*this, &SettingsDialog::onEnginePathChanged));

    auto *pathContainer = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
    pathContainer->append(*pathBox);
    pathContainer->append(lblEnginePathStatus_);
    addRow(engineTab, "Engine Path", *pathContainer,
           "Path to the Gomocup/Yixin-protocol engine executable.");

    spinThreads_.set_adjustment(Gtk::Adjustment::create(eConfig.threads, 1, 256, 1));
    spinThreads_.set_digits(0);
    addRow(engineTab, "Threads", spinThreads_, "Number of search threads the engine may use.");

    spinHash_.set_adjustment(Gtk::Adjustment::create(eConfig.hashSizeMB, 1, 65536, 64));
    spinHash_.set_digits(0);
    addRow(engineTab, "Hash Size (MB)", spinHash_, "Transposition-table size in megabytes.");

    // ── Time tab ────────────────────────────────────────────────────────────
    spinTimeoutTurn_.set_adjustment(Gtk::Adjustment::create(eConfig.timeoutTurn, 0, 6000000, 1000));
    spinTimeoutTurn_.set_digits(0);
    addRow(timeTab, "Timeout / Turn (ms)", spinTimeoutTurn_, "Milliseconds allowed per move. 0 = unlimited.");

    spinTimeoutMatch_.set_adjustment(Gtk::Adjustment::create(eConfig.timeoutMatch, 0, 99000000, 10000));
    spinTimeoutMatch_.set_digits(0);
    addRow(timeTab, "Timeout / Match (ms)", spinTimeoutMatch_, "Total milliseconds for the whole game. 0 = unlimited.");

    spinIncrement_.set_adjustment(Gtk::Adjustment::create(eConfig.increment, 0, 60000, 500));
    spinIncrement_.set_digits(0);
    addRow(timeTab, "Increment (ms)", spinIncrement_, "Time added back to the clock after each move.");

    // ── Search tab ──────────────────────────────────────────────────────────
    spinMaxDepth_.set_adjustment(Gtk::Adjustment::create(eConfig.maxDepth, 1, 225, 1));
    spinMaxDepth_.set_digits(0);
    addRow(searchTab, "Max Depth", spinMaxDepth_, "Maximum search depth in plies.");

    spinMaxNodes_.set_adjustment(Gtk::Adjustment::create(eConfig.maxNodes, 0, 100000000000, 1000000));
    spinMaxNodes_.set_digits(0);
    addRow(searchTab, "Max Nodes", spinMaxNodes_, "Node budget per search. 0 = unlimited.");

    spinMultiPV_.set_adjustment(Gtk::Adjustment::create(eConfig.multiPV, 1, 20, 1));
    spinMultiPV_.set_digits(0);
    addRow(searchTab, "Multi PV", spinMultiPV_, "Number of principal variations the engine reports.");

    // ── UI tab ──────────────────────────────────────────────────────────────
    auto themeModel = Gtk::StringList::create({"System", "Light", "Dark"});
    dropTheme_.set_model(themeModel);
    dropTheme_.set_selected(static_cast<guint>(vConfig.theme));
    addRow(uiTab, "Theme", dropTheme_,
           "System follows your desktop setting; Light / Dark force it.");

    auto modeModel = Gtk::StringList::create(
        {"Single line (engine's perspective)", "Two lines (Black & White)"});
    dropWinGraphMode_.set_model(modeModel);
    dropWinGraphMode_.set_selected(static_cast<guint>(vConfig.winGraphMode));
    addRow(uiTab, "WinGraph Mode", dropWinGraphMode_,
           "Single line: one perspective-correct win-rate (Black, or the engine's "
           "side when 'Engine plays' is set). Two lines: Black and White win-rate "
           "each in its own perspective.");

    checkMoveNumbers_.set_active(vConfig.showMoveNumbers);
    addRow(uiTab, "Show Move Numbers", checkMoveNumbers_, "Draw the move ordinal on each stone.");

    checkCoordinates_.set_active(vConfig.showCoordinates);
    addRow(uiTab, "Show Coordinates", checkCoordinates_, "Draw A–O column and 1–N row labels around the board.");

    // ── Hotkeys tab ─────────────────────────────────────────────────────────
    entryHotkeyAnalyze_.set_text(vConfig.hotkeyAnalyze);
    addRow(hotkeysTab, "Analyze", entryHotkeyAnalyze_, "e.g. F5 or Ctrl+A");
    entryHotkeyStop_.set_text(vConfig.hotkeyStop);
    addRow(hotkeysTab, "Stop", entryHotkeyStop_, "e.g. Escape");
    entryHotkeyUndo_.set_text(vConfig.hotkeyUndo);
    addRow(hotkeysTab, "Undo", entryHotkeyUndo_, "e.g. Ctrl+Z");
    entryHotkeyRedo_.set_text(vConfig.hotkeyRedo);
    addRow(hotkeysTab, "Redo", entryHotkeyRedo_, "e.g. Ctrl+Y");
    entryHotkeyNewGame_.set_text(vConfig.hotkeyNewGame);
    addRow(hotkeysTab, "New Game", entryHotkeyNewGame_, "e.g. Ctrl+N");

    root->append(*notebook);

    // ── Buttons ─────────────────────────────────────────────────────────────
    auto *btnBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    btnBox->set_halign(Gtk::Align::END);
    btnBox->set_margin_top(12);

    auto *btnCancel = Gtk::make_managed<Gtk::Button>("Cancel");
    auto *btnApply  = Gtk::make_managed<Gtk::Button>("Apply");
    btnApply->add_css_class("suggested-action");
    btnApply_ = btnApply;

    btnCancel->signal_clicked().connect([this]() { close(); });
    btnApply->signal_clicked().connect(sigc::mem_fun(*this, &SettingsDialog::onApply));

    btnBox->append(*btnCancel);
    btnBox->append(*btnApply);
    root->append(*btnBox);
    set_child(*root);

    // Run the initial validation pass now that entryEnginePath_ has its
    // starting text and btnApply_ exists to be (in)sensitized.
    onEnginePathChanged();
}

void SettingsDialog::onEnginePathChanged()
{
    std::string path = entryEnginePath_.get_text();
    std::string reason;
    enginePathValid_ = isValidEnginePath(path, reason);

    if (enginePathValid_) {
        lblEnginePathStatus_.set_text("✓ Executable found");
        lblEnginePathStatus_.remove_css_class("error");
        entryEnginePath_.remove_css_class("error");
    } else {
        lblEnginePathStatus_.set_text("✗ " + reason);
        lblEnginePathStatus_.add_css_class("error");
        entryEnginePath_.add_css_class("error");
    }
    updateApplySensitivity();
}

void SettingsDialog::updateApplySensitivity()
{
    if (btnApply_)
        btnApply_->set_sensitive(enginePathValid_);
}

void SettingsDialog::onApply()
{
    // Defense in depth: btnApply_ is desensitized while the engine path is
    // invalid, but guard here too in case Apply is ever reachable another
    // way (e.g. a future default-activation binding on the entry).
    if (!enginePathValid_)
        return;

    // Start from the config the dialog was opened with (not a fresh
    // default-constructed struct) so any field the dialog doesn't expose a
    // control for — multiPV set via console command, customParams, and
    // ViewConfig::showDatabase — is preserved rather than silently reset to
    // its struct default (STATE-02). Only the fields the controls below own
    // are overwritten.
    EngineConfig eConfig = baseEngineConfig_;
    eConfig.enginePath   = entryEnginePath_.get_text();
    eConfig.timeoutTurn  = static_cast<int64_t>(spinTimeoutTurn_.get_value());
    eConfig.timeoutMatch = static_cast<int64_t>(spinTimeoutMatch_.get_value());
    eConfig.increment    = static_cast<int>(spinIncrement_.get_value());
    eConfig.maxDepth     = static_cast<int>(spinMaxDepth_.get_value());
    eConfig.maxNodes     = static_cast<int64_t>(spinMaxNodes_.get_value());
    eConfig.threads      = static_cast<int>(spinThreads_.get_value());
    eConfig.hashSizeMB   = static_cast<int>(spinHash_.get_value());
    eConfig.multiPV      = static_cast<int>(spinMultiPV_.get_value());

    ViewConfig vConfig = baseViewConfig_;
    vConfig.theme           = static_cast<AppTheme>(dropTheme_.get_selected());
    vConfig.winGraphMode    = static_cast<WinGraphMode>(dropWinGraphMode_.get_selected());
    vConfig.showMoveNumbers = checkMoveNumbers_.get_active();
    vConfig.showCoordinates = checkCoordinates_.get_active();
    vConfig.hotkeyAnalyze = entryHotkeyAnalyze_.get_text();
    vConfig.hotkeyStop = entryHotkeyStop_.get_text();
    vConfig.hotkeyUndo = entryHotkeyUndo_.get_text();
    vConfig.hotkeyRedo = entryHotkeyRedo_.get_text();
    vConfig.hotkeyNewGame = entryHotkeyNewGame_.get_text();

    signal_applied.emit(eConfig, vConfig);
    close();
}

void SettingsDialog::onChooseEngine()
{
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Select Engine Executable");
    dialog->open(*this, [this, dialog](Glib::RefPtr<Gio::AsyncResult> &result) {
        try {
            auto file = dialog->open_finish(result);
            if (file)
                entryEnginePath_.set_text(file->get_path());
        } catch (const Glib::Error &) {
            // User cancelled.
        }
    });
}
