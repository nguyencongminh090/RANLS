// Hostile/malformed-input tests for GomocupProtocol.
//
// GomocupProtocol::parseLine() is the trust boundary between this GUI and an
// external engine subprocess: it consumes raw, attacker/bug-controlled text
// over stdout and must never throw or corrupt state, no matter how garbled
// the input is. These tests feed it empty strings, truncated tokens,
// out-of-range coordinates, and non-numeric junk and assert it survives
// without crashing, without emitting bogus signals, and without leaving the
// object unusable for subsequent well-formed input.

#include "vendor/doctest.h"

#include "engine/gomocup_protocol.h"

namespace {

/// Small harness that records what a GomocupProtocol emits while under test.
struct SignalRecorder {
    GomocupProtocol proto;
    int logCount = 0;
    int moveCount = 0;
    int analysisCount = 0;
    Coord lastMove{};
    std::vector<PVLine> lastPVs;
    EngineStatus lastStatus;

    explicit SignalRecorder(int boardSize = 15) : proto(boardSize) {
        proto.signal_log.connect([this](EngineMessageType, const std::string &) { ++logCount; });
        proto.signal_move.connect([this](Coord c) {
            ++moveCount;
            lastMove = c;
        });
        proto.signal_analysis.connect([this](const std::vector<PVLine> &pvs, const EngineStatus &status) {
            ++analysisCount;
            lastPVs = pvs;
            lastStatus = status;
        });
    }
};

} // namespace

TEST_CASE("GomocupProtocol: empty line does not crash and is logged") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine(""));
    CHECK(rec.logCount == 1);
    CHECK(rec.moveCount == 0);
}

TEST_CASE("GomocupProtocol: out-of-range coordinate is not emitted as a move") {
    SignalRecorder rec; // board size 15
    // Numerically well-formed "row,col" but far outside the 15x15 board.
    CHECK_NOTHROW(rec.proto.parseLine("999,999"));
    CHECK(rec.moveCount == 0);
}

TEST_CASE("GomocupProtocol: negative coordinate is not emitted as a move") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("-1,-1"));
    CHECK(rec.moveCount == 0);
}

TEST_CASE("GomocupProtocol: well-formed coordinate inside bounds is emitted once") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("7,7"));
    CHECK(rec.moveCount == 1);
    // parseEngineCoord treats "row,col" as Coord{col, row}.
    CHECK(rec.lastMove == Coord{7, 7});
}

TEST_CASE("GomocupProtocol: garbage after digit+comma prefix does not crash") {
    SignalRecorder rec;
    // Starts with a digit and contains a comma (classified as Coord type)
    // but the payload itself is nonsense.
    CHECK_NOTHROW(rec.proto.parseLine("1,not_a_number,,,"));
    CHECK(rec.moveCount == 0);
}

TEST_CASE("GomocupProtocol: MESSAGE with no payload is a safe no-op") {
    SignalRecorder rec;
    // Exactly "MESSAGE" (7 chars) — line.size() > 8 guard must prevent
    // substr(8) from being called on a too-short string.
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE"));
    CHECK(rec.analysisCount == 0);
}

TEST_CASE("GomocupProtocol: MESSAGE REALTIME PV with malformed tokens does not crash") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE REALTIME PV not_a_coord also_bad"));
    // No valid coords parsed -> no PV update emitted.
    CHECK(rec.analysisCount == 0);
}

TEST_CASE("GomocupProtocol: MESSAGE REALTIME VAL with non-numeric value does not crash") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE REALTIME VAL not_a_number"));
}

TEST_CASE("GomocupProtocol: MESSAGE with unterminated paren-PV block does not crash") {
    SignalRecorder rec;
    // '(' present but no closing ')' — must not throw/hang.
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE (1 unterminated"));
}

TEST_CASE("GomocupProtocol: MESSAGE paren-PV block with garbage depth/eval does not crash") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE (1) xx | garbage-depth | 7,7 8,8"));
}

TEST_CASE("GomocupProtocol: INFO line with missing/garbage numeric fields does not crash") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("INFO NUMPV not_a_number"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO DEPTH"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO EVAL"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV not_a_number"));
}

