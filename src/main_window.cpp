#include "main_window.h"
#include "model/rdb/game_archive.h"
#include "model/rdb/game_graph_convert.h"
#include "model/settings_storage.h"
#include "ui/about_dialog.h"
#include "ui/settings_dialog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

// UX-03: icon-only glyph buttons need both a hover tooltip (sighted users)
// and an accessible name (screen readers). set_tooltip_text() alone does
// NOT populate the accessible name in GTK4 -- Gtk::Accessible::Property::
// LABEL must be set explicitly too. See EngineStatusView's btnStart_/
// btnStop_/btnReload_ (src/ui/engine_status.cpp) for the existing in-repo
// pattern this mirrors (that widget only calls set_tooltip_text(), so the
// accessible-label half is the new part introduced here).
static void setButtonTooltipAndLabel(Gtk::Button &button, const Glib::ustring &text)
{
    button.set_tooltip_text(text);
    Glib::Value<Glib::ustring> value;
    value.init(value.value_type());
    value.set(text);
    button.update_property(Gtk::Accessible::Property::LABEL, value);
}

// UX-06: apply a theme preset through GTK Settings (this build links no
// libadwaita). `System` leaves the GTK/desktop default untouched; Light/Dark
// force a concrete theme.
//
// gtk-application-prefer-dark-theme alone is a no-op on many GTK4 setups
// (it only nudges themes that ship a matching `-dark` variant and honour the
// hint) -- on this machine (KDE/Breeze GTK theme) it does nothing visible.
// Forcing gtk-theme-name to Adwaita / Adwaita-dark is the portable lever:
// Adwaita ships inside GTK itself, so both names always resolve. Safe to
// call at startup and again on every Settings Apply.
static void applyAppTheme(AppTheme theme)
{
    auto settings = Gtk::Settings::get_default();
    if (!settings) return;

    if (theme == AppTheme::System) {
        settings->reset_property("gtk-application-prefer-dark-theme");
        settings->reset_property("gtk-theme-name");
        return;
    }

    const bool dark = (theme == AppTheme::Dark);
    settings->property_gtk_application_prefer_dark_theme() = dark;
    settings->property_gtk_theme_name() = dark ? "Adwaita-dark" : "Adwaita";
}

