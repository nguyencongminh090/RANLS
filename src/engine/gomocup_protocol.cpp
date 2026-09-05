#include "gomocup_protocol.h"
#include <cmath>
#include <cstring>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <iomanip>

// ── Helpers ──────────────────────────────────────────────────────────────────

static double cpToWinrate(double cp) {
    return 1.0 / (1.0 + std::exp(-cp / 200.0));
}

static int64_t parseNodeCount(const std::string &s) {
    if (s.empty()) return 0;
    char   suffix = s.back();
    double val    = 0;
    try {
        if (suffix == 'K' || suffix == 'k') {
            val = std::stod(s.substr(0, s.size() - 1)) * 1000;
        } else if (suffix == 'M' || suffix == 'm') {
            val = std::stod(s.substr(0, s.size() - 1)) * 1'000'000;
        } else if (suffix == 'B' || suffix == 'b' || suffix == 'G' || suffix == 'g') {
            val = std::stod(s.substr(0, s.size() - 1)) * 1'000'000'000;
        } else {
            val = std::stod(s);
        }
    } catch (...) { return 0; }
    return static_cast<int64_t>(val);
}

/// Parse a single engine-reported coordinate token. `boardSize` is required
/// for the A1-notation branch: Yixin's flipY_X mode reports A1 as the
/// bottom-left cell, so the row label must be flipped against the *actual*
/// board size, not an assumed 15 (see PROTO-02).
static Coord parseEngineCoord(const std::string &s, int boardSize) {
    if (s.empty()) return Coord{-1, -1};

    // Try y,x format (Yixin standard)
    int row = -1, col = -1;
    if (std::sscanf(s.c_str(), "%d,%d", &row, &col) == 2) {
        return Coord{col, row};
    }

    // Try A1 format
    if (std::isalpha(s[0])) {
        int col = std::toupper(s[0]) - 'A';
        try {
            int rowNumber = std::stoi(s.substr(1));
            // In Yixin flipY_X mode, A1 is bottom-left.
            return Coord{col, boardSize - rowNumber};
        } catch (...) {
            return Coord{-1, -1};
        }
    }

    return Coord{-1, -1};
}

static std::string coordToEngine(Coord c) {
    // Yixin standard: y,x
    return std::to_string(c.y) + "," + std::to_string(c.x);
}

static bool parseBoolToken(const std::string &s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lower == "1" || lower == "true" || lower == "yes" || lower == "on";
}

static int parseIntToken(const std::string &s, int fallback = 0) {
    try {
        size_t idx = 0;
        int value = std::stoi(s, &idx);
        return idx == s.size() ? value : fallback;
    } catch (...) {
        return fallback;
    }
}

static std::string trimCopy(const std::string &s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

static std::vector<std::string> splitPipe(const std::string &s) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= s.size()) {
        size_t pos = s.find('|', start);
        if (pos == std::string::npos) {
            parts.push_back(trimCopy(s.substr(start)));
            break;
        }
        parts.push_back(trimCopy(s.substr(start, pos - start)));
        start = pos + 1;
    }
    return parts;
}

static std::string signedIntText(int value) {
    std::ostringstream out;
    out << std::showpos << value;
    return out.str();
}

static std::string formatWinrateText(double winrate) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << (winrate * 100.0) << "%";
    return out.str();
}

static std::string evalDisplayText(const EngineStatus &status) {
    if (!status.evalText.empty()) return status.evalText;
    if (status.mateStep > 0) return "+M" + std::to_string(status.mateStep);
    if (status.mateStep < 0) return "-M" + std::to_string(-status.mateStep);
    return formatWinrateText(status.winrate);
}

static bool parseEvalToken(const std::string &eval,
                           double            &winrate,
                           int               &mateStep,
                           std::string       &evalText) {
    if (eval.empty()) return false;

    evalText = eval;
    mateStep = 0;
    if (eval.size() >= 2 && eval.substr(0, 2) == "+M") {
        try {
            mateStep = std::stoi(eval.substr(2));
            winrate = 1.0;
            return true;
        } catch (...) {
            return false;
        }
    }
    if (eval.size() >= 2 && eval.substr(0, 2) == "-M") {
        try {
            mateStep = -std::stoi(eval.substr(2));
            winrate = 0.0;
            return true;
        } catch (...) {
            return false;
        }
    }

    try {
        double cp = std::stod(eval);
        winrate = cpToWinrate(cp);
        return true;
    } catch (...) {
        return false;
    }
}

