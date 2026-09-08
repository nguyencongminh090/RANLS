// Regression tests for PROTO-06: the live per-cell search overlay (replaces the
// old BoardViewModel::candidateMoves / BoardRenderer::drawCandidateMoves).
//
// The overlay is fed by two engine streams through the real GomocupProtocol:
//   - INFO PV DONE  -> a per-cell winrate/mate `tag` on each PV's first move,
//                      stamped with the root depth it was written at
//                      (`tagDepth`); on the last PV of a depth round, tags
//                      written at an earlier depth are cleared (candidates that
//                      dropped out of the top-N).
//   - MESSAGE REALTIME POS/DONE/LOST/BEST/REFRESH -> per-cell pos (0/1/2), a
//                      `lost` flag, and a single `bestMove`; REFRESH clears
//                      only pos.
//
// It is wired exactly like EngineController::connectProtocolSignals: the
// protocol's signal_analysis_overlay feeds GameState::setAnalysisOverlay (which
// only marks a dirty flag), and GameState::tickAnalysis()/flush() coalesce the
// burst into one signal_analysis_overlay emission. Nothing here goes through
// setAnalysisData (no tree-node / eval-history writes).
//
// This transcript IS the permanent fixture -- do not delete it.

#include "vendor/doctest.h"

#include "model/game_state.h"
#include "model/board_view_model.h"
#include "engine/gomocup_protocol.h"

#include <string>
#include <vector>

namespace {

struct Harness {
    GomocupProtocol proto;
    GameState       gs;
    int overlayEmits = 0;   ///< GameState::signal_analysis_overlay emissions (coalesced)

    explicit Harness(int boardSize = 15) : proto(boardSize), gs(boardSize) {
        proto.signal_analysis.connect(
            [this](const std::vector<PVLine> &pvs, const EngineStatus &status) {
                if (gs.isAnalyzing()) gs.setAnalysisData(pvs, status);
            });
        proto.signal_analysis_overlay.connect([this](const AnalysisOverlay &ov) {
            if (gs.isAnalyzing()) gs.setAnalysisOverlay(ov);
        });
        gs.signal_board_changed.connect([this]() { proto.clearAnalysisState(); });
        gs.signal_analysis_overlay.connect([this]() { ++overlayEmits; });
    }

    void feed(const std::vector<std::string> &lines) {
        for (const auto &l : lines) proto.parseLine(l);
    }

    const AnalysisOverlayCell *cell(Coord c) const {
        const auto &m = gs.analysisOverlay().cells;
        auto it = m.find(c);
        return it == m.end() ? nullptr : &it->second;
    }
};

// One completed INFO PV block for slot `idx` at winrate `wr`, first move `first`.
// `withDepth` mirrors printPvCompletes (alpha-beta) which includes INFO DEPTH;
// pass false to mirror printRootMoves (YXNBEST) which omits it.
std::vector<std::string> pvBlock(int idx, int numPV, int depth, const std::string &first,
                                 double wr, bool withDepth = true) {
    std::vector<std::string> out;
    out.push_back("INFO PV " + std::to_string(idx));
    out.push_back("INFO NUMPV " + std::to_string(numPV));
    if (withDepth) out.push_back("INFO DEPTH " + std::to_string(depth));
    out.push_back("INFO SELDEPTH " + std::to_string(depth + 2));
    out.push_back("INFO NODES 1000");
    out.push_back("INFO EVAL " + std::to_string(static_cast<int>((wr - 0.5) * 400)));
    out.push_back("INFO WINRATE " + std::to_string(wr));
    out.push_back("INFO BESTLINE " + first + " 8,8 9,9");
    out.push_back("INFO PV DONE");
    return out;
}

} // namespace