static std::string trim(const std::string &s)
{
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

static std::string upper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

static bool hotkeyMatches(const std::string &specRaw, guint keyval, Gdk::ModifierType state)
{
    std::string spec = upper(trim(specRaw));
    if (spec.empty()) return false;

    bool wantCtrl = spec.find("CTRL+") != std::string::npos;
    bool wantShift = spec.find("SHIFT+") != std::string::npos;
    bool wantAlt = spec.find("ALT+") != std::string::npos;
    spec.erase(0, spec.rfind('+') == std::string::npos ? 0 : spec.rfind('+') + 1);

    bool hasCtrl = (state & Gdk::ModifierType::CONTROL_MASK) != Gdk::ModifierType(0);
    bool hasShift = (state & Gdk::ModifierType::SHIFT_MASK) != Gdk::ModifierType(0);
    bool hasAlt = (state & Gdk::ModifierType::ALT_MASK) != Gdk::ModifierType(0);

    if (wantCtrl != hasCtrl || wantShift != hasShift || wantAlt != hasAlt) return false;

    if (spec == "ESC" || spec == "ESCAPE") return keyval == GDK_KEY_Escape;
    if (spec == "SPACE") return keyval == GDK_KEY_space;

    if (spec.size() == 2 && spec[0] == 'F' && std::isdigit(static_cast<unsigned char>(spec[1]))) {
        int fn = spec[1] - '0';
        return keyval == static_cast<guint>(GDK_KEY_F1 + (fn - 1));
    }
    if (spec.size() == 3 && spec[0] == 'F' && std::isdigit(static_cast<unsigned char>(spec[1]))
        && std::isdigit(static_cast<unsigned char>(spec[2]))) {
        int fn = (spec[1] - '0') * 10 + (spec[2] - '0');
        if (fn >= 10 && fn <= 12) {
            return keyval == static_cast<guint>(GDK_KEY_F1 + (fn - 1));
        }
    }

    if (spec.size() == 1) {
        guint want = gdk_unicode_to_keyval(static_cast<guint32>(std::tolower(spec[0])));
        guint got  = gdk_keyval_to_lower(keyval);
        return want == got;
    }
    return false;
}

// ═════════════════════════════════════════════════════════════════════════════
MainWindow::MainWindow()
    : boardViewModel_(gameState_)
    , controller_(gameState_, engine_)
    , boardView_(boardViewModel_)
    , analysisPanel_(gameState_)
    , bottomPanel_()
{
    set_title(kAppDisplayName);
    set_default_size(1280, 800);

    buildMenuBar();
    buildToolbar();
    buildLayout();

    commandDispatcher_ = std::make_unique<CommandDispatcher>(CommandContext{
        .gameState = gameState_,
        .engine = engine_,
        .controller = controller_,
        .print = [this](const std::string &line) { bottomPanel_.appendRecv("Output", line); },
        .clearConsole = [this]() { bottomPanel_.clearEngineLog(); },
    });

    connectSignals();

    // UI-03: seed the persistent rule indicator once at startup so it never
    // starts out blank before the first setRule() call.
    updateRuleLabel();

    // Load persisted user settings (with defaults fallback).
    auto saved = SettingsStorage::load();
    gameState_.setEngineConfig(saved.engine);
    gameState_.setViewConfig(saved.view);
    gameState_.setMatchConfig(saved.match);
    syncEnginePlaysMenu();
    syncAnalyzeModeMenu();  // ANLZ-01: reflect persisted analyzeMode onto both toggle surfaces

    // STATE-04: restore the last-selected rule (global preference) and the
    // persisted new-game board size. The board is empty at startup so the
    // newGame() call here discards nothing. The engine is not started until
    // the user first analyzes; onStartAnalysis() issues sendConfig() right
    // after startEngine(), so the engine sees the restored size/rule then.
    gameState_.setRule(saved.setup.rule);
    if (saved.setup.boardSize != gameState_.boardSize())
        gameState_.newGame(saved.setup.boardSize);

    // UX-06: apply the persisted theme explicitly at startup (independent of
    // the signal_config_changed path above, so it holds even if the wiring
    // order changes).
    applyAppTheme(gameState_.viewConfig().theme);

    // Global hotkeys from ViewConfig.
    auto keyCtrl = Gtk::EventControllerKey::create();
    keyCtrl->signal_key_pressed().connect([this](guint keyval, guint, Gdk::ModifierType state) {
        const auto &v = gameState_.viewConfig();
        if (hotkeyMatches(v.hotkeyAnalyze, keyval, state)) { onStartAnalysis(); return true; }
        if (hotkeyMatches(v.hotkeyStop, keyval, state)) { onStopAnalysis(); return true; }
        if (hotkeyMatches(v.hotkeyUndo, keyval, state)) { onUndo(); return true; }
        if (hotkeyMatches(v.hotkeyRedo, keyval, state)) { onRedo(); return true; }
        if (hotkeyMatches(v.hotkeyNewGame, keyval, state)) { onNewGame(); return true; }
        return false;
    }, false);
    add_controller(keyCtrl);

    // RT-01: coalesce engine analysis UI updates to a bounded ~13 Hz instead
    // of one full rebuild per parsed engine line. GameState::setAnalysisData
    // marks a dirty flag; this tick consumes it. Search completion / stop
    // still flush immediately via EngineController (see engine_controller.cpp).
    analysisTickConn_ = Glib::signal_timeout().connect(
        [this]() { gameState_.tickAnalysis(); return true; }, 75);
}

MainWindow::~MainWindow()
{
    analysisTickConn_.disconnect();
}

// ─── Menu bar (Gio::Menu + PopoverMenuBar) ───────────────────────────────────
void MainWindow::buildMenuBar()
{
    // Register actions on the window action group.
    auto addAction = [this](const char *name, auto callback) {
        auto action = Gio::SimpleAction::create(name);
        action->signal_activate().connect(
            [this, callback](const Glib::VariantBase &) { (this->*callback)(); });
        add_action(action);
    };

    addAction("new-game",   &MainWindow::onNewGame);
    addAction("load-game",  &MainWindow::onLoadGame);
    addAction("save-game",  &MainWindow::onSaveGame);
    addAction("quit",       &MainWindow::onQuit);
    addAction("board-size", &MainWindow::onBoardSize);
    addAction("settings",   &MainWindow::onSettings);
    addAction("about",      &MainWindow::onAbout);
    addAction("analyze",    &MainWindow::onStartAnalysis);
    addAction("stop",       &MainWindow::onStopAnalysis);

    // Rule actions — use a stateful string action.
    auto ruleAction = Gio::SimpleAction::create_radio_string("set-rule", "freestyle");
    ruleAction->signal_activate().connect([this, ruleAction](const Glib::VariantBase &param) {
        auto val = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(param).get();
        ruleAction->change_state(Glib::Variant<Glib::ustring>::create(val));

        if (val == "freestyle")   onSetRule(GameRule::Freestyle);
        else if (val == "standard")   onSetRule(GameRule::Standard);
        else if (val == "renju")      onSetRule(GameRule::Renju);
    });
    add_action(ruleAction);

    // UI-06: "Engine plays" side selector — a radio-string action mirroring
    // set-rule above. Replaces the old "Analysis" menu, which only duplicated
    // the toolbar's Analyze/Stop buttons. Unlike set-rule, this one DOES seed
    // its state from persisted config: syncEnginePlaysMenu() is called after
    // SettingsStorage::load() in the constructor.
    enginePlaysAction_ = Gio::SimpleAction::create_radio_string("engine-plays", "off");
    enginePlaysAction_->signal_activate().connect(
        [this](const Glib::VariantBase &param) {
            auto val = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(param).get();
            enginePlaysAction_->change_state(Glib::Variant<Glib::ustring>::create(val));

            if (val == "black")      onSetEnginePlays(EnginePlaysSide::Black);
            else if (val == "white") onSetEnginePlays(EnginePlaysSide::White);
            else                     onSetEnginePlays(EnginePlaysSide::Off);
        });
    add_action(enginePlaysAction_);

    // ANLZ-01: "Analyze Mode" — a checkable (boolean) action. Seeds its state
    // from persisted ViewConfig via syncAnalyzeModeMenu(), called after
    // SettingsStorage::load() in the constructor (like engine-plays above).
    analyzeModeAction_ = Gio::SimpleAction::create_bool("analyze-mode", false);
    analyzeModeAction_->signal_change_state().connect(
        [this](const Glib::VariantBase &param) {
            bool active = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(param).get();
            // set_state (not change_state) — change_state re-emits this very
            // signal and would recurse. syncAnalyzeModeMenu() also uses set_state.
            analyzeModeAction_->set_state(Glib::Variant<bool>::create(active));
            onToggleAnalyzeMode(active);
        });
    add_action(analyzeModeAction_);

    // ── Build menu model ────────────────────────────────────────────────────
    auto menuModel = Gio::Menu::create();

    // Game menu.
    auto gameMenu = Gio::Menu::create();
    gameMenu->append("New",        "win.new-game");
    gameMenu->append("Load",       "win.load-game");
    gameMenu->append("Save",       "win.save-game");

    auto ruleSection = Gio::Menu::create();
    ruleSection->append("Freestyle Gomoku",     "win.set-rule::freestyle");
    ruleSection->append("Standard Gomoku",      "win.set-rule::standard");
    ruleSection->append("Free Renju",           "win.set-rule::renju");

    gameMenu->append_submenu("Rule", ruleSection);
    gameMenu->append("Board Size",  "win.board-size");

    auto gameSection2 = Gio::Menu::create();
    gameSection2->append("Quit", "win.quit");
    gameMenu->append_section("", gameSection2);

    // Players menu.
    auto playersMenu = Gio::Menu::create();
    playersMenu->append("Settings…", "win.settings");

    // Engine plays menu (UI-06) — pick which side the engine auto-plays.
    auto enginePlaysMenu = Gio::Menu::create();
    enginePlaysMenu->append("Black", "win.engine-plays::black");
    enginePlaysMenu->append("White", "win.engine-plays::white");
    enginePlaysMenu->append("Off",   "win.engine-plays::off");

    // ANLZ-01: Analyze Mode checkbox lives in its own section of the same menu.
    auto analyzeModeSection = Gio::Menu::create();
    analyzeModeSection->append("Analyze Mode", "win.analyze-mode");
    enginePlaysMenu->append_section("", analyzeModeSection);

    // Help menu.
    auto helpMenu = Gio::Menu::create();
    helpMenu->append("About", "win.about");

    menuModel->append_submenu("Game",         gameMenu);
    menuModel->append_submenu("Players",      playersMenu);
    menuModel->append_submenu("Engine plays", enginePlaysMenu);
    menuModel->append_submenu("Help",         helpMenu);

    menuBar_.set_menu_model(menuModel);
}

// ─── Toolbar (header bar buttons) ────────────────────────────────────────────
void MainWindow::buildToolbar()
{
    headerBar_.set_show_title_buttons(true);

    // ── File group (linked) ─────────────────────────────────────────────────
    auto *fileGroup = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 0);
    fileGroup->add_css_class("linked");

    btnNew_  = Gtk::make_managed<Gtk::Button>("New");
    btnLoad_ = Gtk::make_managed<Gtk::Button>("Load");
    auto *btnSave = Gtk::make_managed<Gtk::Button>("Save");

    btnNew_->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onNewGame));
    btnLoad_->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onLoadGame));
    btnSave->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onSaveGame));

    fileGroup->append(*btnNew_);
    fileGroup->append(*btnLoad_);
    fileGroup->append(*btnSave);
    headerBar_.pack_start(*fileGroup);

    // UI-03: persistent rule indicator -- always visible in the header bar,
    // not only inside the Game > Rule menu. Kept current by updateRuleLabel().
    ruleLabel_.add_css_class("dim-label");
    ruleLabel_.set_margin_start(8);
    headerBar_.pack_start(ruleLabel_);

    // ── Center: analysis group (linked) ─────────────────────────────────────
    auto *analysisGroup = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 0);
    analysisGroup->add_css_class("linked");

    auto *btnStart = Gtk::make_managed<Gtk::Button>("▶ Analyze");
    auto *btnStop  = Gtk::make_managed<Gtk::Button>("■ Stop");

    btnStart->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onStartAnalysis));
    btnStop->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onStopAnalysis));

    analysisGroup->append(*btnStart);
    analysisGroup->append(*btnStop);
    headerBar_.set_title_widget(*analysisGroup);

    // ── Right-side: navigation group (linked) ───────────────────────────────
    auto *navGroup = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 0);
    navGroup->add_css_class("linked");

    btnFirst_ = Gtk::make_managed<Gtk::Button>("⏮");
    btnUndo_  = Gtk::make_managed<Gtk::Button>("↶");
    btnRedo_  = Gtk::make_managed<Gtk::Button>("↷");
    btnLast_  = Gtk::make_managed<Gtk::Button>("⏭");

    // UX-03: these were bare glyphs with no tooltip/accessible name.
    setButtonTooltipAndLabel(*btnFirst_, "Jump to first move");
    setButtonTooltipAndLabel(*btnUndo_,  "Undo move");
    setButtonTooltipAndLabel(*btnRedo_,  "Redo move");
    setButtonTooltipAndLabel(*btnLast_,  "Jump to last move");

    btnFirst_->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onUndoAll));
    btnUndo_->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onUndo));
    btnRedo_->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onRedo));
    btnLast_->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onRedoAll));

    navGroup->append(*btnFirst_);
    navGroup->append(*btnUndo_);
    navGroup->append(*btnRedo_);
    navGroup->append(*btnLast_);
    headerBar_.pack_end(*navGroup);

    set_titlebar(headerBar_);
}