static void parseDepthPair(const std::string &text, int &depth, int &selDepth) {
    auto dash = text.find('-');
    try {
        if (dash != std::string::npos) {
            depth = std::stoi(text.substr(0, dash));
            selDepth = std::stoi(text.substr(dash + 1));
        } else {
            depth = std::stoi(text);
        }
    } catch (...) {}
}

static int64_t parseTimeTextMs(const std::string &text, int64_t fallback = 0) {
    try {
        if (text.size() > 2 && text.substr(text.size() - 2) == "ms") {
            return static_cast<int64_t>(std::stod(text.substr(0, text.size() - 2)));
        }
        if (!text.empty() && text.back() == 's') {
            return static_cast<int64_t>(std::stod(text.substr(0, text.size() - 1)) * 1000.0);
        }
        return static_cast<int64_t>(std::stod(text));
    } catch (...) {
        return fallback;
    }
}

static std::vector<Coord> parseMoveTokens(const std::string &movesText, int boardSize) {
    std::vector<Coord> moves;
    std::istringstream ss(movesText);
    std::string token;
    while (ss >> token) {
        Coord c = parseEngineCoord(token, boardSize);
        if (c.isValid(boardSize)) {
            moves.push_back(c);
        }
    }
    return moves;
}

// ── Hardening: bounded index/count parsing ──────────────────────────────────
// GomocupProtocol::parseLine is a trust boundary: the engine is a separate
// process that can crash, desync, or simply not be Rapfi. Every index/count
// derived from its stdout that ends up as a vector subscript or resize()
// argument must be bounds-checked before use. `kMaxPVCount` reuses the same
// 1..99 MultiPV bound `!analyze n` already enforces
// (src/command/command_dispatcher.cpp:283) so the two entry points can't
// disagree about how many simultaneous PV lines are legal.
static constexpr int kMaxPVCount = 99;