TEST_CASE("GomocupProtocol: DATABASE line with too few fields does not crash") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE DATABASE"));
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE DATABASE 1"));
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE DATABASE not_numbers here"));
}

TEST_CASE("GomocupProtocol: ERROR/UNKNOWN lines are logged as errors without crashing") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("ERROR something went wrong"));
    CHECK_NOTHROW(rec.proto.parseLine("UNKNOWN COMMAND"));
    CHECK(rec.logCount == 2);
}

TEST_CASE("GomocupProtocol: extremely long garbage line does not crash") {
    SignalRecorder rec;
    std::string junk(10000, 'x');
    CHECK_NOTHROW(rec.proto.parseLine(junk));
}

TEST_CASE("GomocupProtocol: protocol keeps working after malformed input") {
    // The parser must not be left in a broken state by hostile input --
    // a well-formed line afterwards should still behave normally.
    SignalRecorder rec;
    rec.proto.parseLine("999,999");           // hostile: out of range
    rec.proto.parseLine("MESSAGE REALTIME PV garbage");
    rec.proto.parseLine("INFO NUMPV -5");

    CHECK_NOTHROW(rec.proto.parseLine("7,7"));
    CHECK(rec.moveCount == 1);
    CHECK(rec.lastMove == Coord{7, 7});
}

// ── PROTO-01: hardening against a hostile/crashed/desynced engine ──────────
//
// GomocupProtocol treats stdout from a real engine subprocess as trusted, but
// it's a separate process that can crash, desync, or simply not be Rapfi.
// These cases feed the exact hostile lines called out in
// docs/todo/PROTO-01-parser-hardening.md and its instruction file, hitting
// the code paths that are actually reachable from parseLine (bare "INFO ..."
// lines reach parseInfo directly; "MESSAGE INFO ..." lines are, unchanged by
// this fix, intentionally ignored by parseMessage -- see gomocup_protocol.cpp
// around the "MESSAGE INFO" early-return -- so the hostile INFO/PV cases
// below use the bare "INFO ..." form, matching the existing NUMPV/PV tests
// above).

TEST_CASE("GomocupProtocol: INFO PV negative index is rejected, not adopted as a subscript") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV -1"));
    // Rejected index must not have replaced the default (0); PV DONE must
    // land on that untouched default rather than doing an OOB access on -1.
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV DONE"));
    REQUIRE(rec.analysisCount == 1);
    CHECK(rec.lastPVs.size() == 1);
}

TEST_CASE("GomocupProtocol: INFO PV huge index is rejected without unbounded allocation") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV 999999999"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV DONE"));
    // Rejected index falls back to the untouched default (0); no ~1e9-entry
    // vector was allocated.
    REQUIRE(rec.analysisCount == 1);
    CHECK(rec.lastPVs.size() == 1);
}

TEST_CASE("GomocupProtocol: INFO NUMPV huge value is clamped, not OOM-allocated") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("INFO NUMPV 2000000000"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV DONE"));
    REQUIRE(rec.analysisCount == 1);
    // Clamped to the 1..99 bound shared with `!analyze n`
    // (src/command/command_dispatcher.cpp:283), not resized to 2 billion.
    CHECK(rec.lastPVs.size() <= 99);
}

TEST_CASE("GomocupProtocol: INFO NUMPV 0 remains a no-op, matching prior behavior") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("INFO NUMPV 0"));
    CHECK(rec.analysisCount == 0);
}

TEST_CASE("GomocupProtocol: DATABASE row with out-of-range coordinate is rejected") {
    SignalRecorder rec;
    int entryCount = 0;
    rec.proto.signal_database_entry.connect([&](const DatabaseEntry &) { ++entryCount; });
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE DATABASE 99 99 -1"));
    CHECK(entryCount == 0);
}

TEST_CASE("GomocupProtocol: truncated DATABASE row (no fields at all) is rejected") {
    SignalRecorder rec;
    int entryCount = 0;
    rec.proto.signal_database_entry.connect([&](const DatabaseEntry &) { ++entryCount; });
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE DATABASE"));
    CHECK(entryCount == 0);
}