// ─── Layout ──────────────────────────────────────────────────────────────────
void MainWindow::buildLayout()
{
    mainHPaned_.set_orientation(Gtk::Orientation::HORIZONTAL);
    mainHPaned_.set_start_child(boardView_);
    mainHPaned_.set_end_child(analysisPanel_);
    mainHPaned_.set_resize_start_child(true);
    mainHPaned_.set_shrink_start_child(false);
    mainHPaned_.set_resize_end_child(true);
    mainHPaned_.set_shrink_end_child(false);
    mainHPaned_.set_position(640);

    mainVPaned_.set_orientation(Gtk::Orientation::VERTICAL);
    mainVPaned_.set_start_child(mainHPaned_);
    mainVPaned_.set_end_child(bottomPanel_);
    mainVPaned_.set_resize_start_child(true);
    mainVPaned_.set_shrink_start_child(false);
    mainVPaned_.set_resize_end_child(true);
    mainVPaned_.set_shrink_end_child(false);
    mainVPaned_.set_position(580);

    rootBox_.append(menuBar_);
    rootBox_.append(mainVPaned_);
    rootBox_.set_vexpand(true);
    set_child(rootBox_);

    // UX-05: keep hPanedFraction_/vPanedFraction_ in sync with whatever the
    // divider is actually at (initial value here, profile-preset switches in
    // connectSignals(), or a live user drag) -- see trackPanedFraction()'s
    // doc comment in main_window.h for how it tells a genuine change apart
    // from a GTK-internal clamp during a resize.
    mainHPaned_.property_position().signal_changed().connect(
        [this]() { trackPanedFraction(mainHPaned_, hPanedFraction_, hPanedLastExtent_, false); });
    mainVPaned_.property_position().signal_changed().connect(
        [this]() { trackPanedFraction(mainVPaned_, vPanedFraction_, vPanedLastExtent_, true); });
}

void MainWindow::trackPanedFraction(Gtk::Paned &paned, double &fraction, int &lastExtent, bool vertical)
{
    const int extent = vertical ? paned.get_height() : paned.get_width();
    if (extent > 0 && extent == lastExtent) {
        // Extent hasn't moved since we last looked -- this position change
        // is a genuine user drag (or our own reapplyPanedFractions() call,
        // which recomputes the same fraction back out, so it's idempotent),
        // not a GTK-internal clamp caused by the window itself resizing.
        fraction = static_cast<double>(paned.get_position()) / extent;
    }
    lastExtent = extent;
}

void MainWindow::reapplyPanedFractions()
{
    const int hExtent = mainHPaned_.get_width();
    if (hExtent > 0) {
        mainHPaned_.set_position(static_cast<int>(hPanedFraction_ * hExtent));
    }
    const int vExtent = mainVPaned_.get_height();
    if (vExtent > 0) {
        mainVPaned_.set_position(static_cast<int>(vPanedFraction_ * vExtent));
    }
}

void MainWindow::size_allocate_vfunc(int width, int height, int baseline)
{
    Gtk::ApplicationWindow::size_allocate_vfunc(width, height, baseline);

    // Reassert both dividers' proportional positions now that children have
    // been allocated at the new window size. On a plain resize this is a
    // no-op (the fraction round-trips to the same pixel value); on a
    // grow-back-after-shrink this is what restores the board pane instead
    // of leaving it clamped to whatever the shrink squeezed it down to.
    reapplyPanedFractions();
}

