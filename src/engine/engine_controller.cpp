#include "engine_controller.h"
#include "gomocup_protocol.h"
#include <iostream>

EngineController::EngineController(GameState &gameState, EngineProcess &engine)
    : gameState_(gameState)
    , engine_(engine)
{
    // Default to Gomocup protocol. In the future this could be configurable.
    protocol_ = std::make_unique<GomocupProtocol>(gameState_.boardSize());
    connectProtocolSignals();

    engine_.signal_line_received.connect(
        sigc::mem_fun(*this, &EngineController::onEngineLine));

    engine_.signal_process_died.connect([this]() {
        // Only a real transition: dying while NotStarted (nothing to crash)
        // or already Crashed/Stopping-to-completion is not a fresh event.
        if (state_ == EngineState::NotStarted || state_ == EngineState::Crashed) return;
        gameState_.setAnalyzing(false);
        // ANLZ-06: a dead process cannot deliver any further coordinate line
        // for whatever search was in flight — inert it so a stray line from
        // a half-flushed pipe buffer can't be misread as a real move.
        searchIntent_ = SearchIntent::None;
        // PROTO-04: a dead process will never deliver the trailing coordinate
        // sendOrDefer() is waiting on either — drop whatever was queued
        // rather than holding it (and blocking every future command) forever.
        pendingStopFlush_ = false;
        pendingActions_.clear();
        setState(EngineState::Crashed);
    });
}

EngineController::~EngineController()
{
    // No GLib main loop is guaranteed to be running by the time this runs, so
    // we cannot rely on stopEngine()'s async completion callback ever firing.
    // Force-kill synchronously instead (EngineProcess::stop() — immediate,
    // no g_usleep, no g_main_context_iteration). This is deliberately not
    // stopEngine(): see docs/instruction/ENG-01-engine-state-honesty-and-blocking-stop.md.
    engine_.stop();
}

