#include "main_window.h"
#include "model/settings_storage.h"
#include "ui/settings_dialog.h"

#include <iostream>
#include <algorithm>
#include <cctype>

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
    set_title("Rapfi Analysis");
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

    // Load persisted user settings (with defaults fallback).
    auto saved = SettingsStorage::load();
    gameState_.setEngineConfig(saved.engine);
    gameState_.setViewConfig(saved.view);

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

    auto ruleSubmenu = Gio::MenuItem::create("Rule", ruleSection);
    gameMenu->append_submenu("Rule", ruleSection);
    gameMenu->append("Board Size",  "win.board-size");

    auto gameSection2 = Gio::Menu::create();
    gameSection2->append("Quit", "win.quit");
    gameMenu->append_section("", gameSection2);

    // Players menu.
    auto playersMenu = Gio::Menu::create();
    playersMenu->append("Settings…", "win.settings");

    // Analysis menu.
    auto analysisMenu = Gio::Menu::create();
    analysisMenu->append("▶ Analyze",  "win.analyze");
    analysisMenu->append("■ Stop",     "win.stop");

    // Help menu.
    auto helpMenu = Gio::Menu::create();
    helpMenu->append("About", "win.about");

    menuModel->append_submenu("Game",     gameMenu);
    menuModel->append_submenu("Players",  playersMenu);
    menuModel->append_submenu("Analysis", analysisMenu);
    menuModel->append_submenu("Help",     helpMenu);

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
}

// ─── Signal wiring ───────────────────────────────────────────────────────────
void MainWindow::connectSignals()
{
    // Board click → place a move.
    boardView_.signal_move_clicked.connect([this](Coord pos) {
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
        // Apply theme preferences.
        auto theme = gameState_.viewConfig().theme;
        auto settings = Gtk::Settings::get_default();
        if (settings) {
            if (theme == AppTheme::Dark) {
                settings->property_gtk_application_prefer_dark_theme() = true;
            } else if (theme == AppTheme::Light) {
                settings->property_gtk_application_prefer_dark_theme() = false;
            } else { // System
                settings->reset_property("gtk-application-prefer-dark-theme");
            }
        }

        // Apply board view changes (Move numbers, Coordinates).
        boardViewModel_.update();
        boardView_.queueRedraw();

        // Apply a simple profile preset for layout split.
        auto profile = gameState_.viewConfig().uiProfile;
        if (profile == "Compact") {
            mainVPaned_.set_position(640);
            mainHPaned_.set_position(700);
        } else if (profile == "Review") {
            mainVPaned_.set_position(520);
            mainHPaned_.set_position(560);
        } else {
            mainVPaned_.set_position(580);
            mainHPaned_.set_position(640);
        }
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
        controller_.stopEngine();
    });
    analysisPanel_.engineStatus().signal_reload.connect([this]() {
        controller_.reloadEngine();
    });
}

// ── Actions ──────────────────────────────────────────────────────────────────
void MainWindow::onNewGame()
{
    std::cerr << "[DBG] onNewGame called" << std::endl;
    // newGame() resets to DEFAULT_BOARD_SIZE, which can differ from whatever
    // size the engine protocol last saw (PROTO-02) -- resync unconditionally,
    // same as every other newGame()/board-size call site.
    gameState_.newGame();
    controller_.sendConfig();
}

void MainWindow::onLoadGame()
{
    // TODO: file chooser dialog
}

void MainWindow::onSaveGame()
{
    // TODO: file save dialog
}

void MainWindow::onQuit()
{
    // ENG-01: stopEngine() is now asynchronous. Closing the window
    // immediately (as before) could destroy EngineController/EngineProcess
    // mid-shutdown, dangling the pending async completion. Wait for the
    // (non-blocking) stop to actually finish — or complete synchronously if
    // there was nothing to stop — before closing.
    controller_.stopEngine([this]() { close(); });
}

void MainWindow::onSetRule(GameRule rule)
{
    gameState_.setRule(rule);
    controller_.sendConfig();
}

void MainWindow::onBoardSize()
{
    // Simple dialog to input board size.
    auto *dialog = new Gtk::Window();
    dialog->set_title("Board Size");
    dialog->set_transient_for(*this);
    dialog->set_modal(true);
    dialog->set_default_size(250, -1);

    auto *box  = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    box->set_margin(16);
    auto *spin = Gtk::make_managed<Gtk::SpinButton>(
        Gtk::Adjustment::create(gameState_.boardSize(), 5, 22, 1));
    auto *btn  = Gtk::make_managed<Gtk::Button>("Apply");

    btn->add_css_class("suggested-action");
    btn->signal_clicked().connect([this, spin, dialog]() {
        int size = static_cast<int>(spin->get_value());
        gameState_.newGame(size);
        controller_.sendConfig();
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
    auto *dialog = new SettingsDialog(*this, gameState_.engineConfig(), gameState_.viewConfig());
    dialog->signal_applied.connect([this](EngineConfig eConfig, ViewConfig vConfig) {
        bool pathChanged = (eConfig.enginePath != gameState_.engineConfig().enginePath);
        gameState_.setEngineConfig(eConfig);
        gameState_.setViewConfig(vConfig);
        SettingsStorage::save(eConfig, vConfig);

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
    auto *dialog = new Gtk::AboutDialog();
    dialog->set_transient_for(*this);
    dialog->set_modal(true);
    dialog->set_program_name("Rapfi Analysis");
    dialog->set_version("2.0");
    dialog->set_comments("Professional Gomoku Analysis Tool");
    dialog->set_visible(true);
}

void MainWindow::onStartAnalysis()
{
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
    controller_.stopAnalysis();
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