// ─── Signal wiring ───────────────────────────────────────────────────────────
void MainWindow::connectSignals()
{
    // Board click → place a move.
    boardView_.signal_move_clicked.connect([this](Coord pos) {
        // ANLZ-05: a click during an in-flight analysis places the stone. Stop
        // the search first — stopAnalysis() clears GameState::analyzing_ and
        // returns the controller to Idle synchronously, so makeMove()'s
        // `if (analyzing_) return false;` guard (deliberately unchanged) passes.
        // The signal_board_changed it emits re-enters scheduleAnalyzeModeRestart()
        // for the new position. Gate on isAnalyzing() so a click with no engine /
        // no search stays a plain makeMove().
        if (controller_.isAnalyzing())
            controller_.stopAnalysis();
        gameState_.makeMove(pos);
    });

    // Board state changed → refresh view.
    gameState_.signal_board_changed.connect([this]() {
        // Clear the hover preview overlay when a new move is placed (not tied to
        // state_.pvLines(), so update() below can't refresh it on its own).
        // candidateMoves is intentionally NOT cleared here anymore: GameState now
        // clears pvLines_ itself on every position change (see STATE-01), so
        // update() below already repopulates candidateMoves from the correct
        // (now-empty, post-change) pvLines() — a separate clear here was
        // redundant defensive code that could only ever paper over a GameState
        // bug, not fix one.
        boardViewModel_.pvPreview.clear();

        boardViewModel_.update();
        boardView_.queueRedraw();

        // Refresh database for the new position if enabled.
        if (gameState_.viewConfig().showDatabase) {
            controller_.queryDatabase();
        }

        // Update move log.
        auto &hist = gameState_.history();
        Glib::ustring log;
        int count = hist.moveCount();
        for (int i = 0; i < count; ++i) {
            Coord c = hist.moves()[i];
            if (i % 2 == 0)
                log += Glib::ustring::compose("%1. ", (i / 2) + 1);
            log += Glib::ustring::compose("%1%2 ",
                       std::string(1, static_cast<char>('A' + c.x)),
                       std::to_string(gameState_.boardSize() - c.y));
        }
        bottomPanel_.clear();
        bottomPanel_.appendMoveLog(log);

        // UI-06: a position change may hand the turn to the side the engine
        // plays — check if an auto-move is now due.
        maybeStartAutoMove();

        // ANLZ-01: a position change means the current analysis (if any) is now
        // stale — restart it on the new position so the WinGraph gains a point.
        // ANLZ-07: force=true — this is a genuine position change, so any
        // cached "converged" result (necessarily for a DIFFERENT position)
        // must never suppress analysing this one.
        scheduleAnalyzeModeRestart(/*force=*/true);
    });

    // UI-03: rule changed → refresh the persistent rule indicator and
    // re-derive board-view state that depends on the rule (Renju forbidden
    // points -- see BoardViewModel::update()/RenjuRule::forbiddenPoints()).
    // Previously nothing listened to this signal at all.
    gameState_.signal_rule_changed.connect([this]() {
        updateRuleLabel();
        boardViewModel_.update();
        boardView_.queueRedraw();
    });

    // Move selected → jump and redraw.
    gameState_.signal_move_selected.connect([this](int /*moveIndex*/) {
        boardViewModel_.update();
        boardView_.queueRedraw();
    });

    // PV hover → show ghost stones.
    analysisPanel_.pvView().signal_pv_hovered.connect(
        [this](std::vector<Coord> pvMoves) {
            boardViewModel_.pvPreview = pvMoves;
            boardView_.queueRedraw();
        });

    analysisPanel_.pvView().signal_pv_hover_left.connect([this]() {
        boardViewModel_.pvPreview.clear();
        boardView_.queueRedraw();
    });

    // Engine analysis updated → update candidate moves on board in realtime.
    gameState_.signal_engine_analysis.connect([this]() {
        boardViewModel_.update();
        boardView_.queueRedraw();
    });

    // Database updates.
    controller_.signal_database_entry.connect([this](const DatabaseEntry& entry) {
        gameState_.addDatabaseEntry(entry);
    });

    controller_.signal_database_refresh.connect([this]() {
        gameState_.clearDatabase();
    });

    controller_.signal_database_done.connect([this]() {
        gameState_.updateDatabase();
    });

    gameState_.signal_database_updated.connect([this]() {
        boardViewModel_.update();
        boardView_.queueRedraw();
    });

    // Engine output → Engine Log tab as [RECV].
    controller_.signal_engine_output.connect([this](std::string type, std::string line) {
        bottomPanel_.appendRecv(type, line);
    });

    // All commands sent to engine → Engine Log tab as [SEND].
    engine_.signal_line_sent.connect([this](const std::string &cmd) {
        bottomPanel_.appendSend(cmd);
    });

    // User-typed command from BottomPanel → engine.
    bottomPanel_.signal_command_sent.connect([this](std::string cmd) {
        if (!commandDispatcher_) return;
        bool handled = commandDispatcher_->executeLine(cmd);
        (void)handled;
    });

    // Engine state changes → update indicator.
    controller_.signal_state_changed.connect([this](EngineController::EngineState state) {
        analysisPanel_.engineStatus().setEngineState(state);
    });

    // Config changes → update view and theme.
    gameState_.signal_config_changed.connect([this]() {
        // UX-06: theme (System/Light/Dark) — driven through GTK Settings
        // (no libadwaita in this build). System leaves the GTK default alone.
        applyAppTheme(gameState_.viewConfig().theme);

        // Apply board view changes (Move numbers, Coordinates). The renderer
        // reads vm_.viewConfig.showCoordinates; refreshing the model here and
        // queuing a redraw is what makes the Settings toggle take effect.
        boardViewModel_.update();
        boardView_.queueRedraw();

        // ENG-02: keep the "Engine plays" radio in sync with MatchConfig for
        // changes that don't route through the menu-activate handler — e.g. the
        // dispatcher `analyze` / `!play` revert-on-engine's-turn path. This is a
        // state-only update (Gio::SimpleAction::change_state); it does NOT fire
        // signal_activate, so onSetEnginePlays does not re-enter / re-persist.
        syncEnginePlaysMenu();

        // ANLZ-01: keep the Analyze Mode checkbox / button in sync with
        // ViewConfig for any change that didn't route through onToggleAnalyzeMode
        // (state-only, no re-persist — same rationale as syncEnginePlaysMenu).
        syncAnalyzeModeMenu();
    });

    // Analysis state changes → toggle UI interaction.
    controller_.signal_state_changed.connect([this](EngineController::EngineState state) {
        bool sensitive = (state != EngineController::EngineState::Analyzing);
        if (btnFirst_) btnFirst_->set_sensitive(sensitive);
        if (btnUndo_)  btnUndo_ ->set_sensitive(sensitive);
        if (btnRedo_)  btnRedo_ ->set_sensitive(sensitive);
        if (btnLast_)  btnLast_ ->set_sensitive(sensitive);
        if (btnNew_)   btnNew_  ->set_sensitive(sensitive);
        if (btnLoad_)  btnLoad_ ->set_sensitive(sensitive);

        // Also disable corresponding Menu items
        auto toggleAction = [this, sensitive](const char *name) {
            auto act = lookup_action(name);
            if (act) {
                auto simpleAct = std::dynamic_pointer_cast<Gio::SimpleAction>(act);
                if (simpleAct) simpleAct->set_enabled(sensitive);
            }
        };
        toggleAction("new-game");
        toggleAction("load-game");
        toggleAction("board-size");
        toggleAction("set-rule");
    });

    // Engine made a move → auto-play on the board.
    controller_.signal_engine_move.connect([this](Coord pos) {
        gameState_.makeMove(pos);
    });

    // UI-06: when the engine transitions to Idle (just started, or a search
    // finished) it may already be its turn under "Engine plays <side>".
    controller_.signal_state_changed.connect([this](EngineController::EngineState state) {
        if (state == EngineController::EngineState::Idle) {
            maybeStartAutoMove();
            // ANLZ-01: engine just became Idle (started, or a one-shot / prior
            // analyze-mode search finished) — resume pondering the current
            // position if Analyze Mode is on.
            scheduleAnalyzeModeRestart();
        }
    });

    // Start/Stop/Reload buttons in EngineStatusView.
    analysisPanel_.engineStatus().signal_start.connect([this]() {
        auto &cfg = gameState_.engineConfig();
        if (!cfg.enginePath.empty()) {
            controller_.startEngine();
            controller_.sendConfig();
        } else {
            onSettings();  // Open settings if no engine path.
        }
    });
    analysisPanel_.engineStatus().signal_stop.connect([this]() {
        // ENG-02: the analysis-panel Stop is the same manual intervention as
        // the toolbar Stop — cancel any armed auto-play (no-op if none).
        revertEnginePlaysToOff();
        controller_.stopEngine();
    });
    analysisPanel_.engineStatus().signal_reload.connect([this]() {
        controller_.reloadEngine();
    });

    // ANLZ-01: analysis-panel "∞" toggle — same handler as the menu checkbox.
    analysisPanel_.engineStatus().signal_analyze_mode_toggled.connect(
        [this](bool active) { onToggleAnalyzeMode(active); });

    // ENG-03: WM close button ("X") / titlebar close. Without this,
    // signal_close_request is unhandled, GTK's default close runs, main()
    // returns, and the heap-allocated MainWindow (application.cpp) is never
    // deleted — ~MainWindow/~EngineController/~EngineProcess never run, so
    // the engine subprocess never gets END or force_exit(). Route through
    // the same graceful shutdown as menu-Quit instead: veto this close,
    // start the async stop, and let its completion callback re-issue
    // close() (which re-triggers this handler — closeInFlight_ makes that
    // second pass fall through to GTK's real close).
    signal_close_request().connect([this]() -> bool {
        if (closeInFlight_) return false; // second pass: let GTK close now.
        closeInFlight_ = true;
        requestGracefulClose();
        return true; // veto this close; the stopEngine() completion re-issues it.
    }, false);
}