// ── 1. INFO stream: tags + tagDepth, and stale-tag cleanup on round end ──────
TEST_CASE("PROTO-06: INFO PV DONE writes per-cell tag + tagDepth; stale tags clear per round") {
    Harness h;
    h.gs.setAnalyzing(true);

    const Coord A{3, 3}, B{3, 4}, C{4, 3}, D{5, 5};

    // Round 1 @ depth 10 — three candidates A, B, C.
    h.feed(pvBlock(0, 3, 10, "3,3", 0.60));
    h.feed(pvBlock(1, 3, 10, "4,3", 0.55));   // "4,3" -> Coord{3,4} = B
    h.feed(pvBlock(2, 3, 10, "3,4", 0.50));   // "3,4" -> Coord{4,3} = C  (last PV of round)
    REQUIRE(h.gs.tickAnalysis());

    REQUIRE(h.cell(A)); CHECK(h.cell(A)->tag == "60%"); CHECK(h.cell(A)->tagDepth == 10);
    REQUIRE(h.cell(B)); CHECK(h.cell(B)->tagDepth == 10);
    REQUIRE(h.cell(C)); CHECK(h.cell(C)->tagDepth == 10);

    // Round 2 @ depth 11 — C drops out, D appears.
    h.feed(pvBlock(0, 3, 11, "3,3", 0.62));
    h.feed(pvBlock(1, 3, 11, "4,3", 0.57));
    h.feed(pvBlock(2, 3, 11, "5,5", 0.48));   // last PV of round -> stale cleanup runs
    REQUIRE(h.gs.tickAnalysis());

    CHECK(h.cell(A)->tag == "62%");
    CHECK(h.cell(A)->tagDepth == 11);
    CHECK(h.cell(B)->tagDepth == 11);
    REQUIRE(h.cell(D)); CHECK(h.cell(D)->tagDepth == 11);
    // C was written at depth 10 and did not reappear in the depth-11 round:
    // its tag is cleared exactly once, on the round it disappeared.
    REQUIRE(h.cell(C));
    CHECK(h.cell(C)->tag.empty());
}

// ── 2. REALTIME: LOST persists, BEST is a single cell, REFRESH clears only pos ─
TEST_CASE("PROTO-06: REALTIME LOST/BEST/POS/DONE/REFRESH update the overlay correctly") {
    Harness h;
    h.gs.setAnalyzing(true);

    h.feed({"MESSAGE REALTIME LOST 5,5"});
    REQUIRE(h.gs.tickAnalysis());
    REQUIRE(h.cell(Coord{5, 5}));
    CHECK(h.cell(Coord{5, 5})->lost);

    // POS / DONE (synthetic — a stock Rapfi never sends these).
    h.feed({"MESSAGE REALTIME POS 6,6"});
    h.feed({"MESSAGE REALTIME DONE 7,7"});
    h.feed({"MESSAGE REALTIME BEST 4,4"});
    REQUIRE(h.gs.tickAnalysis());
    CHECK(h.cell(Coord{6, 6})->pos == 2);
    CHECK(h.cell(Coord{7, 7})->pos == 1);
    CHECK(h.gs.analysisOverlay().bestMove == Coord{4, 4});

    // REFRESH: clears pos only — lost + bestMove survive.
    h.feed({"MESSAGE REALTIME REFRESH"});
    REQUIRE(h.gs.tickAnalysis());
    CHECK(h.cell(Coord{6, 6})->pos == 0);
    CHECK(h.cell(Coord{7, 7})->pos == 0);
    CHECK(h.cell(Coord{5, 5})->lost);                       // persists
    CHECK(h.gs.analysisOverlay().bestMove == Coord{4, 4});  // persists
}

// ── 3. Reset: position change / explicit clear wipes the whole overlay ───────
TEST_CASE("PROTO-06: signal_board_changed and clearAnalysisOverlay clear the whole overlay") {
    Harness h;
    h.gs.setAnalyzing(true);
    h.feed(pvBlock(0, 1, 9, "3,3", 0.6));
    h.feed({"MESSAGE REALTIME LOST 5,5", "MESSAGE REALTIME BEST 4,4"});
    REQUIRE(h.gs.tickAnalysis());
    REQUIRE_FALSE(h.gs.analysisOverlay().empty());

    // Search end (EngineController calls this on the completion coordinate).
    h.gs.clearAnalysisOverlay();
    CHECK(h.gs.analysisOverlay().empty());

    // Rebuild, then a real position change.
    h.gs.setAnalyzing(true);
    h.feed(pvBlock(0, 1, 9, "3,3", 0.6));
    REQUIRE(h.gs.tickAnalysis());
    REQUIRE_FALSE(h.gs.analysisOverlay().empty());

    h.gs.setAnalyzing(false);
    REQUIRE(h.gs.makeMove(Coord{7, 7}));   // -> resetAnalysisState() -> overlay cleared
    CHECK(h.gs.analysisOverlay().empty());
}

// ── 4. Coalescing: a burst of overlay lines -> exactly one emission per tick ──
TEST_CASE("PROTO-06: a batch of overlay lines coalesces to one signal_analysis_overlay emit") {
    Harness h;
    h.gs.setAnalyzing(true);

    int before = h.overlayEmits;
    // ~30 overlay-mutating lines in one synchronous batch.
    std::vector<std::string> burst;
    for (int i = 0; i < 3; ++i) {
        auto b = pvBlock(i, 3, 12, i == 0 ? "3,3" : (i == 1 ? "4,3" : "3,4"), 0.55);
        burst.insert(burst.end(), b.begin(), b.end());
    }
    for (int i = 0; i < 5; ++i) burst.push_back("MESSAGE REALTIME LOST " + std::to_string(i) + ",0");
    burst.push_back("MESSAGE REALTIME REFRESH");
    h.feed(burst);

    // Nothing emitted synchronously — only the dirty flag was set.
    CHECK(h.overlayEmits == before);

    CHECK(h.gs.tickAnalysis());
    CHECK(h.overlayEmits == before + 1);

    // A second tick with nothing new pending must not re-emit.
    h.gs.tickAnalysis();
    CHECK(h.overlayEmits == before + 1);
}