/// Parse `s` as a plain signed integer, requiring the *entire* token to be
/// consumed. Returns false (leaving `out` untouched) on empty/non-numeric/
/// partially-numeric text or on over/underflow. Never throws -- `std::stol`'s
/// exceptions are caught here so callers don't need their own try/catch.
static bool parseStrictInt(const std::string &s, long &out) {
    if (s.empty()) return false;
    try {
        size_t idx = 0;
        long value = std::stol(s, &idx);
        if (idx != s.size()) return false;
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

/// Parse a bounded engine-derived index/count (PV slot, NUMPV, ...) from
/// text. Returns false if the token isn't a clean integer or falls outside
/// [minVal, maxVal]; `out` is left untouched on failure so callers can keep
/// prior state instead of adopting a hostile value.
static bool parseBoundedInt(const std::string &s, int minVal, int maxVal, int &out) {
    long value = 0;
    if (!parseStrictInt(s, value)) return false;
    if (value < minVal || value > maxVal) return false;
    out = static_cast<int>(value);
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════

GomocupProtocol::GomocupProtocol(int boardSize) : boardSize_(boardSize) {}

std::vector<std::string> GomocupProtocol::generateStart(int boardSize) {
    boardSize_ = boardSize;
    return {"START " + std::to_string(boardSize)};
}

std::vector<std::string> GomocupProtocol::generateRule(GameRule rule) {
    return {"INFO rule " + std::to_string(static_cast<int>(rule))};
}

std::vector<std::string> GomocupProtocol::generateConfig(const EngineConfig& cfg) {
    std::vector<std::string> cmds;
    cmds.push_back("INFO timeout_turn " + std::to_string(cfg.timeoutTurn));
    cmds.push_back("INFO timeout_match " + std::to_string(cfg.timeoutMatch));
    cmds.push_back("INFO time_increment " + std::to_string(cfg.increment));
    cmds.push_back("INFO max_depth " + std::to_string(cfg.maxDepth));
    cmds.push_back("INFO max_node " + std::to_string(cfg.maxNodes));
    cmds.push_back("INFO thread_num " + std::to_string(cfg.threads));
    cmds.push_back("INFO hash_size " + std::to_string(static_cast<int64_t>(cfg.hashSizeMB) * 1024));
    cmds.push_back("INFO SHOW_DETAIL 0");

    for (const auto& [key, val] : cfg.customParams) {
        cmds.push_back("INFO " + key + " " + val);
    }
    
    return cmds;
}

void GomocupProtocol::clearAnalysisState() {
    // UI-04: drop everything accumulated for the previous think so a stale,
    // late-arriving engine message for an old position cannot repopulate
    // currentPVs_ and leave phantom PV rows on the new position. Called both
    // at the start of a fresh analysis (below) and on every position change
    // (EngineController wires this to GameState::signal_board_changed).
    currentPVs_.clear();
    currentStatus_   = {};
    currentPVIndex_  = 0;
    currentNumPV_    = 0;
    currentBestLine_.clear();
    resetCurrentPVState();
}

std::vector<std::string> GomocupProtocol::generateAnalyzeRequest(const std::vector<Coord>& path, int multiPV) {
    std::vector<std::string> cmds;

    clearAnalysisState();

    cmds.push_back("YXBOARD");
    for (size_t i = 0; i < path.size(); ++i) {
        int color = (i % 2 == 0) ? 1 : 2;
        cmds.push_back(coordToEngine(path[i]) + "," + std::to_string(color));
    }
    cmds.push_back("DONE");
    cmds.push_back("YXNBEST " + std::to_string(multiPV));
    return cmds;
}

std::vector<std::string> GomocupProtocol::generateMoveRequest(const std::vector<Coord>& path) {
    std::vector<std::string> cmds;

    clearAnalysisState();

    // Empty board: BEGIN is the dedicated "engine plays first as Black" command.
    if (path.empty())
        return {"BEGIN"};

    // Unlike YXBOARD (analysis only), a bare BOARD ... DONE block replaces the
    // position AND starts a search for the side-to-move's reply, which the
    // engine returns as a single coordinate line (parsed into signal_move).
    // UI-06 relies on that reply to auto-play the engine's move.
    cmds.push_back("BOARD");
    for (size_t i = 0; i < path.size(); ++i) {
        int color = (i % 2 == 0) ? 1 : 2;
        cmds.push_back(coordToEngine(path[i]) + "," + std::to_string(color));
    }
    cmds.push_back("DONE");
    return cmds;
}

std::string GomocupProtocol::generateStop() {
    return "STOP";
}

std::string GomocupProtocol::generateQuit() {
    return "END";
}

std::vector<std::string> GomocupProtocol::generateDatabaseQuery(const std::vector<Coord>& path) {
    // PROTO-03 (reverted, see docs/fix-log/2026-09-05-proto-03-database-query-missing-color.md's
    // correction note): this bare "y,x" shape, no color field, is correct as
    // written — a captured transcript with 12 back-to-back yxquerydatabaseallt
    // calls in exactly this format, zero errors, confirmed it. Do not add a
    // color suffix here; the actual bug was PROTO-04 (a send/receive race in
    // EngineController), not this format.
    std::vector<std::string> cmds;
    cmds.push_back("yxquerydatabaseallt");
    for (const auto& c : path) {
        cmds.push_back(std::to_string(c.y) + "," + std::to_string(c.x));
    }
    cmds.push_back("DONE");
    return cmds;
}

void GomocupProtocol::parseLine(const std::string& line) {
    if (line.empty()) {
        signal_log.emit(EngineMessageType::Output, line);
        return;
    }

    EngineMessageType type = EngineMessageType::Output;

    if (line.size() >= 3 && std::isdigit(static_cast<unsigned char>(line[0])) && line.find(',') != std::string::npos) {
        type = EngineMessageType::Coord;
    } else if (line.rfind("MESSAGE", 0) == 0) {
        type = EngineMessageType::Message;
    } else if (line.rfind("ERROR", 0) == 0 || line.rfind("UNKNOWN", 0) == 0) {
        type = EngineMessageType::Error;
    }

    signal_log.emit(type, line);

    if (line == "OK") {
        return;
    }

    if (type == EngineMessageType::Coord) {
        Coord move = parseEngineCoord(line, boardSize_);
        if (move.isValid(boardSize_)) {
            signal_move.emit(move);
            return;
        }
    }

    if (type == EngineMessageType::Message) {
        if (line.size() > 8) {
            parseMessage(line.substr(8));
        }
        return;
    }

    if (line.rfind("INFO ", 0) == 0 || line.rfind("INFO", 0) == 0) {
        if (line.size() > 5) {
            parseInfo(line.substr(5));
        }
        return;
    }
}

void GomocupProtocol::resetCurrentPVState() {
    currentPvDepth_ = currentStatus_.depth;
    currentPvSelDepth_ = currentStatus_.selDepth;
    currentPvNodes_ = 0;
    currentPvWinrate_ = currentStatus_.winrate;
    currentPvMateStep_ = currentStatus_.mateStep;
    currentPvEvalText_ = currentStatus_.evalText;
    currentBestLine_.clear();
}

void GomocupProtocol::parseMessage(const std::string &msg) {
    if (msg.rfind("REALTIME ", 0) == 0) {
        std::string sub = msg.substr(9);
        if (sub.rfind("BEST ", 0) == 0) {
            Coord best = parseEngineCoord(sub.substr(5), boardSize_);
            if (best.isValid(boardSize_)) {
                currentStatus_.bestMove = best;
                signal_analysis.emit(currentPVs_, currentStatus_);
            }
        } else if (sub.rfind("PV ", 0) == 0) {
            parseRealtimePV(sub.substr(3));
        } else if (sub.rfind("VAL ", 0) == 0) {
            try {
                double cp = std::stod(sub.substr(4));
                currentStatus_.winrate = cpToWinrate(cp);
                currentStatus_.mateStep = 0;
                currentStatus_.evalText.clear();
            } catch (...) {}
        }
        return;
    }

    if (msg.rfind("DATABASE", 0) == 0) {
        parseDatabase(msg.substr(8));
        return;
    }

    if (msg.rfind("INFO", 0) == 0 ||
        msg.rfind("SWAP", 0) == 0 || msg.rfind("SOOSORV", 0) == 0) {
        return;
    }

    auto commitPV = [this](int idx, PVLine pv) {
        if (idx < 0 || idx >= kMaxPVCount) {
            signal_log.emit(EngineMessageType::Error,
                             "GomocupProtocol: rejected out-of-range PV index " + std::to_string(idx));
            return;
        }
        // STATE-03: a fresh primary-line (index 0) report marks the start of
        // a new multi-PV round in the MESSAGE-stream formats (Bestline,
        // NORMAL "(n)|...", UCILIKE "multipv ..."), none of which carry an
        // explicit NUMPV/count signal the way the INFO/Detail stream does.
        // Without this, a round that reports fewer PVs than the previous one
        // (e.g. multiPV lowered mid-search) leaves the old round's surplus
        // higher-index entries in currentPVs_ forever, since resize() below
        // only ever grows. Truncate to just the new index 0 here so stale
        // entries drop immediately; the round's remaining indices regrow the
        // vector back out as they arrive right after. This relies on the
        // engine reporting pvIdx 0..multiPv-1 in strictly increasing order
        // per round (true for Rapfi: search/ab/search.cpp's multiPV loop is
        // single-threaded and never skips/reorders indices), so an index-0
        // report is unambiguously "a new round started."
        if (idx == 0 && currentPVs_.size() > 1) {
            currentPVs_.resize(1);
            currentNumPV_ = 1;
        }
        if (idx >= static_cast<int>(currentPVs_.size())) {
            currentPVs_.resize(idx + 1);
        }

        pv.pvIndex = idx + 1;
        currentPVs_[idx] = std::move(pv);
        if (!currentPVs_[idx].moves.empty() && idx == 0) {
            currentStatus_.bestMove = currentPVs_[idx].moves.front();
        }
        signal_analysis.emit(currentPVs_, currentStatus_);
    };

    if (msg.rfind("Bestline ", 0) == 0) {
        auto moves = parseMoveTokens(msg.substr(9), boardSize_);
        if (!moves.empty()) {
            PVLine pv = currentPVs_.empty() ? PVLine{} : currentPVs_[0];
            pv.depth = currentStatus_.depth;
            pv.selDepth = currentStatus_.selDepth;
            pv.nodes = currentStatus_.nodes;
            pv.score = currentStatus_.winrate;
            pv.mateStep = currentStatus_.mateStep;
            pv.evalText = currentStatus_.evalText;
            pv.moves = std::move(moves);
            commitPV(0, std::move(pv));
        }
        return;
    }

    if (!msg.empty() && msg[0] == '(') {
        auto close = msg.find(')');
        if (close != std::string::npos) {
            int pvIndex = parseIntToken(msg.substr(1, close - 1), 1) - 1;
            auto parts = splitPipe(msg);
            if (parts.size() >= 3) {
                if (pvIndex < 0 || pvIndex >= kMaxPVCount) {
                    signal_log.emit(EngineMessageType::Error,
                                     "GomocupProtocol: rejected out-of-range PV index in '("
                                     + msg.substr(1, close - 1) + ")'");
                    return;
                }
                std::istringstream head(parts[0].substr(close + 1));
                std::string eval;
                head >> eval;

                PVLine pv;
                pv.depth = currentStatus_.depth;
                pv.selDepth = currentStatus_.selDepth;
                parseDepthPair(parts[1], pv.depth, pv.selDepth);
                pv.nodes = currentStatus_.nodes;
                pv.score = currentStatus_.winrate;
                pv.mateStep = currentStatus_.mateStep;
                pv.evalText = currentStatus_.evalText;
                if (parseEvalToken(eval, pv.score, pv.mateStep, pv.evalText)) {
                    if (pvIndex == 0) {
                        currentStatus_.winrate = pv.score;
                        currentStatus_.mateStep = pv.mateStep;
                        currentStatus_.evalText = pv.evalText;
                    }
                }
                pv.moves = parseMoveTokens(parts[2], boardSize_);

                currentPVIndex_ = pvIndex;
                if (currentNumPV_ <= pvIndex) currentNumPV_ = pvIndex + 1;
                currentStatus_.depth = std::max(currentStatus_.depth, pv.depth);
                currentStatus_.selDepth = std::max(currentStatus_.selDepth, pv.selDepth);
                commitPV(pvIndex, std::move(pv));
                return;
            }
        }
    }

    if (msg.rfind("Depth ", 0) == 0) {
        auto parts = splitPipe(msg);
        PVLine pv = currentPVs_.empty() ? PVLine{} : currentPVs_[0];
        pv.depth = currentStatus_.depth;
        pv.selDepth = currentStatus_.selDepth;
        pv.nodes = currentStatus_.nodes;
        pv.score = currentStatus_.winrate;
        pv.mateStep = currentStatus_.mateStep;
        pv.evalText = currentStatus_.evalText;

        for (const auto &part : parts) {
            if (part.rfind("Depth ", 0) == 0) {
                parseDepthPair(trimCopy(part.substr(6)), pv.depth, pv.selDepth);
                currentStatus_.depth = pv.depth;
                currentStatus_.selDepth = pv.selDepth;
            } else if (part.rfind("Eval ", 0) == 0) {
                parseEvalToken(trimCopy(part.substr(5)), pv.score, pv.mateStep, pv.evalText);
                currentStatus_.winrate = pv.score;
                currentStatus_.mateStep = pv.mateStep;
                currentStatus_.evalText = pv.evalText;
            } else if (part.rfind("Time ", 0) == 0) {
                currentStatus_.timeMs = parseTimeTextMs(trimCopy(part.substr(5)), currentStatus_.timeMs);
            } else {
                auto moves = parseMoveTokens(part, boardSize_);
                if (!moves.empty()) pv.moves = std::move(moves);
            }
        }

        commitPV(0, std::move(pv));
        return;
    }

    if (msg.rfind("Speed ", 0) == 0) {
        auto parts = splitPipe(msg);
        bool updated = false;
        for (const auto &part : parts) {
            if (part.rfind("Depth ", 0) == 0) {
                parseDepthPair(trimCopy(part.substr(6)), currentStatus_.depth, currentStatus_.selDepth);
                updated = true;
            } else if (part.rfind("Eval ", 0) == 0) {
                parseEvalToken(trimCopy(part.substr(5)), currentStatus_.winrate,
                               currentStatus_.mateStep, currentStatus_.evalText);
                updated = true;
            } else if (part.rfind("Node ", 0) == 0 || part.rfind("Visit ", 0) == 0) {
                auto space = part.find(' ');
                currentStatus_.nodes = parseNodeCount(trimCopy(part.substr(space + 1)));
                updated = true;
            } else if (part.rfind("Time ", 0) == 0) {
                currentStatus_.timeMs = parseTimeTextMs(trimCopy(part.substr(5)), currentStatus_.timeMs);
                updated = true;
            }
        }
        if (updated) signal_analysis.emit(currentPVs_, currentStatus_);
        return;
    }

    std::istringstream ss(msg);
    std::string token;
    bool updatedAny = false;
    std::vector<std::string> pvTokens;
    bool inPV = false;
    int parsedDepth = currentStatus_.depth;
    int parsedSelDepth = currentStatus_.selDepth;
    int parsedPVIndex = 0;
    int64_t parsedNodes = currentStatus_.nodes;
    double parsedWinrate = currentStatus_.winrate;
    int parsedMateStep = currentStatus_.mateStep;
    std::string parsedEvalText = currentStatus_.evalText;

    while (ss >> token) {
        if (inPV) {
            pvTokens.push_back(token);
            continue;
        }

        if (token == "depth" || token == "d") {
            std::string val;
            if (ss >> val) {
                parseDepthPair(val, parsedDepth, parsedSelDepth);
                currentStatus_.depth = parsedDepth;
                currentStatus_.selDepth = parsedSelDepth;
                updatedAny = true;
            }
        } else if (token == "multipv") {
            std::string val;
            if (ss >> val) {
                int mpv = 0;
                if (parseBoundedInt(val, 1, kMaxPVCount, mpv)) {
                    parsedPVIndex = mpv - 1;
                    currentPVIndex_ = parsedPVIndex;
                    if (currentNumPV_ < mpv) currentNumPV_ = mpv;
                }
                updatedAny = true;
            }
        } else if (token == "ev" || token == "eval") {
            std::string eval;
            if (ss >> eval) {
                if (parseEvalToken(eval, parsedWinrate, parsedMateStep, parsedEvalText)) {
                    if (parsedPVIndex == 0) {
                        currentStatus_.winrate = parsedWinrate;
                        currentStatus_.mateStep = parsedMateStep;
                        currentStatus_.evalText = parsedEvalText;
                    }
                }
                updatedAny = true;
            }
        } else if (token == "n/ms") {
            double nms = 0;
            if (ss >> nms) {
                currentStatus_.nps = static_cast<int64_t>(nms * 1000.0);
                updatedAny = true;
            }
        } else if (token == "n") {
            std::string val;
            if (ss >> val) {
                parsedNodes = parseNodeCount(val);
                currentStatus_.nodes = parsedNodes;
                updatedAny = true;
            }
        } else if (token == "nps") {
            int64_t nps = 0;
            if (ss >> nps) {
                currentStatus_.nps = nps;
                updatedAny = true;
            }
        } else if (token == "tm" || token == "time") {
            int64_t tm = 0;
            if (ss >> tm) {
                currentStatus_.timeMs = tm;
                updatedAny = true;
            }
        } else if (token == "pv") {
            inPV = true;
        }
    }

    if (!pvTokens.empty()) {
        PVLine pv;
        pv.depth = parsedDepth;
        pv.selDepth = parsedSelDepth;
        pv.nodes = parsedNodes;
        pv.score = parsedWinrate;
        pv.mateStep = parsedMateStep;
        pv.evalText = parsedEvalText;

        for (const auto &mt : pvTokens) {
            Coord c = parseEngineCoord(mt, boardSize_);
            if (c.isValid(boardSize_)) pv.moves.push_back(c);
        }

        if (!pv.moves.empty()) {
            int idx = parsedPVIndex;
            if (idx == 0) {
                currentStatus_.bestMove = pv.moves.front();
            }
            commitPV(idx, std::move(pv));
            updatedAny = false;
        }
    }

    if (updatedAny) {
        signal_analysis.emit(currentPVs_, currentStatus_);
    }
}

void GomocupProtocol::parseRealtimePV(const std::string &data) {
    PVLine pv;
    pv.depth = currentStatus_.depth;
    pv.selDepth = currentStatus_.selDepth;
    pv.nodes = currentStatus_.nodes;
    pv.score = currentStatus_.winrate;
    pv.mateStep = currentStatus_.mateStep;
    pv.evalText = currentStatus_.evalText;

    std::istringstream ss(data);
    std::string token;
    while (ss >> token) {
        Coord c = parseEngineCoord(token, boardSize_);
        if (c.isValid(boardSize_)) {
            pv.moves.push_back(c);
        }
    }

    if (!pv.moves.empty()) {
        pv.pvIndex = 1;
        if (currentPVs_.empty()) {
            currentPVs_.push_back(pv);
        } else {
            currentPVs_[0] = pv;
        }
        signal_analysis.emit(currentPVs_, currentStatus_);
    }
}

void GomocupProtocol::parseInfo(const std::string &info) {
    std::istringstream ss(info);
    std::string key;
    if (!(ss >> key)) return;

    if (key == "NUMPV") {
        int n = 0;
        if (ss >> n && n > 0) {
            int clamped = std::min(n, kMaxPVCount);
            if (clamped != n) {
                signal_log.emit(EngineMessageType::Error,
                                 "GomocupProtocol: NUMPV " + std::to_string(n) +
                                 " exceeds max " + std::to_string(kMaxPVCount) + "; clamped");
            }
            currentNumPV_ = clamped;
            currentPVs_.resize(clamped);
        }
    } else if (key == "DEPTH") {
        if (ss >> currentPvDepth_) {
            currentStatus_.depth = std::max(currentStatus_.depth, currentPvDepth_);
        }
    } else if (key == "SELDEPTH") {
        if (ss >> currentPvSelDepth_) {
            currentStatus_.selDepth = std::max(currentStatus_.selDepth, currentPvSelDepth_);
        }
    } else if (key == "BESTLINE") {
        std::string rest;
        std::getline(ss, rest);
        currentBestLine_ = parseMoveTokens(rest, boardSize_);
    } else if (key == "EVAL") {
        std::string eval;
        ss >> eval;
        if (parseEvalToken(eval, currentPvWinrate_, currentPvMateStep_, currentPvEvalText_)
            && currentPVIndex_ == 0) {
            currentStatus_.winrate = currentPvWinrate_;
            currentStatus_.mateStep = currentPvMateStep_;
            currentStatus_.evalText = currentPvEvalText_;
        }
    } else if (key == "WINRATE") {
        double winrate = 0.5;
        if (ss >> winrate) {
            currentPvWinrate_ = winrate;
            if (currentPVIndex_ == 0) {
                currentStatus_.winrate = winrate;
            }
        }
    } else if (key == "NPS") {
        ss >> currentStatus_.nps;
    } else if (key == "SPEED") {
        ss >> currentStatus_.nps;
    } else if (key == "NODES") {
        ss >> currentPvNodes_;
    } else if (key == "TOTALNODES") {
        ss >> currentStatus_.nodes;
    } else if (key == "TIME") {
        ss >> currentStatus_.timeMs;
    } else if (key == "TOTALTIME") {
        ss >> currentStatus_.timeMs;
    } else if (key == "PV") {
        std::string sub;
        if (ss >> sub) {
            if (sub == "DONE") {
                onPVDone();
            } else {
                int idx = 0;
                if (parseBoundedInt(sub, 0, kMaxPVCount - 1, idx)) {
                    currentPVIndex_ = idx;
                    resetCurrentPVState();
                } else {
                    signal_log.emit(EngineMessageType::Error,
                                     "GomocupProtocol: rejected out-of-range/non-numeric PV index '"
                                     + sub + "'");
                }
            }
        }
    }
}

void GomocupProtocol::onPVDone() {
    int idx = currentPVIndex_;

    if (idx < 0 || idx >= kMaxPVCount) {
        signal_log.emit(EngineMessageType::Error,
                         "GomocupProtocol: PV DONE with out-of-range index " + std::to_string(idx));
        return;
    }

    if (idx >= static_cast<int>(currentPVs_.size())) {
        currentPVs_.resize(idx + 1);
    }

    PVLine &pv = currentPVs_[idx];
    pv.pvIndex  = idx + 1;
    pv.depth    = currentPvDepth_;
    pv.selDepth = currentPvSelDepth_;
    pv.nodes    = currentPvNodes_;
    pv.score    = currentPvWinrate_;
    pv.mateStep = currentPvMateStep_;
    pv.evalText = currentPvEvalText_;

    if (!currentBestLine_.empty()) {
        pv.moves = currentBestLine_;
        if (idx == 0) currentStatus_.bestMove = currentBestLine_.front();
    } else if (idx == 0 && !pv.moves.empty()) {
        currentStatus_.bestMove = pv.moves.front();
    }

    signal_analysis.emit(currentPVs_, currentStatus_);
}

static std::string decodeLabel(int val) {
    if (val == -1) return "";
    std::string s;
    for (int i = 3; i >= 0; --i) {
        char c = (val >> (i * 8)) & 0xFF;
        if (c != 0) s.push_back(c);
    }
    return s;
}

void GomocupProtocol::parseDatabase(const std::string &dbLine) {
    std::string trimLine = dbLine;
    // Trim leading space
    size_t start = trimLine.find_first_not_of(" ");
    if (start != std::string::npos) trimLine = trimLine.substr(start);

    if (trimLine.rfind("REFRESH", 0) == 0) {
        signal_database_refresh.emit();
        return;
    }
    if (trimLine.rfind("DONE", 0) == 0) {
        signal_database_done.emit();
        return;
    }

    // Format: y x labelValue value depth bound hasComment boardText
    std::istringstream ss(trimLine);
    int row, col, labelVal;
    if (!(ss >> row >> col >> labelVal)) {
        signal_log.emit(EngineMessageType::Error,
                         "GomocupProtocol: malformed DATABASE row (missing row/col/label): '"
                         + dbLine + "'");
        return;
    }

    DatabaseEntry entry;
    entry.pos = Coord{col, row};
    // boardSize_ may lag the engine's actual board size (see PROTO-02, out of
    // scope here); validate against MAX_BOARD_SIZE as a safe interim floor so
    // this never rejects a coordinate that's valid for the real board.
    if (!entry.pos.isValid(MAX_BOARD_SIZE)) {
        signal_log.emit(EngineMessageType::Error,
                         "GomocupProtocol: rejected out-of-range DATABASE coordinate ("
                         + std::to_string(row) + "," + std::to_string(col) + ")");
        return;
    }
    entry.label = decodeLabel(labelVal);

    if (!(ss >> entry.value >> entry.depth >> entry.bound)) {
        signal_log.emit(EngineMessageType::Error,
                         "GomocupProtocol: malformed DATABASE row (missing value/depth/bound): '"
                         + dbLine + "'");
        return;
    }
    int hasComment;
    if (ss >> hasComment) entry.hasComment = (hasComment != 0);
    
    // Remaining is boardText (optional)
    std::string rest;
    std::getline(ss, rest);
    // Trim leading spaces from rest
    size_t rstart = rest.find_first_not_of(" ");
    if (rstart != std::string::npos) entry.boardText = rest.substr(rstart);

    signal_database_entry.emit(entry);
}