// ── Actions ──────────────────────────────────────────────────────────────────

// UX-03: New Game and board-size Apply both discard the current board,
// move history, and variation tree with no confirmation. Guard both behind
// this helper -- but only when there is actually something to lose (an
// empty board doesn't need a nag prompt). The dialog is fire-and-forget:
// `onConfirmed` runs from the response handler, and the dialog deletes
// itself when it closes.
void MainWindow::confirmDiscardGame(const Glib::ustring &action, std::function<void()> onConfirmed)
{
    if (gameState_.history().moveCount() == 0) {
        // Board already empty -- nothing to lose, don't nag.
        onConfirmed();
        return;
    }

    auto *dialog = new Gtk::MessageDialog(
        *this,
        action + " will discard the current game (board, move history, and variation tree). Continue?",
        /*use_markup=*/false,
        Gtk::MessageType::WARNING,
        Gtk::ButtonsType::YES_NO,
        /*modal=*/true);
    dialog->set_secondary_text("This cannot be undone.");
    dialog->signal_response().connect([dialog, onConfirmed](int response) {
        if (response == static_cast<int>(Gtk::ResponseType::YES)) {
            onConfirmed();
        }
        delete dialog;
    });
    dialog->set_visible(true);
}

void MainWindow::onNewGame()
{
    confirmDiscardGame("Starting a new game", [this]() {
        // STATE-04: keep the current (persisted) board size as the new-game
        // size rather than snapping back to DEFAULT_BOARD_SIZE. Still resync
        // the engine unconditionally -- the protocol may have last seen a
        // different size (PROTO-02), same as every other newGame() call site.
        gameState_.newGame(gameState_.boardSize());
        controller_.sendConfig();
    });
}

// IO-01: reuse the app's existing destructive-action / error-surfacing
// conventions -- a heap MessageDialog that deletes itself on response, same
// shape as confirmDiscardGame() above and the UX-02 validation feedback.
void MainWindow::showErrorDialog(const Glib::ustring &primary, const Glib::ustring &detail)
{
    auto *dialog = new Gtk::MessageDialog(*this, primary, /*use_markup=*/false,
                                          Gtk::MessageType::ERROR,
                                          Gtk::ButtonsType::OK, /*modal=*/true);
    if (!detail.empty()) dialog->set_secondary_text(detail);
    dialog->signal_response().connect([dialog](int) { delete dialog; });
    dialog->set_visible(true);
}