void EngineController::connectProtocolSignals() {
    protocol_->signal_log.connect([this](EngineMessageType type, std::string line) {
        std::string typeStr;
        switch (type) {
            case EngineMessageType::Message: typeStr = "Message"; break;
            case EngineMessageType::Coord:   typeStr = "Coord"; break;
            case EngineMessageType::Error:   typeStr = "Error"; break;
            case EngineMessageType::Debug:   typeStr = "Debug"; break;
            default:                         typeStr = "Output"; break;
        }
        signal_engine_output.emit(typeStr, line);
    });

    protocol_->signal_move.connect([this](Coord move) {
        const bool wasSearching = (state_ == EngineState::Analyzing);
        // ANLZ-06: capture and consume the intent before deciding anything.
        // Both analyze() (YXNBEST) and requestEngineMove() end in the engine
        // emitting a bare coordinate line the same way, so state_ alone
        // cannot tell "search-completion marker, discard" apart from "a real
        // move the caller asked for, play it". Reset to None immediately so
        // a second/trailing coordinate line for the same finished search
        // (or one arriving after a stop already reset it) can't be
        // misread as a fresh request.
        const SearchIntent intent = searchIntent_;
        searchIntent_ = SearchIntent::None;

        if (wasSearching) {
            // UI-13 ordering: the search is over, but the board is still on the
            // just-searched position and no state-change handler has run yet.
            //  1. clear analyzing_ so GameState::makeMove() (driven by
            //     signal_engine_move below, for a Move-intent search) is
            //     accepted;
            //  2. flush the coalesced analysis for the searched position NOW —
            //     RT-01: immediate, not waiting for a tick, exactly one flush
            //     on completion — so its final PV/eval lands on that position's
            //     node before the board advances;
            //  3. THEN play the move, if intent says to (board advances, tree
            //     node for the reply ply is created);
            //  4. THEN transition to Idle — so state-change handlers
            //     (auto-move, control sensitivity) observe the played move and
            //     the already-delivered final analysis, and cannot preempt the
            //     flush for the searched position. Idle also re-enters
            //     scheduleAnalyzeModeRestart() via signal_state_changed, so a
            //     discarded Analysis-intent coordinate simply re-ponders the
            //     same position instead of being lost.
            gameState_.setAnalyzing(false);
            gameState_.flush();
        }

        // ANLZ-07: an analysis-intent search just completed — capture its
        // result (best move + eval text, already parsed into GameState by
        // signal_analysis) and compare it to the previous completed
        // analysis-intent result for this exact position. Identical means
        // the search has converged and re-running it would just repeat the
        // same request/response forever (the busy-loop this task fixes);
        // MainWindow::scheduleAnalyzeModeRestart() reads analysisConverged()
        // before re-arming. This must happen before the intent is discarded
        // below and regardless of whether the coordinate itself is played.
        if (wasSearching && intent == SearchIntent::Analysis) {
            auto path = gameState_.currentPath();
            AnalysisResult result = captureAnalysisResult();
            lastAnalysisConverged_ = haveLastAnalysis_
                && lastAnalysisPath_ == path
                && lastAnalysisResult_ == result;
            lastAnalysisPath_ = std::move(path);
            lastAnalysisResult_ = result;
            haveLastAnalysis_ = true;
        }

        // ANLZ-06: only a genuinely requested move (requestEngineMove(), the
        // ENG-02/UI-06 "engine plays <side>" path) is ever played. An
        // Analysis-intent coordinate — analyze()'s YXNBEST search reaching
        // its best move — is a search-completion marker only, never a move
        // the user or the auto-move logic asked for; playing it is exactly
        // the ANLZ-06 bug. Intent None (e.g. a late line after stopAnalysis()
        // already reset it) is likewise discarded.
        if (intent == SearchIntent::Move)
            signal_engine_move.emit(move);

        if (wasSearching)
            setState(EngineState::Idle);

        // PROTO-04: any coordinate line arriving while pendingStopFlush_ is
        // set IS the trailing reply sendOrDefer() is waiting on — nothing
        // else could produce one, since everything queued behind it is held
        // unsent until this fires. The wire is confirmed idle now; flush
        // whatever queued up in order. Swap the queue out to a local first:
        // an action can itself call sendOrDefer() again (e.g. analyze()),
        // which must see an empty queue and send immediately, not re-append
        // to the batch already being flushed.
        if (pendingStopFlush_) {
            pendingStopFlush_ = false;
            auto actions = std::move(pendingActions_);
            pendingActions_.clear();
            for (auto &action : actions) action();
        }
    });

    protocol_->signal_analysis.connect([this](const std::vector<PVLine>& pvs, const EngineStatus& status) {
        // UI-04: only accept analysis while a search is actually in flight.
        // After STOP (or natural completion) the engine can still emit a few
        // trailing MESSAGE/INFO lines for the just-finished position; if the
        // user has meanwhile changed position, forwarding those would push the
        // previous position's PV rows onto the new one.
        if (!gameState_.isAnalyzing()) return;
        gameState_.setAnalysisData(pvs, status);
    });

    // UI-04: a position change (move / undo / redo / New Game / load) discards
    // any analysis the protocol is still holding for the old position. The
    // model side (GameState::resetAnalysisState, STATE-01) already clears
    // pvLines_; this clears the upstream protocol buffer that feeds it so a
    // late async engine line can't refill it.
    gameState_.signal_board_changed.connect([this]() {
        protocol_->clearAnalysisState();
    });

    protocol_->signal_database_entry.connect([this](const DatabaseEntry& entry) {
        signal_database_entry.emit(entry);
    });
    protocol_->signal_database_refresh.connect([this]() {
        signal_database_refresh.emit();
    });
    protocol_->signal_database_done.connect([this]() {
        signal_database_done.emit();
    });
}

void EngineController::setState(EngineState next)
{
    if (state_ == next) return;
    state_ = next;
    signal_state_changed.emit(state_);
}

bool EngineController::isUsable() const
{
    return state_ == EngineState::Starting || state_ == EngineState::Idle
        || state_ == EngineState::Analyzing;
}

// ─── Engine lifecycle ────────────────────────────────────────────────────────
void EngineController::startEngine()
{
    // Already starting/running: no-op. Crashed is a legitimate restart point.
    if (state_ != EngineState::NotStarted && state_ != EngineState::Crashed) return;

    const auto &cfg = gameState_.engineConfig();
    if (cfg.enginePath.empty()) return;

    setState(EngineState::Starting);

    bool ok = engine_.start(cfg.enginePath);
    if (!ok) {
        // ENG-01: honor EngineProcess::start()'s return value. Do not claim
        // a running state, and surface a visible error naming the path that
        // failed (routed to the bottom-panel console via signal_engine_output,
        // the same path used for all other engine-originated messages).
        signal_engine_output.emit("Error", "Failed to start engine: " + cfg.enginePath);
        setState(EngineState::NotStarted);
        return;
    }

    setState(EngineState::Idle);
}