TEST_CASE("GomocupProtocol: DATABASE row missing value/depth/bound is rejected, not defaulted") {
    SignalRecorder rec;
    int entryCount = 0;
    rec.proto.signal_database_entry.connect([&](const DatabaseEntry &) { ++entryCount; });
    // row/col/label present and valid, but value/depth/bound are truncated --
    // must not silently emit an entry with default-constructed fields.
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE DATABASE 7 7 0"));
    CHECK(entryCount == 0);
}

TEST_CASE("GomocupProtocol: MESSAGE paren-PV block with numeric index 0 does not crash") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE (0) 50 | 10-8 | 7,7 8,8"));
}

TEST_CASE("GomocupProtocol: MESSAGE paren-PV block with non-numeric index does not crash") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE (abc) 50 | 10-8 | 7,7 8,8"));
}

TEST_CASE("GomocupProtocol: empty, whitespace-only, and embedded-NUL lines do not crash") {
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine(""));
    CHECK_NOTHROW(rec.proto.parseLine("   "));
    std::string withNul = std::string("INFO PV") + '\0' + "1";
    CHECK_NOTHROW(rec.proto.parseLine(withNul));
}

TEST_CASE("GomocupProtocol: well-formed DATABASE row still parses identically to before") {
    SignalRecorder rec;
    DatabaseEntry got;
    int entryCount = 0;
    rec.proto.signal_database_entry.connect([&](const DatabaseEntry &e) {
        ++entryCount;
        got = e;
    });
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE DATABASE 7 7 0 10 5 0 1 some text"));
    REQUIRE(entryCount == 1);
    CHECK(got.pos == Coord{7, 7});
    CHECK(got.value == 10);
    CHECK(got.depth == 5);
    CHECK(got.bound == 0);
    CHECK(got.hasComment == true);
    CHECK(got.boardText == "some text");
}