// RDB-02: Load Game. If the current game is non-empty, route through the same
// discard confirmation New Game uses. Inside the confirmed callback: pick a
// file, choose a reader by extension (`.rdb` -> RdbArchive, `.yxgame` ->
// legacy YxgameReader), decode it to a GameGraph and apply that to the model
// via rdb::applyGameGraphToState (newGame -> setRule -> rebuild the full
// variation tree -> replay mainline), then sendConfig() to resync the engine.
// Any parse/apply failure shows a visible error and leaves the current game
// untouched.
void MainWindow::onLoadGame()
{
    // Analyze Mode / an in-flight search would make newGame()/makeMove()
    // early-return; stop it first (mirrors onStopAnalysis()'s manual-stop).
    if (gameState_.isAnalyzing())
        onStopAnalysis();

    confirmDiscardGame("Loading a game", [this]() {
        auto dialog = Gtk::FileDialog::create();
        dialog->set_title("Load Game");

        auto rdbFilter = Gtk::FileFilter::create();
        rdbFilter->set_name("RANLS game (*.rdb)");
        rdbFilter->add_pattern("*.rdb");
        auto yxFilter = Gtk::FileFilter::create();
        yxFilter->set_name("Legacy game (*.yxgame)");
        yxFilter->add_pattern("*.yxgame");
        auto allFilter = Gtk::FileFilter::create();
        allFilter->set_name("All files");
        allFilter->add_pattern("*");
        auto filters = Gio::ListStore<Gtk::FileFilter>::create();
        filters->append(rdbFilter);
        filters->append(yxFilter);
        filters->append(allFilter);
        dialog->set_filters(filters);
        dialog->set_default_filter(rdbFilter);

        dialog->open(*this, [this, dialog](Glib::RefPtr<Gio::AsyncResult> &result) {
            std::string path;
            try {
                auto file = dialog->open_finish(result);
                if (!file) return;
                path = file->get_path();
            } catch (const Glib::Error &) {
                return; // user cancelled
            }

            auto        reader = rdb::archiveReaderFor(path);
            std::string err;
            auto        graph = reader->load(path, &err);
            if (!graph) {
                showErrorDialog("Could not load game", err);
                return;
            }
            if (!rdb::applyGameGraphToState(gameState_, *graph, &err)) {
                showErrorDialog("Could not load game", err);
                return;
            }
            controller_.sendConfig();
        });
    });
}

// RDB-02: Save Game. Serialises the whole variation tree (not just the played
// line) to a user-chosen `.rdb` path via rdb::toGameGraph + an RdbArchive
// writer. A write failure — or a non-`.rdb` target, for which there is no
// writer — shows a visible error dialog.
void MainWindow::onSaveGame()
{
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Save Game");
    dialog->set_initial_name("game.rdb");

    auto rdbFilter = Gtk::FileFilter::create();
    rdbFilter->set_name("RANLS game (*.rdb)");
    rdbFilter->add_pattern("*.rdb");
    auto filters = Gio::ListStore<Gtk::FileFilter>::create();
    filters->append(rdbFilter);
    dialog->set_filters(filters);
    dialog->set_default_filter(rdbFilter);

    dialog->save(*this, [this, dialog](Glib::RefPtr<Gio::AsyncResult> &result) {
        std::string path;
        try {
            auto file = dialog->save_finish(result);
            if (!file) return;
            path = file->get_path();
        } catch (const Glib::Error &) {
            return; // user cancelled
        }

        // A path with no extension defaults to `.rdb`.
        std::filesystem::path fsPath(path);
        if (fsPath.extension().empty())
            fsPath.replace_extension(".rdb");

        auto writer = rdb::archiveWriterFor(fsPath);
        if (!writer) {
            showErrorDialog("Could not save game",
                            "RANLS only saves games in the .rdb format.");
            return;
        }

        rdb::GraphMeta meta;
        meta.generator = kAppDisplayName;
        // RDB-03: one display-only engine entry from the current EngineConfig.
        // Referenced by per-node analysis (engineRef); never affects load
        // behaviour, and a missing/empty list must not fail a load.
        {
            const auto &ec = gameState_.engineConfig();
            if (!ec.enginePath.empty()) {
                rdb::EngineInfo ei;
                ei.id     = 0;
                ei.name   = std::filesystem::path(ec.enginePath).filename().string();
                ei.params = "threads=" + std::to_string(ec.threads)
                            + " hash=" + std::to_string(ec.hashSizeMB) + "MB";
                meta.engines.push_back(std::move(ei));
            }
        }
        const auto graph = rdb::toGameGraph(gameState_.tree(),
                                            gameState_.boardSize(),
                                            gameState_.rule(), meta);

        std::string err;
        if (!writer->save(fsPath, graph, &err))
            showErrorDialog("Could not save game", err);
    });
}

void MainWindow::onQuit()
{
    requestGracefulClose();
}

void MainWindow::requestGracefulClose()
{
    // ENG-01: stopEngine() is now asynchronous. Closing the window
    // immediately (as before) could destroy EngineController/EngineProcess
    // mid-shutdown, dangling the pending async completion. Wait for the
    // (non-blocking) stop to actually finish — or complete synchronously if
    // there was nothing to stop — before closing.
    //
    // ENG-03: shared by both the menu-Quit action and the signal_close_request
    // handler (WM "X" / titlebar close). The callback calls close(), never
    // onStopAnalysis() — a normal shutdown must not trip ENG-02's
    // auto-play-interrupt-reverts-to-manual behavior. If a shutdown is
    // already in flight (menu-Quit, or a prior close-request pass),
    // stopEngine() chains this completion onto it instead of firing early
    // (EngineController::stopEngine, state_ == Stopping) — so calling this
    // twice is safe and never double-sends END.
    controller_.stopEngine([this]() { close(); });
}

void MainWindow::onSetRule(GameRule rule)
{
    gameState_.setRule(rule);
    controller_.sendConfig();
    persistGameSetup();
}

// STATE-04: write the current rule + board size to the settings file. Every
// SettingsStorage::save() call must pass the *current* value of all four
// config blocks (STATE-02 hazard: save() truncates and rewrites the whole
// file, so a default-constructed block wipes the others).
void MainWindow::persistGameSetup()
{
    SettingsStorage::save(gameState_.engineConfig(), gameState_.viewConfig(),
                          gameState_.matchConfig(),
                          {gameState_.rule(), gameState_.boardSize()});
}

// UI-03: keeps the persistent header-bar rule indicator in sync with
// gameState_.rule(). Connected to gameState_.signal_rule_changed in
// connectSignals() and called once at startup in the constructor.
void MainWindow::updateRuleLabel()
{
    const char *text = "Rule: Freestyle Gomoku";
    switch (gameState_.rule()) {
        case GameRule::Freestyle: text = "Rule: Freestyle Gomoku"; break;
        case GameRule::Standard:  text = "Rule: Standard Gomoku";  break;
        case GameRule::Renju:     text = "Rule: Free Renju";       break;
    }
    ruleLabel_.set_text(text);
}