void EngineController::stopEngine(std::function<void()> onComplete)
{
    if (state_ == EngineState::NotStarted) {
        if (onComplete) onComplete();
        return;
    }

    if (state_ == EngineState::Stopping) {
        // Already stopping — chain this caller's completion onto the
        // in-flight shutdown instead of firing it early. Firing early would
        // let a caller (e.g. MainWindow::onQuit()) close/destroy `this`
        // before the original async stop has actually settled.
        if (onComplete) {
            auto prev = std::move(pendingStopComplete_);
            pendingStopComplete_ = [prev, onComplete]() {
                if (prev) prev();
                onComplete();
            };
        }
        return;
    }

    if (engine_.isRunning()) {
        sendRawCommand("YXSAVEDATABASE");
        sendRawCommand("END");
    }

    gameState_.setAnalyzing(false);
    // ANLZ-06: same reasoning as stopAnalysis() — a shutdown must also inert
    // any trailing coordinate line an in-flight search still emits.
    searchIntent_ = SearchIntent::None;
    // PROTO-04: shutting down regardless — nothing queued behind
    // sendOrDefer() will ever get a wire to send on, so drop it now instead
    // of leaking it (or, worse, having it fire mid/after teardown).
    pendingStopFlush_ = false;
    pendingActions_.clear();
    setState(EngineState::Stopping);

    // Non-blocking: no g_usleep, no g_main_context_iteration (ENG-01). The UI
    // stays responsive and shows "Stopping" until this completes, either when
    // the process exits or when EngineProcess's grace-period timeout forces it.
    //
    // This completion callback may run long after this call returns (see
    // EngineProcess::stopAsync()) — possibly after `this` has been destroyed.
    // Guard with a weak_ptr to lifetimeGuard_ rather than touching `this`
    // unconditionally.
    std::weak_ptr<void> guard = lifetimeGuard_;
    engine_.stopAsync([this, guard, onComplete]() {
        if (guard.expired()) return; // EngineController was destroyed meanwhile.
        setState(EngineState::NotStarted);
        auto pending = std::move(pendingStopComplete_);
        if (onComplete) onComplete();
        if (pending) pending();
    });
}

void EngineController::reloadEngine()
{
    stopEngine([this]() {
        startEngine();
        if (state_ == EngineState::Idle) sendConfig();
    });
}

void EngineController::sendConfig()
{
    // PROTO-02: generateStart() is also what keeps protocol_'s cached
    // boardSize_ in sync with the model. Call it unconditionally -- even
    // when the engine isn't usable yet -- so a board-size change made while
    // the engine is stopped doesn't leave the protocol's parser working off
    // a stale size until some later successful sendConfig(). Only the actual
    // wire commands are gated on isUsable(); a not-yet-started engine has
    // nothing to send them to.
    auto startCmds = protocol_->generateStart(gameState_.boardSize());

    if (!isUsable()) return;

    const auto &cfg = gameState_.engineConfig();
    auto configCmds = protocol_->generateConfig(cfg);
    auto ruleCmds   = protocol_->generateRule(gameState_.rule());

    // PROTO-04: a New Game / rule / board-size change routinely follows
    // stopAnalysis() (see onNewGame()/onSetRule()) — defer behind any
    // still-settling aborted search exactly like the other command methods,
    // so a fresh START can't land on the wire ahead of the old search's
    // trailing coordinate.
    sendOrDefer([this, startCmds = std::move(startCmds),
                 configCmds = std::move(configCmds),
                 ruleCmds = std::move(ruleCmds)]() {
        for (const auto& cmd : startCmds)  engine_.sendLine(cmd);
        for (const auto& cmd : configCmds) engine_.sendLine(cmd);
        for (const auto& cmd : ruleCmds)   engine_.sendLine(cmd);
    });
}

// ─── Analysis ────────────────────────────────────────────────────────────────
void EngineController::analyze()
{
    if (state_ != EngineState::Idle) return;

    auto path = gameState_.currentPath();
    const auto &cfg = gameState_.engineConfig();

    // PROTO-04: defer both the wire commands AND the intent/state bookkeeping
    // until they can actually run — flipping searchIntent_/state_ to
    // Analyzing now, while the previous search's trailing coordinate is
    // still in flight, would make the signal_move handler misattribute that
    // stale coordinate to *this* not-yet-sent search.
    sendOrDefer([this, path, multiPV = cfg.multiPV]() {
        for (const auto& cmd : protocol_->generateAnalyzeRequest(path, multiPV)) {
            engine_.sendLine(cmd);
        }
        // ANLZ-06: this search's terminal coordinate line (YXNBEST still
        // emits one) must be discarded, not played — see the signal_move
        // handler.
        searchIntent_ = SearchIntent::Analysis;
        gameState_.setAnalyzing(true);
        setState(EngineState::Analyzing);
    });
}

