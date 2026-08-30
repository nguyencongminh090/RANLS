// UI-07: the PV panel must never carry a row from a previous position into
// the current position's analysis. This exercises the REAL compact
// "MESSAGE depth N-M ev X n .. n/ms .. tm .. pv .." format Rapfi emits under
// INFO SHOW_DETAIL 0 (the UI-04 tests only used the synthetic
// "MESSAGE (n) .. | .. | .." format, which is why UI-07 shipped).
//
// The harness mirrors EngineController::connectProtocolSignals + the
// MainWindow signal_engine_move -> GameState::makeMove wiring:
//   - signal_analysis is forwarded to GameState::setAnalysisData ONLY while
//     GameState::isAnalyzing() (UI-04);
//   - GameState::signal_board_changed drives GomocupProtocol::clearAnalysisState()
//     (UI-04);
//   - GomocupProtocol::signal_move drives GameState::makeMove() (the engine
//     playing its own move after STOP).
// The PV vector the UI renders is whatever GameState::pvLines() holds after a
// tick/flush.

#include "vendor/doctest.h"

#include "model/game_state.h"
#include "engine/gomocup_protocol.h"

#include <string>
#include <vector>

namespace {

struct Harness {
    GomocupProtocol proto;
    GameState       gs;

    explicit Harness(int boardSize = 15) : proto(boardSize), gs(boardSize) {
        proto.signal_analysis.connect(
            [this](const std::vector<PVLine> &pvs, const EngineStatus &status) {
                if (gs.isAnalyzing())
                    gs.setAnalysisData(pvs, status);
            });
        gs.signal_board_changed.connect([this]() { proto.clearAnalysisState(); });
        proto.signal_move.connect([this](Coord c) { gs.makeMove(c); });
    }

    void feed(const std::vector<std::string> &lines) {
        for (const auto &l : lines) proto.parseLine(l);
    }

    // UI-facing PV vector after coalesced update lands.
    const std::vector<PVLine> &uiPVs() { gs.flush(); return gs.pvLines(); }
};

// Every coordinate appearing in a PV, for cross-position leak detection.
std::vector<Coord> allMoves(const std::vector<PVLine> &pvs) {
    std::vector<Coord> out;
    for (const auto &pv : pvs)
        for (auto c : pv.moves) out.push_back(c);
    return out;
}

} // namespace

TEST_CASE("UI-07: compact MESSAGE-depth format, MultiPV=1 keeps exactly one row per position") {
    Harness h;

    // ---- Position 1: three stones (12,13 / 13,13 / 13,10) ----
    REQUIRE(h.gs.makeMove(Coord{13, 12}));
    REQUIRE(h.gs.makeMove(Coord{13, 13}));
    REQUIRE(h.gs.makeMove(Coord{10, 13}));

    h.gs.setAnalyzing(true);
    for (const auto &cmd : h.proto.generateAnalyzeRequest(h.gs.currentPath(), 1))
        (void)cmd;
    h.feed({
        "MESSAGE depth 2-3 ev -5 n 498 n/ms 498 tm 0 pv L3 K5",
        "MESSAGE depth 10-15 ev -9 n 42K n/ms 1425 tm 30 pv K5 M4 L4 J6 M3 O1 L5 J5 J4 L6",
        "MESSAGE depth 21-38 ev 6 n 5117K n/ms 1674 tm 3056 pv K5 L4 J4 L6 L5 J5 I6 M5 K3 K7 J8 I5",
    });

    {
        const auto &pvs = h.uiPVs();
        REQUIRE(pvs.size() == 1);
        CHECK(pvs[0].moves.size() == 12);
        CHECK(pvs[0].moves.front() == Coord{10, 10}); // K5
    }
    auto pos1Moves = allMoves(h.gs.pvLines());

    // ---- STOP, then the engine plays its own move 10,10 ----
    h.gs.setAnalyzing(false);
    h.gs.flush();
    h.proto.parseLine("STOP");           // no-op for the parser
    h.proto.parseLine("10,10");           // engine move -> signal_move -> makeMove

    // The move landed and cleared the previous analysis.
    REQUIRE(h.gs.history().currentIndex() == 3); // 0-based: 3 stones + engine move
    CHECK(h.gs.pvLines().empty());

    // ---- Position 2: four stones, fresh analysis ----
    h.gs.setAnalyzing(true);
    for (const auto &cmd : h.proto.generateAnalyzeRequest(h.gs.currentPath(), 1))
        (void)cmd;
    h.feed({
        "MESSAGE depth 2-4 ev 20 n 394 n/ms 394 tm 0 pv L4 J4 L6",
        "MESSAGE depth 15-26 ev -3 n 133K n/ms 1587 tm 84 pv L4 J4 L6 L5 J5 I6 M5 K3 K7 J8 I5 H6",
        "MESSAGE depth 22-44 ev -12 n 16M n/ms 1741 tm 9663 pv J4 K6 K3 K7 K4 L4 M3 L3 I5 L2 L5 H6",
    });

    const auto &pvs = h.uiPVs();

    // Exactly one row, and it is position 2's line (starts J4), never a
    // carried-over position-1 row (which started K5).
    REQUIRE(pvs.size() == 1);
    CHECK(pvs[0].moves.front() == Coord{9, 11}); // J4

    // No coordinate from position 1's line survives that isn't also in
    // position 2's line start.
    for (auto c : allMoves(pvs))
        CHECK(c != Coord{10, 10}); // K5 was position 1's leading move
}

TEST_CASE("UI-07: a trailing status-only line for the previous position cannot repaint stale PV rows") {
    Harness h;

    REQUIRE(h.gs.makeMove(Coord{7, 7}));
    h.gs.setAnalyzing(true);
    h.feed({
        "MESSAGE depth 5-6 ev 21 n 2618 n/ms 1309 tm 2 pv M4 K4 K5 J3 M3",
        "MESSAGE depth 12-17 ev -1 n 65K n/ms 1427 tm 46 pv K5 M4 L5 L3 N5 M5 M3 L4 J4 L6 K4",
    });
    REQUIRE(h.uiPVs().size() == 1);

    // STOP + engine move -> position change clears everything.
    h.gs.setAnalyzing(false);
    h.gs.flush();
    h.proto.parseLine("10,10");
    REQUIRE(h.gs.pvLines().empty());

    // New analysis; the very first line the engine sends carries NO pv token
    // (pure status). It must not resurrect the previous position's PV vector.
    h.gs.setAnalyzing(true);
    for (const auto &cmd : h.proto.generateAnalyzeRequest(h.gs.currentPath(), 1)) (void)cmd;
    h.proto.parseLine("MESSAGE depth 1-1 ev 0 n 10 n/ms 10 tm 0");
    CHECK(h.uiPVs().empty());

    h.proto.parseLine("MESSAGE depth 4-5 ev 20 n 1334 n/ms 1334 tm 1 pv L4 J4 L6 L5");
    const auto &pvs = h.uiPVs();
    REQUIRE(pvs.size() == 1);
    CHECK(pvs[0].moves.front() == Coord{11, 11}); // L4
}
