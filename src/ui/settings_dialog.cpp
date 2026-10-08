#include "settings_dialog.h"

#include "i18n/i18n.h"
#include "ui/settings_registry.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <string>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace settings_dialog_detail {

bool hasExecutableExtension(const std::string &path)
{
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com";
}

} // namespace settings_dialog_detail

namespace {

/// Checks whether `path` is usable as an engine executable: exists, is a
/// regular file (not a directory/socket/etc.), and — on POSIX — has execute
/// permission for the current user, or — on Windows — carries an executable
/// extension. Returns false with a human-readable `reason` set on the first
/// failing check.
bool isValidEnginePath(const std::string &path, std::string &reason)
{
    if (path.empty()) {
        reason = i18n::tr("Path is empty");
        return false;
    }
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        reason = i18n::tr("Path does not exist");
        return false;
    }
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        reason = i18n::tr("Not a regular file");
        return false;
    }
#if defined(_WIN32)
    // MSVC/Windows CRT has no X_OK; the execute bit is meaningless. Fall back to
    // an executable-extension check (existence + is_regular_file already passed).
    if (!settings_dialog_detail::hasExecutableExtension(path)) {
        reason = i18n::tr("Not an executable (.exe / .bat / .cmd / .com)");
        return false;
    }
#else
    if (access(path.c_str(), X_OK) != 0) {
        reason = i18n::tr("Not executable");
        return false;
    }
#endif
    return true;
}