TEST_CASE("GomocupProtocol: well-formed INFO PV n / PV DONE sequence is unchanged by hardening") {
    // Regression guard: a legitimate, in-range MultiPV sequence must produce
    // exactly the same PVLine data after the hardening changes as before.
    SignalRecorder rec;
    CHECK_NOTHROW(rec.proto.parseLine("INFO NUMPV 2"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV 0"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO DEPTH 12"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO EVAL 55"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO BESTLINE 7,7 8,8"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV DONE"));

    REQUIRE(rec.analysisCount >= 1);
    REQUIRE(rec.lastPVs.size() >= 1);
    CHECK(rec.lastPVs[0].pvIndex == 1);
    CHECK(rec.lastPVs[0].depth == 12);
    REQUIRE(rec.lastPVs[0].moves.size() == 2);
    CHECK(rec.lastPVs[0].moves[0] == Coord{7, 7});
    CHECK(rec.lastPVs[0].moves[1] == Coord{8, 8});
}

// ── UI-06: generateMoveRequest asks the engine to actually move ───────────
//
// The "Engine plays <side>" auto-move feature needs a request that STARTS A
// SEARCH and yields a committed move, unlike generateAnalyzeRequest() (YXBOARD,
// analysis only). generateMoveRequest() emits a bare `BOARD ... DONE` block for
// a non-empty position and `BEGIN` for the empty board.

TEST_CASE("GomocupProtocol: generateMoveRequest emits a searching BOARD/DONE block") {
    GomocupProtocol proto(15);
    auto cmds = proto.generateMoveRequest({Coord{7, 7}, Coord{7, 8}});
    REQUIRE(cmds.size() == 4);
    CHECK(cmds.front() == "BOARD");
    CHECK(cmds[1].substr(cmds[1].size() - 2) == ",1"); // first stone is Black
    CHECK(cmds[2].substr(cmds[2].size() - 2) == ",2"); // second stone is White
    CHECK(cmds.back() == "DONE");
}

TEST_CASE("GomocupProtocol: generateMoveRequest on the empty board uses BEGIN") {
    GomocupProtocol proto(15);
    auto cmds = proto.generateMoveRequest({});
    REQUIRE(cmds.size() == 1);
    CHECK(cmds.front() == "BEGIN");
}

// ── PROTO-03 (reverted): generateDatabaseQuery's bare "y,x" shape is correct ──
//
// A captured transcript with 12 back-to-back yxquerydatabaseallt calls in
// exactly this format (no color field) came back with zero errors — pinning
// this shape as a regression guard against re-adding an unneeded color
// suffix. See docs/fix-log/2026-09-05-proto-03-database-query-missing-color.md's
// correction note and PROTO-04 for the actual bug (a send/receive race).

TEST_CASE("GomocupProtocol: generateDatabaseQuery emits a plain y,x position block") {
    GomocupProtocol proto(15);
    auto cmds = proto.generateDatabaseQuery({Coord{7, 7}, Coord{7, 8}});
    REQUIRE(cmds.size() == 4);
    CHECK(cmds.front() == "yxquerydatabaseallt");
    CHECK(cmds[1] == "7,7");
    CHECK(cmds[2] == "8,7");
    CHECK(cmds.back() == "DONE");
}

TEST_CASE("GomocupProtocol: generateDatabaseQuery on the empty board is just the bare block") {
    GomocupProtocol proto(15);
    auto cmds = proto.generateDatabaseQuery({});
    REQUIRE(cmds.size() == 2);
    CHECK(cmds.front() == "yxquerydatabaseallt");
    CHECK(cmds.back() == "DONE");
}

// ── PROTO-02: A1-notation coordinates must use the real board size ─────────
//
// parseEngineCoord() used to hardcode `15 - rowNumber` when decoding A1-style
// tokens (Yixin's flipY_X mode reports A1 as the bottom-left cell), which
// silently corrupted move data on any board that wasn't 15x15. These cases
// pin the correct flip at two non-15 sizes, through the same
// "MESSAGE REALTIME BEST <token>" path a real engine uses.

TEST_CASE("GomocupProtocol: A1-notation coordinate flips against a 5x5 board, not a hardcoded 15") {
    SignalRecorder rec(5);
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE REALTIME BEST A1"));
    // A1 is bottom-left: col 'A' -> x=0, row 1 -> y = boardSize(5) - 1 = 4.
    REQUIRE(rec.analysisCount == 1);
    CHECK(rec.lastStatus.bestMove == Coord{0, 4});
}

TEST_CASE("GomocupProtocol: A1-notation coordinate flips against a 22x22 board, not a hardcoded 15") {
    SignalRecorder rec(22);
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE REALTIME BEST A1"));
    // col 'A' -> x=0, row 1 -> y = boardSize(22) - 1 = 21.
    REQUIRE(rec.analysisCount == 1);
    CHECK(rec.lastStatus.bestMove == Coord{0, 21});
}

TEST_CASE("GomocupProtocol: A1-notation top-right corner resolves correctly at board size 22") {
    SignalRecorder rec(22);
    // Column 'V' is the 22nd letter (A..V), row 22 is the top row.
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE REALTIME BEST V22"));
    // x = 'V'-'A' = 21, y = boardSize(22) - 22 = 0.
    REQUIRE(rec.analysisCount == 1);
    CHECK(rec.lastStatus.bestMove == Coord{21, 0});
}

TEST_CASE("GomocupProtocol: A1-notation coordinate still flips against 15 for a default-sized board") {
    // Regression guard: the old hardcoded-15 behavior must be preserved when
    // the board actually is 15x15.
    SignalRecorder rec(15);
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE REALTIME BEST A1"));
    REQUIRE(rec.analysisCount == 1);
    CHECK(rec.lastStatus.bestMove == Coord{0, 14});
}

TEST_CASE("GomocupProtocol: A1-notation tokens inside a PV line use the real board size") {
    // Exercises parseMoveTokens() -> parseEngineCoord() at a non-15 size, the
    // same call path INFO BESTLINE and Bestline-message PV lists use.
    SignalRecorder rec(5);
    CHECK_NOTHROW(rec.proto.parseLine("INFO NUMPV 1"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV 0"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO BESTLINE A1 E5"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV DONE"));

    REQUIRE(rec.analysisCount >= 1);
    REQUIRE(rec.lastPVs.size() >= 1);
    REQUIRE(rec.lastPVs[0].moves.size() == 2);
    CHECK(rec.lastPVs[0].moves[0] == Coord{0, 4});  // A1 on a 5x5 board.
    CHECK(rec.lastPVs[0].moves[1] == Coord{4, 0});  // E5 on a 5x5 board.
}

// --- STATE-03: currentPVs_ never shrinks / materialises empty PV slots ---
//
// Rapfi's multiPV loop (search/ab/search.cpp: `for (sd.pvIdx = 0; sd.pvIdx <
// sd.multiPv; ++sd.pvIdx)`) always reports pvIdx 0..multiPv-1 in strictly
// increasing order within a round, on the single main search thread — real
// engine output never skips an index or delivers them out of order. So the
// defect this covers isn't mid-round gaps (they can't occur from a
// spec-compliant engine, and PVView/BoardViewModel's `!moves.empty()`
// filters would hide one harmlessly even if it did); it's specifically:
//
//  starting a new round with fewer PV lines than the previous one (i.e.
//  multiPV lowered mid-search) must drop the previous round's stale
//  high-index entries, not leave them visible forever — those entries have
//  real (non-empty) moves from the earlier, larger round, so the UI-side
//  empty-move filters do NOT catch them.
//
// Both cases below are exercised via the MESSAGE-stream paren-PV format
// ("(n) eval | depth-selDepth | moves"), which shares a single commitPV()
// entry point with the Bestline / UCILIKE "multipv" formats and has no
// explicit NUMPV/count signal of its own (unlike the INFO/Detail stream).

TEST_CASE("GomocupProtocol: sequential in-order PV arrival across a round leaves no empty filler") {
    SignalRecorder rec;

    // Indices arrive strictly in order 0, 1, 2, matching real engine
    // behavior (see comment above) — currentPVs_ should grow cleanly with
    // no gap ever materialising.
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE (1) 55 | 9-7 | 7,8 8,7"));
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE (2) 52 | 9-7 | 7,9 8,9"));
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE (3) 50 | 10-8 | 7,7 8,8"));

    REQUIRE(rec.lastPVs.size() == 3);
    for (const auto &pv : rec.lastPVs) {
        CHECK_FALSE(pv.moves.empty());
    }
}

TEST_CASE("GomocupProtocol: MESSAGE-stream commitPV drops stale high-index PVs when a new round reports fewer lines") {
    SignalRecorder rec;

    // First round: 3 PV lines.
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE (1) 55 | 9-7 | 7,8 8,7"));
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE (2) 52 | 9-7 | 7,9 8,9"));
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE (3) 50 | 10-8 | 7,7 8,8"));
    REQUIRE(rec.lastPVs.size() == 3);

    // Next round starts (multiPV lowered to 1): only index 0 is reported.
    // The stale index-1/2 entries from the previous round must not survive.
    CHECK_NOTHROW(rec.proto.parseLine("MESSAGE (1) 60 | 11-9 | 7,6 8,6"));
    REQUIRE(rec.lastPVs.size() == 1);
    CHECK_FALSE(rec.lastPVs[0].moves.empty());
    // parseEngineCoord treats "row,col" as Coord{col, row}: "7,6" -> Coord{6, 7}.
    CHECK(rec.lastPVs[0].moves[0] == Coord{6, 7});
}

TEST_CASE("GomocupProtocol: INFO NUMPV lowered mid-search drops stale high-index PVs") {
    SignalRecorder rec;

    // First round: NUMPV 3, all three indices reported via the INFO/Detail
    // stream (PV n / PV DONE).
    CHECK_NOTHROW(rec.proto.parseLine("INFO NUMPV 3"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV 0"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO BESTLINE 7,7 8,8"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV DONE"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV 1"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO BESTLINE 7,8 8,7"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV DONE"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV 2"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO BESTLINE 7,9 8,9"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV DONE"));
    REQUIRE(rec.lastPVs.size() == 3);

    // Next iteration: engine drops to NUMPV 1 (multiPV lowered).
    CHECK_NOTHROW(rec.proto.parseLine("INFO NUMPV 1"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV 0"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO BESTLINE 7,6 8,6"));
    CHECK_NOTHROW(rec.proto.parseLine("INFO PV DONE"));

    REQUIRE(rec.lastPVs.size() == 1);
    CHECK_FALSE(rec.lastPVs[0].moves.empty());
    // parseEngineCoord treats "row,col" as Coord{col, row}: "7,6" -> Coord{6, 7}.
    CHECK(rec.lastPVs[0].moves[0] == Coord{6, 7});
}