// ── 5. YXNBEST-style block (no INFO DEPTH) still gets the round depth right ──
TEST_CASE("PROTO-06: YXNBEST transcript without INFO DEPTH still clears stale tags by round depth") {
    Harness h;
    h.gs.setAnalyzing(true);

    const Coord A{3, 3}, B{3, 4}, C{4, 3};

    // Round @ depth 10 (alpha-beta style, INFO DEPTH present) — A, B, C.
    h.feed(pvBlock(0, 3, 10, "3,3", 0.60));
    h.feed(pvBlock(1, 3, 10, "4,3", 0.55));
    h.feed(pvBlock(2, 3, 10, "3,4", 0.50));
    REQUIRE(h.gs.tickAnalysis());
    CHECK(h.cell(C)->tagDepth == 10);

    // YXNBEST round @ depth 12: printRootMoves omits INFO DEPTH from the block.
    // The round depth is still maintained on the wire — here from the per-depth
    // "MESSAGE Depth ..." summary line (parseMessage "Depth " branch).
    h.feed({"MESSAGE Depth 12-14 | Eval 60 | 3,3 8,8"});
    CHECK(h.gs.engineStatus().depth == 12);
    h.feed(pvBlock(0, 2, 12, "3,3", 0.64, /*withDepth=*/false));
    h.feed(pvBlock(1, 2, 12, "4,3", 0.58, /*withDepth=*/false));   // last PV -> stale cleanup
    REQUIRE(h.gs.tickAnalysis());

    // C (tagDepth 10) dropped out of the depth-12 round -> tag cleared using
    // currentStatus_.depth == 12, not a stale value.
    CHECK(h.cell(A)->tagDepth == 12);
    CHECK(h.cell(B)->tagDepth == 12);
    CHECK(h.cell(C)->tag.empty());
}

// ── 6. BoardViewModel: live-only gate + master/winrate toggles ──────────────
TEST_CASE("PROTO-06: BoardViewModel::searchOverlay is live-only and respects the two toggles") {
    Harness h;
    h.gs.setAnalyzing(true);
    h.feed(pvBlock(0, 2, 10, "3,3", 0.60));
    h.feed(pvBlock(1, 2, 10, "4,3", 0.55));
    h.feed({"MESSAGE REALTIME LOST 5,5"});
    REQUIRE(h.gs.tickAnalysis());

    using Kind = BoardViewModel::SearchOverlayMark::Kind;

    {
        BoardViewModel bvm(h.gs);
        bvm.update();
        size_t tags = 0, lost = 0;
        for (const auto &m : bvm.searchOverlay) {
            if (m.kind == Kind::Tag)  ++tags;
            if (m.kind == Kind::Lost) ++lost;
        }
        CHECK(tags == 2);
        CHECK(lost == 1);
    }

    // showSearchWinrate off -> tags hidden, the lost mark stays.
    {
        ViewConfig vc = h.gs.viewConfig();
        vc.showSearchWinrate = false;
        h.gs.setViewConfig(vc);
        BoardViewModel bvm(h.gs);
        bvm.update();
        size_t tags = 0, lost = 0;
        for (const auto &m : bvm.searchOverlay) {
            if (m.kind == Kind::Tag)  ++tags;
            if (m.kind == Kind::Lost) ++lost;
        }
        CHECK(tags == 0);
        CHECK(lost == 1);
    }

    // showSearchOverlay off -> nothing at all.
    {
        ViewConfig vc = h.gs.viewConfig();
        vc.showSearchOverlay = false;
        h.gs.setViewConfig(vc);
        BoardViewModel bvm(h.gs);
        bvm.update();
        CHECK(bvm.searchOverlay.empty());
    }

    // Search ends -> overlay is not copied into render state even though the
    // model still briefly holds it (live-only gate).
    {
        ViewConfig vc = h.gs.viewConfig();
        vc.showSearchOverlay = true;
        vc.showSearchWinrate = true;
        h.gs.setViewConfig(vc);
        h.gs.setAnalyzing(false);
        BoardViewModel bvm(h.gs);
        bvm.update();
        CHECK(bvm.searchOverlay.empty());
    }
}