void EngineController::requestEngineMove()
{
    if (state_ != EngineState::Idle) return;

    auto path = gameState_.currentPath();

    // PROTO-04: same deferral as analyze(), same reasoning.
    sendOrDefer([this, path]() {
        for (const auto& cmd : protocol_->generateMoveRequest(path)) {
            engine_.sendLine(cmd);
        }
        // Same state bookkeeping as analyze(): the engine is now searching,
        // and position mutations must be blocked until its move arrives.
        // protocol_->signal_move (wired in connectProtocolSignals) flips us
        // back to Idle and emits signal_engine_move when the reply comes in.
        // ANLZ-06: this search's terminal coordinate is a real requested
        // move — signal_move must play it, unlike an analyze()-driven search.
        searchIntent_ = SearchIntent::Move;
        gameState_.setAnalyzing(true);
        setState(EngineState::Analyzing);
    });
}

void EngineController::stopAnalysis()
{
    // Always forward STOP when the engine is usable.
    if (!isUsable()) return;

    engine_.sendLine(protocol_->generateStop());

    // PROTO-04: only an aborted Analysis-intent (YXNBEST) search still
    // emits a trailing coordinate line after STOP — the ANLZ-06 comment
    // above ("YXNBEST still emits one") is exactly this observed exception.
    // A Move-intent (BOARD) search's reply is genuinely suppressed by STOP
    // per docs/protocol.md's STOP entry, so there is nothing to wait for
    // there — arming pendingStopFlush_ for it would hang every future
    // command behind a coordinate that is never coming.
    const bool willEmitTrailingCoord =
        state_ == EngineState::Analyzing && searchIntent_ == SearchIntent::Analysis;

    // ANLZ-06: unconditional, regardless of state_ — inert any trailing
    // coordinate line the aborted search still emits after STOP. state_
    // alone is not enough: by the time that line arrives state_ is already
    // Idle (set just below), so wasSearching in the signal_move handler
    // would read false either way; it's this intent reset that makes the
    // line inert rather than accidentally falling through as "Move".
    searchIntent_ = SearchIntent::None;

    if (state_ == EngineState::Analyzing) {
        // PROTO-04: from here on, every sendOrDefer() caller (queryDatabase(),
        // the next analyze(), sendConfig(), ...) queues instead of sending,
        // until the flush below runs — closing the race this fixes: state_
        // says Idle immediately (below), but the real subprocess's aborted
        // search has not actually settled yet.
        pendingStopFlush_ = willEmitTrailingCoord;
        gameState_.setAnalyzing(false);
        setState(EngineState::Idle);
        // RT-01: analysis-stopped must emit immediately, not wait for the
        // next throttle tick.
        gameState_.flush();
    }
}

void EngineController::queryDatabase() {
    if (!isUsable()) return;
    auto cmds = protocol_->generateDatabaseQuery(gameState_.currentPath());
    // PROTO-04: this is the call site that surfaced the race — see the
    // sendOrDefer() doc comment and docs/fix-log/<date>-proto-04-*.md.
    sendOrDefer([this, cmds = std::move(cmds)]() {
        for (const auto& cmd : cmds) {
            engine_.sendLine(cmd);
        }
    });
}

void EngineController::sendOrDefer(std::function<void()> action)
{
    if (pendingStopFlush_) {
        pendingActions_.push_back(std::move(action));
        return;
    }
    action();
}

void EngineController::sendRawCommand(const std::string &command)
{
    if (!isUsable()) return;
    engine_.sendLine(command);
}

// ANLZ-07: same "best move + eval" derivation EngineStatusView::update() uses
// for display (src/ui/engine_status.cpp) — prefer pvLines()[0] when present
// (it's the authoritative current-round best line), fall back to the raw
// EngineStatus otherwise. No new engine query: both are already parsed into
// GameState by the protocol's signal_analysis handler above.
EngineController::AnalysisResult EngineController::captureAnalysisResult() const
{
    const auto &pvs = gameState_.pvLines();
    const auto &status = gameState_.engineStatus();
    const PVLine *bestPv = !pvs.empty() && !pvs[0].moves.empty() ? &pvs[0] : nullptr;

    AnalysisResult result;
    result.bestMove = bestPv ? bestPv->moves.front() : status.bestMove;
    result.evalText = bestPv ? bestPv->evalText : status.evalText;
    return result;
}

// ─── Line parsing ────────────────────────────────────────────────────────────
void EngineController::onEngineLine(const std::string &line)
{
    // Simply pipe to protocol parser. The protocol emits domain signals.
    protocol_->parseLine(line);
}