void MainWindow::onBoardSize()
{
    // CLEAN-01: dialog is heap-allocated (not Gtk::make_managed — it has no
    // parent container to own it) and previously never freed. hide_on_close
    // guarantees close()/user-close always route through hide(), so
    // signal_hide() is the one place to reclaim it — mirrors the common
    // gtkmm "delete self on response/hide" idiom for standalone dialogs.
    auto *dialog = new Gtk::Window();
    dialog->set_title("Board Size");
    dialog->set_transient_for(*this);
    dialog->set_modal(true);
    dialog->set_default_size(250, -1);
    dialog->set_hide_on_close(true);
    dialog->signal_hide().connect([dialog]() { delete dialog; });

    auto *box  = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    box->set_margin(16);
    auto *spin = Gtk::make_managed<Gtk::SpinButton>(
        Gtk::Adjustment::create(gameState_.boardSize(), 5, 22, 1));
    auto *btn  = Gtk::make_managed<Gtk::Button>("Apply");

    btn->add_css_class("suggested-action");
    btn->signal_clicked().connect([this, spin, dialog]() {
        int size = static_cast<int>(spin->get_value());
        // UX-03: changing the board size discards the game as a side effect
        // of what the user framed as a *setting* change, not "start a new
        // game" -- worse than the New Game case, so it gets the same guard.
        confirmDiscardGame("Changing the board size", [this, size]() {
            gameState_.newGame(size);
            controller_.sendConfig();
            // STATE-04: persist the new size as the new-game default. Inside
            // the confirm-success callback so a cancelled confirm saves
            // nothing, and after newGame() so boardSize() reflects the change.
            persistGameSetup();
        });
        dialog->close();
    });

    box->append(*Gtk::make_managed<Gtk::Label>("Board Size (5–22):"));
    box->append(*spin);
    box->append(*btn);
    dialog->set_child(*box);
    dialog->set_visible(true);
}

void MainWindow::onSettings()
{
    // CLEAN-01: see onBoardSize() above — same delete-on-hide pattern. A
    // fresh dialog is still constructed on every open (so it always reflects
    // the current EngineConfig/ViewConfig, unlike a reused-instance approach
    // that would show stale values from the first open), just no longer leaked.
    auto *dialog = new SettingsDialog(*this, gameState_.engineConfig(), gameState_.viewConfig());
    dialog->set_hide_on_close(true);
    dialog->signal_hide().connect([dialog]() { delete dialog; });
    dialog->signal_applied.connect([this](EngineConfig eConfig, ViewConfig vConfig) {
        bool pathChanged = (eConfig.enginePath != gameState_.engineConfig().enginePath);
        gameState_.setEngineConfig(eConfig);
        gameState_.setViewConfig(vConfig);
        // Preserve the UI-06 MatchConfig block and the STATE-04 GameSetupConfig
        // block — the Settings dialog owns neither, so pass their current values
        // through rather than letting save()'s default arguments reset
        // engine_plays / rule / board_size (STATE-02 hazard).
        SettingsStorage::save(eConfig, vConfig, gameState_.matchConfig(),
                              {gameState_.rule(), gameState_.boardSize()});

        if (pathChanged && !eConfig.enginePath.empty()) {
            // ENG-01: stopEngine() completes asynchronously now, so a
            // stopEngine(); startEngine(); pair back-to-back would have
            // startEngine() see state Stopping and no-op. reloadEngine()
            // already sequences stop -> start -> sendConfig() (only once
            // the stop has actually finished) via a completion callback.
            controller_.reloadEngine();
        } else {
            controller_.sendConfig();
        }
    });
    dialog->set_visible(true);
}

void MainWindow::onAbout()
{
    // UI-11: custom AboutDialog (src/ui/about_dialog.*) replaces the stock
    // Gtk::AboutDialog. This method just owns the CLEAN-01 lifetime: see
    // onBoardSize() above — same heap + delete-on-hide pattern.
    auto *dialog = new AboutDialog(*this);
    dialog->set_hide_on_close(true);
    dialog->signal_hide().connect([dialog]() { delete dialog; });
    dialog->set_visible(true);
}

void MainWindow::onStartAnalysis()
{
    // ENG-02: asking the engine to analyze while it is its own assigned turn is
    // a manual override of auto-play — quietly cancel the arrangement. Computed
    // from the board side-to-move (available regardless of engine state) so
    // both the "engine not running" and "already running" branches below
    // revert consistently.
    if (isEnginesTurn(gameState_.matchConfig().enginePlays,
                      gameState_.board().sideToMove()))
        revertEnginePlaysToOff();

    if (!engine_.isRunning()) {
        // Engine not running — start it first, but do NOT analyze yet.
        auto &cfg = gameState_.engineConfig();
        if (cfg.enginePath.empty()) {
            // ENG-01: reuse the same "open Settings" fallback used by
            // EngineStatusView's own Start button (see signal_start handler
            // above) instead of doing nothing silently.
            onSettings();
            return;
        }
        controller_.startEngine();
        controller_.sendConfig();
        controller_.analyze();
        return;
    }
    controller_.analyze();
}

void MainWindow::onStopAnalysis()
{
    // ENG-02: Stop (toolbar button or hotkey) is a manual intervention — cancel
    // any armed auto-play. The helper is a no-op when no side was assigned.
    revertEnginePlaysToOff();
    controller_.stopAnalysis();
}

void MainWindow::revertEnginePlaysToOff()
{
    MatchConfig mc = gameState_.matchConfig();
    if (mc.enginePlays == EnginePlaysSide::Off) return;  // nothing armed
    mc.enginePlays = EnginePlaysSide::Off;
    gameState_.setMatchConfig(mc);
    syncEnginePlaysMenu();
    // Deliberately NO SettingsStorage::save here — unlike onSetEnginePlays(),
    // this revert is a transient session action (ENG-02). The persisted,
    // user-chosen side is restored on next launch. No status message / toast.
}