// I18N-02: the dialog's row labels live in settings_registry (shared with the
// palette, whose titles are I18N-04). Translating them needs literal keys the
// lint can see, so the dialog maps the registry label to a tr() literal here.
// tests/test_i18n02_wiring.cpp asserts every registry label has a case.
std::string settingLabel(const settings_registry::SettingEntry &e)
{
    const std::string_view l = e.label;
    if (l == "Engine Path")               return i18n::tr("Engine Path");
    if (l == "Protocol Extension (.ptc)") return i18n::tr("Protocol Extension (.ptc)");
    if (l == "Threads")                   return i18n::tr("Threads");
    if (l == "Hash Size (MB)")            return i18n::tr("Hash Size (MB)");
    if (l == "Timeout / Turn (ms)")       return i18n::tr("Timeout / Turn (ms)");
    if (l == "Timeout / Match (ms)")      return i18n::tr("Timeout / Match (ms)");
    if (l == "Increment (ms)")            return i18n::tr("Increment (ms)");
    if (l == "Max Depth")                 return i18n::tr("Max Depth");
    if (l == "Max Nodes")                 return i18n::tr("Max Nodes");
    if (l == "Multi PV")                  return i18n::tr("Multi PV");
    if (l == "Analysis Detail")           return i18n::tr("Analysis Detail");
    if (l == "Theme")                     return i18n::tr("Theme");
    if (l == "Language")                  return i18n::tr("Language");
    if (l == "WinGraph Mode")             return i18n::tr("WinGraph Mode");
    if (l == "Show Move Numbers")         return i18n::tr("Show Move Numbers");
    if (l == "Show Coordinates")          return i18n::tr("Show Coordinates");
    if (l == "Analyze")                   return i18n::tr("Analyze");
    if (l == "Stop")                      return i18n::tr("Stop");
    if (l == "Undo")                      return i18n::tr("Undo");
    if (l == "Redo")                      return i18n::tr("Redo");
    if (l == "New Game")                  return i18n::tr("New Game");
    return std::string(l);  // a new registry row without a key above shows English
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
SettingsDialog::SettingsDialog(Gtk::Window &parent, const EngineConfig &eConfig, const ViewConfig &vConfig)
    : baseEngineConfig_(eConfig), baseViewConfig_(vConfig)
{
    set_title(i18n::tr("Settings"));
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
    notebook_ = notebook;

    // Build one tab: a grid of label/widget rows wrapped in a ScrolledWindow.
    struct Tab {
        Gtk::Grid *grid;
        int        row = 0;
        int        index = 0;
    };
    auto makeTab = [&](const std::string &title) -> std::shared_ptr<Tab> {
        auto *grid = Gtk::make_managed<Gtk::Grid>();
        grid->set_row_spacing(8);
        grid->set_column_spacing(12);
        grid->set_margin(12);
        auto *scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
        scroller->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
        scroller->set_child(*grid);
        scroller->set_vexpand(true);
        const int page = notebook->append_page(*scroller, title);
        auto t = std::make_shared<Tab>();
        t->grid = grid;
        t->index = page;
        return t;
    };
    // PAL-03: every row is declared in settings_registry (label + palette
    // metadata + tab). `focus` is the widget Ctrl+K should focus when the
    // row's widget is a container (e.g. the engine-path box).
    auto addRow = [this](const std::shared_ptr<Tab> &t, const char *id, Gtk::Widget &widget,
                         const std::string &tooltip = {}, Gtk::Widget *focus = nullptr) {
        const auto *reg = settings_registry::find(id);
        g_return_if_fail(reg != nullptr);              // a new setting needs a registry row
        g_return_if_fail(reg->tab == t->index);        // registry tab must match where it is built
        auto *lbl = Gtk::make_managed<Gtk::Label>(settingLabel(*reg));
        lbl->set_halign(Gtk::Align::END);
        t->grid->attach(*lbl, 0, t->row, 1, 1);
        widget.set_hexpand(true);
        t->grid->attach(widget, 1, t->row, 1, 1);
        if (!tooltip.empty()) {
            lbl->set_tooltip_text(tooltip);
            widget.set_tooltip_text(tooltip);
        }
        targets_[std::string(id)] = {t->index, focus ? focus : &widget};
        t->row++;
    };

    auto engineTab  = makeTab(i18n::tr("Engine"));
    auto timeTab    = makeTab(i18n::tr("Time"));
    auto searchTab  = makeTab(i18n::tr("Search"));
    auto uiTab      = makeTab(i18n::tr("UI"));
    auto hotkeysTab = makeTab(i18n::tr("Hotkeys"));

    // ── Engine tab ──────────────────────────────────────────────────────────
    auto *pathBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
    entryEnginePath_.set_text(eConfig.enginePath);
    entryEnginePath_.set_hexpand(true);
    auto *btnBrowse = Gtk::make_managed<Gtk::Button>(i18n::tr("Browse…"));
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
    addRow(engineTab, "set.engine-path", *pathContainer,
           i18n::tr("Path to the Gomocup/Yixin-protocol engine executable."), &entryEnginePath_);

    // PROTO-03: optional protocol-extension (.ptc) file. Empty = none.
    auto *ptcBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
    entryPtcPath_.set_text(eConfig.protocolExtensionPath);
    entryPtcPath_.set_hexpand(true);
    auto *btnPtcBrowse = Gtk::make_managed<Gtk::Button>(i18n::tr("Browse…"));
    btnPtcBrowse->signal_clicked().connect(sigc::mem_fun(*this, &SettingsDialog::onChoosePtc));
    auto *btnPtcClear = Gtk::make_managed<Gtk::Button>(i18n::tr("Clear"));
    btnPtcClear->signal_clicked().connect([this]() { entryPtcPath_.set_text(""); });
    ptcBox->append(entryPtcPath_);
    ptcBox->append(*btnPtcBrowse);
    ptcBox->append(*btnPtcClear);
    addRow(engineTab, "set.ptc", *ptcBox,
           i18n::tr("Optional TOML file declaring extra console commands for this engine "
                    "(PROTO-03). Takes effect on engine start / Reload."), &entryPtcPath_);

    spinThreads_.set_adjustment(Gtk::Adjustment::create(eConfig.threads, 1, 256, 1));
    spinThreads_.set_digits(0);
    addRow(engineTab, "set.threads", spinThreads_, i18n::tr("Number of search threads the engine may use."));

    spinHash_.set_adjustment(Gtk::Adjustment::create(eConfig.hashSizeMB, 1, 65536, 64));
    spinHash_.set_digits(0);
    addRow(engineTab, "set.hash", spinHash_, i18n::tr("Transposition-table size in megabytes."));

    // ── Time tab ────────────────────────────────────────────────────────────
    spinTimeoutTurn_.set_adjustment(Gtk::Adjustment::create(eConfig.timeoutTurn, 0, 6000000, 1000));
    spinTimeoutTurn_.set_digits(0);
    addRow(timeTab, "set.timeout-turn", spinTimeoutTurn_, i18n::tr("Milliseconds allowed per move. 0 = unlimited."));

    spinTimeoutMatch_.set_adjustment(Gtk::Adjustment::create(eConfig.timeoutMatch, 0, 99000000, 10000));
    spinTimeoutMatch_.set_digits(0);
    addRow(timeTab, "set.timeout-match", spinTimeoutMatch_, i18n::tr("Total milliseconds for the whole game. 0 = unlimited."));

    spinIncrement_.set_adjustment(Gtk::Adjustment::create(eConfig.increment, 0, 60000, 500));
    spinIncrement_.set_digits(0);
    addRow(timeTab, "set.increment", spinIncrement_, i18n::tr("Time added back to the clock after each move."));

    // ── Search tab ──────────────────────────────────────────────────────────
    spinMaxDepth_.set_adjustment(Gtk::Adjustment::create(eConfig.maxDepth, 1, 225, 1));
    spinMaxDepth_.set_digits(0);
    addRow(searchTab, "set.max-depth", spinMaxDepth_, i18n::tr("Maximum search depth in plies."));

    spinMaxNodes_.set_adjustment(Gtk::Adjustment::create(eConfig.maxNodes, 0, 100000000000, 1000000));
    spinMaxNodes_.set_digits(0);
    addRow(searchTab, "set.max-nodes", spinMaxNodes_, i18n::tr("Node budget per search. 0 = unlimited."));

    spinMultiPV_.set_adjustment(Gtk::Adjustment::create(eConfig.multiPV, 1, 20, 1));
    spinMultiPV_.set_digits(0);
    addRow(searchTab, "set.multipv", spinMultiPV_, i18n::tr("Number of principal variations the engine reports."));

    spinShowDetail_.set_adjustment(Gtk::Adjustment::create(eConfig.showDetail, 0, 3, 1));
    spinShowDetail_.set_digits(0);
    addRow(searchTab, "set.analysis-detail", spinShowDetail_,
           i18n::tr("How much incremental search output the engine streams (INFO SHOW_DETAIL). "
                    "0 = final result only; 2 = per-depth PV blocks; 3 = also the live move feed. "
                    "Default 3. A SHOW_DETAIL set via the console still overrides this."));

    // ── UI tab ──────────────────────────────────────────────────────────────
    auto themeModel = Gtk::StringList::create({i18n::tr("System"), i18n::tr("Light"), i18n::tr("Dark")});
    dropTheme_.set_model(themeModel);
    dropTheme_.set_selected(static_cast<guint>(vConfig.theme));
    addRow(uiTab, "set.theme", dropTheme_,
           i18n::tr("System follows your desktop setting; Light / Dark force it."));

    // I18N-03: choices are System (translated) + each language's native name.
    // Applied on Apply via signal_applied; the dialog closes then, so a later
    // open is rebuilt in the new language (nothing to retranslate in place).
    {
        auto langModel = Gtk::StringList::create({i18n::tr("System")});
        for (const auto &lang : i18n::languages())
            langModel->append(lang.nativeName);
        dropLanguage_.set_model(langModel);
        const auto choices = languageChoices();
        const auto it = std::find(choices.begin(), choices.end(), i18n::normalizeLanguageSetting(vConfig.language));
        dropLanguage_.set_selected(static_cast<guint>(it == choices.end() ? 0 : it - choices.begin()));
        addRow(uiTab, "set.language", dropLanguage_,
               i18n::tr("Language of the interface. System follows your desktop language."));
    }

    auto modeModel = Gtk::StringList::create(
        {i18n::tr("Single line (Black's perspective)"), i18n::tr("Two lines (Black & White)")});
    dropWinGraphMode_.set_model(modeModel);
    dropWinGraphMode_.set_selected(static_cast<guint>(vConfig.winGraphMode));
    addRow(uiTab, "set.wingraph-mode", dropWinGraphMode_,
           i18n::tr("Single line: one win-rate curve, always from Black's perspective. "
                    "Two lines: Black and White win-rate each in its own perspective."));

    checkMoveNumbers_.set_active(vConfig.showMoveNumbers);
    addRow(uiTab, "set.move-numbers", checkMoveNumbers_, i18n::tr("Draw the move ordinal on each stone."));

    checkCoordinates_.set_active(vConfig.showCoordinates);
    addRow(uiTab, "set.coordinates", checkCoordinates_, i18n::tr("Draw A–O column and 1–N row labels around the board."));

    // ── Hotkeys tab ─────────────────────────────────────────────────────────
    entryHotkeyAnalyze_.set_text(vConfig.hotkeyAnalyze);
    addRow(hotkeysTab, "set.hotkey-analyze", entryHotkeyAnalyze_, i18n::tr("e.g. F5 or Ctrl+A"));
    entryHotkeyStop_.set_text(vConfig.hotkeyStop);
    addRow(hotkeysTab, "set.hotkey-stop", entryHotkeyStop_, i18n::tr("e.g. Escape"));
    entryHotkeyUndo_.set_text(vConfig.hotkeyUndo);
    addRow(hotkeysTab, "set.hotkey-undo", entryHotkeyUndo_, i18n::tr("e.g. Ctrl+Z"));
    entryHotkeyRedo_.set_text(vConfig.hotkeyRedo);
    addRow(hotkeysTab, "set.hotkey-redo", entryHotkeyRedo_, i18n::tr("e.g. Ctrl+Y"));
    entryHotkeyNewGame_.set_text(vConfig.hotkeyNewGame);
    addRow(hotkeysTab, "set.hotkey-newgame", entryHotkeyNewGame_, i18n::tr("e.g. Ctrl+N"));

    root->append(*notebook);

    // ── Buttons ─────────────────────────────────────────────────────────────
    auto *btnBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    btnBox->set_halign(Gtk::Align::END);
    btnBox->set_margin_top(12);

    auto *btnCancel = Gtk::make_managed<Gtk::Button>(i18n::tr("Cancel"));
    auto *btnApply  = Gtk::make_managed<Gtk::Button>(i18n::tr("Apply"));
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

bool SettingsDialog::showSetting(const std::string &id)
{
    const auto it = targets_.find(id);
    if (it == targets_.end() || !notebook_)
        return false;
    notebook_->set_current_page(it->second.tab);
    if (it->second.focus)
        it->second.focus->grab_focus();
    return true;
}

std::vector<std::string> SettingsDialog::registeredSettingIds() const
{
    std::vector<std::string> ids;
    ids.reserve(targets_.size());
    for (const auto &[id, target] : targets_)
        ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

void SettingsDialog::onEnginePathChanged()
{
    std::string path = entryEnginePath_.get_text();
    std::string reason;
    enginePathValid_ = isValidEnginePath(path, reason);

    if (enginePathValid_) {
        lblEnginePathStatus_.set_text(i18n::tr("✓ Executable found"));
        lblEnginePathStatus_.remove_css_class("error");
        entryEnginePath_.remove_css_class("error");
    } else {
        lblEnginePathStatus_.set_text(i18n::format(i18n::tr("✗ %s"), reason));
        lblEnginePathStatus_.add_css_class("error");
        entryEnginePath_.add_css_class("error");
    }
    updateApplySensitivity();
}

std::vector<std::string> SettingsDialog::languageChoices()
{
    std::vector<std::string> v{i18n::kSystemLanguage};
    for (const auto &lang : i18n::languages())
        v.push_back(lang.code);
    return v;
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
    eConfig.showDetail   = static_cast<int>(spinShowDetail_.get_value());
    eConfig.protocolExtensionPath = entryPtcPath_.get_text();

    ViewConfig vConfig = baseViewConfig_;
    vConfig.theme           = static_cast<AppTheme>(dropTheme_.get_selected());
    {
        const auto choices = languageChoices();
        const guint sel = dropLanguage_.get_selected();
        vConfig.language = sel < choices.size() ? choices[sel] : std::string(i18n::kSystemLanguage);
    }
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

void SettingsDialog::onChoosePtc()
{
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title(i18n::tr("Select Protocol Extension (.ptc)"));

    auto filter = Gtk::FileFilter::create();
    filter->set_name(i18n::tr("Protocol extension (*.ptc)"));
    filter->add_pattern("*.ptc");
    auto filters = Gio::ListStore<Gtk::FileFilter>::create();
    filters->append(filter);
    dialog->set_filters(filters);
    dialog->set_default_filter(filter);

    dialog->open(*this, [this, dialog](Glib::RefPtr<Gio::AsyncResult> &result) {
        try {
            auto file = dialog->open_finish(result);
            if (file)
                entryPtcPath_.set_text(file->get_path());
        } catch (const Glib::Error &) {
            // User cancelled.
        }
    });
}

void SettingsDialog::onChooseEngine()
{
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title(i18n::tr("Select Engine Executable"));
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