// ── UI-06: "Engine plays <side>" auto-move ───────────────────────────────────
void MainWindow::onSetEnginePlays(EnginePlaysSide side)
{
    MatchConfig mc = gameState_.matchConfig();
    if (mc.enginePlays == side) { maybeStartAutoMove(); return; }
    mc.enginePlays = side;
    gameState_.setMatchConfig(mc);
    // Persist alongside the other config blocks (same pattern as onSettings()).
    // STATE-04: pass GameSetupConfig too so rule / board_size aren't wiped.
    SettingsStorage::save(gameState_.engineConfig(), gameState_.viewConfig(),
                          gameState_.matchConfig(),
                          {gameState_.rule(), gameState_.boardSize()});
    maybeStartAutoMove();
}

void MainWindow::syncEnginePlaysMenu()
{
    if (!enginePlaysAction_) return;
    const char *state = "off";
    switch (gameState_.matchConfig().enginePlays) {
        case EnginePlaysSide::Black: state = "black"; break;
        case EnginePlaysSide::White: state = "white"; break;
        case EnginePlaysSide::Off:   state = "off";   break;
    }
    enginePlaysAction_->change_state(Glib::Variant<Glib::ustring>::create(state));
}

void MainWindow::maybeStartAutoMove()
{
    if (autoMoveScheduled_) return;
    if (gameState_.matchConfig().enginePlays == EnginePlaysSide::Off) return;

    // Defer to an idle callback: signal_board_changed can fire many times in
    // one synchronous batch (a game load replays every move), and GameState
    // rejects a makeMove() while analyzing_ is set — so requesting a move
    // mid-batch would both fire on the wrong position and break the replay.
    // The flag coalesces the burst into a single deferred check.
    autoMoveScheduled_ = true;
    Glib::signal_idle().connect_once([this]() {
        autoMoveScheduled_ = false;

        const auto plays = gameState_.matchConfig().enginePlays;
        if (plays == EnginePlaysSide::Off) return;
        // ANLZ-05: while Analyze Mode is on the engine never auto-plays — not even
        // on its own assigned turn. It only ever analyses the current position
        // (scheduleAnalyzeModeRestart() now covers the engine's-turn position too).
        // This reverses planning.md Q6 for Analyze Mode; with Analyze Mode off the
        // auto-move / ENG-02 behaviour is unchanged.
        if (gameState_.viewConfig().analyzeMode) return;
        if (!engine_.isRunning()) return;
        if (controller_.engineState() != EngineController::EngineState::Idle) return;

        const Stone toMove = gameState_.board().sideToMove();
        if (!isEnginesTurn(plays, toMove)) return;  // ENG-02: shared predicate

        // After the engine's move lands, side-to-move flips to the other
        // colour, so this check fails next time — no infinite loop.
        controller_.requestEngineMove();
    });
}

// ── ANLZ-01: Analyze Mode (continuous background analysis) ────────────────────
void MainWindow::onToggleAnalyzeMode(bool active)
{
    ViewConfig vc = gameState_.viewConfig();
    if (vc.analyzeMode != active) {
        vc.analyzeMode = active;
        gameState_.setViewConfig(vc);
        // STATE-02: save() rewrites the whole file — pass every config block,
        // exactly like onSetEnginePlays(). persistGameSetup() already does this.
        persistGameSetup();
    }

    // Keep both toggle surfaces (menu checkbox + panel button) consistent.
    syncAnalyzeModeMenu();

    if (active) {
        // Turning it on kicks an immediate restart on the current position
        // (the idle-coalesced check re-verifies engine running / Idle / turn).
        // ANLZ-07: force=true — the user explicitly asked for a restart; any
        // cached "converged" result from before Analyze Mode was toggled off
        // must not suppress it.
        scheduleAnalyzeModeRestart(/*force=*/true);
    } else {
        // Q7: stop the current search, leave the process running. Orthogonal to
        // ENG-02 — deliberately NO revertEnginePlaysToOff() here.
        controller_.stopAnalysis();
    }
}

void MainWindow::syncAnalyzeModeMenu()
{
    const bool on = gameState_.viewConfig().analyzeMode;
    if (analyzeModeAction_)
        analyzeModeAction_->set_state(Glib::Variant<bool>::create(on));  // no re-emit
    analysisPanel_.engineStatus().setAnalyzeModeActive(on);
}

void MainWindow::scheduleAnalyzeModeRestart(bool force)
{
    if (!gameState_.viewConfig().analyzeMode) return;

    // ANLZ-07: latch `force` across coalesced calls — see analyzeModeForce_'s
    // doc comment. A later non-forced call must never downgrade an earlier
    // forced one still waiting on the idle callback.
    if (force) analyzeModeForce_ = true;

    if (analyzeModeScheduled_) return;

    // Defer to a single idle callback — same rationale as maybeStartAutoMove():
    // signal_board_changed can fire many times in one synchronous batch (a game
    // load replays every move; undoAll/redoAll step ply by ply), and the engine
    // must analyse only the final settled position, once.
    analyzeModeScheduled_ = true;
    Glib::signal_idle().connect_once([this]() {
        analyzeModeScheduled_ = false;
        const bool doForce = analyzeModeForce_;
        analyzeModeForce_ = false;

        if (!gameState_.viewConfig().analyzeMode) return;
        if (!engine_.isRunning()) return;
        if (controller_.engineState() != EngineController::EngineState::Idle) return;

        // ANLZ-05: the engine's-turn position is analysed too. The old
        // `isEnginesTurn(...) return;` bail existed only to hand that position to
        // maybeStartAutoMove(), which no longer runs while Analyze Mode is on
        // (planning.md Q6 reversed). Analyze Mode is now a pure study mode.

        // ANLZ-07: skip re-arming when the search that just finished on this
        // position already converged to the same result as the one before it
        // — nothing new to find, and re-running it would just repeat the
        // same YXBOARD+YXNBEST request/response forever (the busy-loop this
        // task fixes). `doForce` (a genuine position change, or the user
        // explicitly toggling Analyze Mode off/on) always bypasses this —
        // never let a stale cached result suppress analysing a position the
        // user actually asked to (re)study.
        if (!doForce && controller_.analysisConverged()) return;

        // Restart order matters: analyze() early-returns unless state == Idle,
        // so stopAnalysis() (Idle + RT-01 flush) must precede it.
        controller_.stopAnalysis();
        controller_.analyze();
    });
}

void MainWindow::onUndoAll()
{
    gameState_.undoAll();
}

void MainWindow::onUndo()
{
    gameState_.undoMove();
}

void MainWindow::onRedo()
{
    gameState_.redoMove();
}

void MainWindow::onRedoAll()
{
    gameState_.redoAll();
}
