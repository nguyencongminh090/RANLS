#include "command_dispatcher.h"

#include "engine/engine_process.h"
#include "engine/engine_controller.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <optional>
#include <sstream>

static std::string coordToMoveText(Coord c, int boardSize)
{
    if (!c.isValid(boardSize)) return "?";
    std::string out;
    out.push_back(static_cast<char>('a' + c.x));
    out += std::to_string(boardSize - c.y);
    return out;
}

static std::string joinArgs(const std::vector<std::string> &args, size_t startIdx = 0)
{
    std::string out;
    for (size_t i = startIdx; i < args.size(); ++i) {
        if (!out.empty()) out += " ";
        out += args[i];
    }
    return out;
}

CommandDispatcher::CommandDispatcher(CommandContext ctx)
    : ctx_(std::move(ctx))
{
    registerBuiltins();
}

void CommandDispatcher::registerCommand(CommandSpec spec, Handler handler)
{
    handlers_[spec.name] = std::move(handler);
    specs_.push_back(std::move(spec));
}

void CommandDispatcher::printError(const std::string &msg) const
{
    if (ctx_.print) ctx_.print("ERR: " + msg);
}

void CommandDispatcher::printInfo(const std::string &msg) const
{
    if (ctx_.print) ctx_.print(msg);
}

void CommandDispatcher::revertEnginePlaysIfEnginesTurn()
{
    // ENG-02: same rule as MainWindow::onStartAnalysis — analyzing on the
    // engine's own assigned turn cancels auto-play. Shared pure predicate in
    // model/config.h; in-memory MatchConfig only (no SettingsStorage::save).
    MatchConfig mc = ctx_.gameState.matchConfig();
    if (!isEnginesTurn(mc.enginePlays, ctx_.gameState.board().sideToMove()))
        return;
    mc.enginePlays = EnginePlaysSide::Off;
    ctx_.gameState.setMatchConfig(mc);
}

std::optional<int> CommandDispatcher::parseIntArg(const std::string &s) const
{
    try {
        size_t idx = 0;
        int v = std::stoi(s, &idx);
        if (idx != s.size()) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int64_t> CommandDispatcher::parseInt64Arg(const std::string &s) const
{
    try {
        size_t idx = 0;
        int64_t v = std::stoll(s, &idx);
        if (idx != s.size()) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<GameRule> CommandDispatcher::parseRule(const std::string &s) const
{
    std::string v;
    v.reserve(s.size());
    for (char c : s) v.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (v == "freestyle") return GameRule::Freestyle;
    if (v == "standard") return GameRule::Standard;
    if (v == "renju") return GameRule::Renju;
    return std::nullopt;
}

bool CommandDispatcher::executeLine(const std::string &line)
{
    // If we’re in a position-input session, route raw lines there.
    if (posSession_) {
        handlePosSessionLine(line);
        return true;
    }

    // Internal commands are prefixed with '!'. Everything else is treated as
    // external/raw engine protocol line (with safety restrictions).
    auto trimLeft = [](const std::string &s) -> std::string {
        size_t i = 0;
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        return s.substr(i);
    };

    std::string in = trimLeft(line);
    if (in.empty()) return true;

    // Support ASCII '!' and fullwidth '！' (common in some IMEs).
    bool asciiBang = (!in.empty() && in[0] == '!');
    bool fullwidthBang = (in.rfind("！", 0) == 0); // UTF-8 fullwidth exclamation

    if (asciiBang || fullwidthBang) {
        std::string internal = trimLeft(in.substr(asciiBang ? 1 : std::string("！").size()));
        auto parsed = parseCommandLine(internal);
        if (!parsed.ok) {
            if (parsed.error != "Empty command") printError(parsed.error);
            return true;
        }

        auto it = handlers_.find(parsed.cmd.name);
        if (it == handlers_.end()) {
            printError("Unknown internal command: " + parsed.cmd.name + " (try: !help)");
            return false;
        }

        it->second(parsed.cmd);
        return true;
    }

    // External/raw protocol line.
    if (!ctx_.engine.isRunning()) {
        printError("Engine not running. Use: !engine start");
        return true;
    }

    // If the user typed an internal command without '!', warn and don't forward.
    {
        auto parsed = parseCommandLine(in);
        if (parsed.ok) {
            if (handlers_.find(parsed.cmd.name) != handlers_.end()) {
                printError("This is an internal command. Prefix with '!': !" + parsed.cmd.raw);
                return true;
            }
        }
    }

    if (isDangerousExternalEngineCommand(in)) {
        printError("Blocked external command (would desync UI). Use internal commands instead:");
        printError("  - for position: !loadpos / !play");
        printError("  - for new game: !start / !new / !rule");
        printError("  - or force raw: !send <line>");
        return true;
    }
    ctx_.controller.sendRawCommand(in);
    return true;
}

void CommandDispatcher::printHelp() const
{
    // Group specs by group name.
    std::unordered_map<std::string, std::vector<const CommandSpec *>> groups;
    for (const auto &s : specs_) {
        groups[s.group].push_back(&s);
    }

    std::vector<std::string> groupNames;
    groupNames.reserve(groups.size());
    for (const auto &kv : groups) groupNames.push_back(kv.first);
    std::sort(groupNames.begin(), groupNames.end());

    printInfo("Commands (v1).");
    printInfo("  - Internal commands must start with '!': they keep UI + engine in sync.");
    printInfo("  - Lines without '!' are sent as raw protocol to the engine (restricted).");
    printInfo("Tip: use quotes for values with spaces.");
    for (const auto &g : groupNames) {
        printInfo("");
        printInfo("[" + g + "]");
        auto list = groups[g];
        std::sort(list.begin(), list.end(), [](const CommandSpec *a, const CommandSpec *b) {
            return a->name < b->name;
        });
        for (const auto *s : list) {
            std::string line = "  " + s->usage;
            if (!s->summary.empty()) line += " — " + s->summary;
            printInfo(line);
        }
    }
}

void CommandDispatcher::registerBuiltins()
{
    registerCommand(
        {"info", "help", "!help", "Show this help (grouped)"},
        [this](const Command &) { printHelp(); });

    registerCommand(
        {"info", "about", "!about", "Send ABOUT to the engine"},
        [this](const Command &) {
            if (!ctx_.engine.isRunning()) {
                printError("Engine not running. Use: !engine start");
                return;
            }
            ctx_.controller.sendRawCommand("ABOUT");
        });

    registerCommand(
        {"board", "start", "!start <size>", "New game with board size; sync engine if running"},
        [this](const Command &c) {
            if (c.args.size() != 1) {
                printError("Usage: start <size>");
                return;
            }
            auto size = parseIntArg(c.args[0]);
            if (!size || *size < 5 || *size > MAX_BOARD_SIZE) {
                printError("Invalid size. Expected 5.." + std::to_string(MAX_BOARD_SIZE));
                return;
            }
            ctx_.controller.stopAnalysis();
            ctx_.gameState.newGame(*size);
            ctx_.controller.sendConfig();
            printInfo("OK: new game, size " + std::to_string(*size));
        });

    registerCommand(
        {"board", "rule", "!rule <freestyle|standard|renju>", "Set rule and sync engine"},
        [this](const Command &c) {
            if (c.args.size() != 1) {
                printError("Usage: rule <freestyle|standard|renju>");
                return;
            }
            auto r = parseRule(c.args[0]);
            if (!r) {
                printError("Unknown rule: " + c.args[0]);
                return;
            }
            ctx_.gameState.setRule(*r);
            ctx_.controller.sendConfig();
            printInfo("OK: rule set");
        });

    registerCommand(
        {"board", "new", "!new", "Restart current game (keep size)"},
        [this](const Command &) {
            ctx_.controller.stopAnalysis();
            ctx_.gameState.newGame(ctx_.gameState.boardSize());
            ctx_.controller.sendConfig();
            printInfo("OK: new game");
        });

    registerCommand(
        {"board", "undo", "!undo", "Undo one move"},
        [this](const Command &) {
            if (!ctx_.gameState.undoMove()) {
                printError("Cannot undo (maybe analyzing or empty)");
                return;
            }
            printInfo("OK");
        });

    registerCommand(
        {"board", "redo", "!redo", "Redo one move"},
        [this](const Command &) {
            if (!ctx_.gameState.redoMove()) {
                printError("Cannot redo");
                return;
            }
            printInfo("OK");
        });

    registerCommand(
        {"analysis", "analyze", "!analyze [n]", "Start analysis (optional n = MultiPV)"},
        [this](const Command &c) {
            if (!ctx_.engine.isRunning()) {
                printError("Engine not running. Use: !engine start");
                return;
            }

            if (c.args.size() > 1) {
                printError("Usage: analyze [n]");
                return;
            }

            if (c.args.size() == 1) {
                auto n = parseIntArg(c.args[0]);
                if (!n || *n <= 0 || *n > 99) {
                    printError("Invalid n. Expected 1..99");
                    return;
                }
                auto cfg = ctx_.gameState.engineConfig();
                cfg.multiPV = *n;
                ctx_.gameState.setEngineConfig(cfg);
                ctx_.controller.sendConfig();
            }

            revertEnginePlaysIfEnginesTurn();  // ENG-02
            ctx_.controller.analyze();
        });

    registerCommand(
        {"analysis", "stop", "!stop", "Stop analysis (STOP)"},
        [this](const Command &) { ctx_.controller.stopAnalysis(); });

    registerCommand(
        {"engine", "engine", "!engine <start|stop|reload>", "Engine lifecycle"},
        [this](const Command &c) {
            if (c.args.size() != 1) {
                printError("Usage: engine <start|stop|reload>");
                return;
            }
            std::string sub = c.args[0];
            std::transform(sub.begin(), sub.end(), sub.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

            if (sub == "start") {
                auto &cfg = ctx_.gameState.engineConfig();
                if (cfg.enginePath.empty()) {
                    printError("enginePath is empty (set it in Settings).");
                    return;
                }
                ctx_.controller.startEngine();
                ctx_.controller.sendConfig();
                printInfo("OK: engine started");
            } else if (sub == "stop") {
                ctx_.controller.stopEngine();
                printInfo("OK: engine stopped");
            } else if (sub == "reload") {
                ctx_.controller.reloadEngine();
                printInfo("OK: engine reloaded");
            } else {
                printError("Unknown subcommand: " + sub);
            }
        });

    registerCommand(
        {"engine", "send", "!send <raw engine line>", "Send a raw protocol line to the engine"},
        [this](const Command &c) {
            if (c.tail.empty()) {
                printError("Usage: send <raw engine line>");
                return;
            }
            if (!ctx_.engine.isRunning()) {
                printError("Engine not running. Use: !engine start");
                return;
            }
            ctx_.controller.sendRawCommand(c.tail);
        });

    registerCommand(
        {"config", "info", "!info set <key> <value>", "Edit engine config and sendConfig"},
        [this](const Command &c) {
            if (c.args.size() < 1) {
                printError("Usage: info set <key> <value>");
                return;
            }
            if (c.args[0] != "set") {
                printError("Usage: info set <key> <value>");
                return;
            }
            if (c.args.size() < 3) {
                printError("Usage: info set <key> <value>");
                return;
            }
            const std::string &key = c.args[1];
            std::string value = joinArgs(c.args, 2);
            setEngineConfigKey(key, value);
        });

    registerCommand(
        {"board", "loadpos", "!loadpos", "Load position from move text (end with: done)"},
        [this](const Command &) { beginPosSession(); });

    registerCommand(
        {"board", "pos", "!pos <moveText...> | !pos (then lines, end with: done)", "Position input helper"},
        [this](const Command &c) {
            if (c.args.empty()) {
                beginPosSession();
                return;
            }
            // One-liner: pos <text...>
            beginPosSession();
            for (const auto &a : c.args) {
                handlePosSessionLine(a);
            }
            finalizePosSession();
        });

    registerCommand(
        {"board", "getpos", "!getpos", "Print current position as y,x,color lines"},
        [this](const Command &) {
            const int boardSize = ctx_.gameState.boardSize();
            const auto &board = ctx_.gameState.board();
            std::vector<std::string> lines;
            lines.reserve(boardSize * boardSize);

            int blackCount = 0;
            int whiteCount = 0;
            for (int y = 0; y < boardSize; ++y) {
                for (int x = 0; x < boardSize; ++x) {
                    Stone s = board.stoneAt(Coord{x, y});
                    if (s == Stone::Empty) continue;
                    int color = (s == Stone::Black) ? 1 : 2;
                    if (s == Stone::Black) ++blackCount;
                    else ++whiteCount;
                    lines.push_back(std::to_string(y) + "," + std::to_string(x) + "," + std::to_string(color));
                }
            }

            printInfo("POS_BEGIN");
            for (const auto &line : lines) printInfo(line);
            printInfo("DONE");

            std::string moveText;
            for (int i = 0; i < ctx_.gameState.history().moveCount(); ++i) {
                moveText += coordToMoveText(ctx_.gameState.history().moves()[i], boardSize);
            }
            std::ostringstream summary;
            summary << "POS_SUMMARY size=" << boardSize
                    << " ply=" << ctx_.gameState.history().moveCount()
                    << " black=" << blackCount
                    << " white=" << whiteCount
                    << " moves=" << moveText;
            printInfo(summary.str());
        });

    registerCommand(
        {"analysis", "play", "!play <moveText...>", "Load moves then start analysis"},
        [this](const Command &c) {
            if (!ctx_.engine.isRunning()) {
                printError("Engine not running. Use: !engine start");
                return;
            }
            if (c.tail.empty()) {
                printError("Usage: !play <moveText...>  (e.g. !play h7h8h12)");
                return;
            }

            auto moves = parseMovesText(c.tail, ctx_.gameState.boardSize());
            if (moves.empty()) {
                printError("No moves parsed.");
                return;
            }

            std::vector<std::pair<Coord, Stone>> stones;
            stones.reserve(moves.size());
            for (size_t i = 0; i < moves.size(); ++i) {
                stones.push_back({moves[i], (i % 2 == 0) ? Stone::Black : Stone::White});
            }

            ctx_.controller.stopAnalysis();
            if (!ctx_.gameState.loadPosition(stones)) {
                printError("Failed to load position (invalid/duplicate or analyzing).");
                return;
            }
            ctx_.controller.sendConfig();
            revertEnginePlaysIfEnginesTurn();  // ENG-02
            ctx_.controller.analyze();
        });

    registerCommand(
        {"debug", "clear", "!clear", "Clear the console log"},
        [this](const Command &) {
            if (ctx_.clearConsole) ctx_.clearConsole();
        });

    registerCommand(
        {"database", "db", "!db <query|load|save|on|off|label|comment|delete>", "Database commands"},
        [this](const Command &c) {
            if (c.args.empty()) {
                printInfo("Usage: !db <query|load|save|on|off|label|comment|delete>");
                return;
            }
            std::string sub = c.args[0];
            if (sub == "query") {
                ctx_.controller.queryDatabase();
            } else if (sub == "on") {
                auto v = ctx_.gameState.viewConfig();
                v.showDatabase = true;
                ctx_.gameState.setViewConfig(v);
                ctx_.controller.sendRawCommand("info usedatabase 1");
                ctx_.controller.queryDatabase();
                printInfo("Database markers enabled.");
            } else if (sub == "off") {
                auto v = ctx_.gameState.viewConfig();
                v.showDatabase = false;
                ctx_.gameState.setViewConfig(v);
                ctx_.controller.sendRawCommand("info usedatabase 0");
                ctx_.gameState.clearDatabase();
                ctx_.gameState.updateDatabase();
                printInfo("Database markers disabled.");
            } else if (sub == "load") {
                if (c.args.size() < 2) {
                    printError("Usage: !db load <path>");
                    return;
                }
                std::string path = joinArgs(c.args, 1);
                ctx_.controller.sendRawCommand("YXSETDATABASE \"" + path + "\"");
                printInfo("Database load command sent.");
            } else if (sub == "save") {
                ctx_.controller.sendRawCommand("YXSAVEDATABASE");
                printInfo("Save command sent.");
            } else if (sub == "label") {
                if (c.args.size() < 3) {
                    printError("Usage: !db label <coord> <text4>");
                    return;
                }
                auto moves = parseMovesText(c.args[1], ctx_.gameState.boardSize());
                if (moves.empty()) {
                    printError("Invalid coordinate: " + c.args[1]);
                    return;
                }
                Coord pos = moves[0];
                std::string label = c.args[2];
                ctx_.controller.sendRawCommand("YXEDITLABELDATABASE");
                ctx_.controller.sendRawCommand(std::to_string(pos.x) + "," + std::to_string(pos.y));
                ctx_.controller.sendRawCommand(label);
                ctx_.controller.queryDatabase();
            } else if (sub == "comment") {
                if (c.args.size() < 3) {
                    printError("Usage: !db comment <coord> <text...>");
                    return;
                }
                auto moves = parseMovesText(c.args[1], ctx_.gameState.boardSize());
                if (moves.empty()) {
                    printError("Invalid coordinate: " + c.args[1]);
                    return;
                }
                Coord pos = moves[0];
                std::string text = joinArgs(c.args, 2);
                ctx_.controller.sendRawCommand("YXEDITTEXTDATABASE");
                ctx_.controller.sendRawCommand(std::to_string(pos.x) + "," + std::to_string(pos.y));
                ctx_.controller.sendRawCommand(text);
                ctx_.controller.queryDatabase();
            } else if (sub == "delete") {
                if (c.args.size() < 2) {
                    printError("Usage: !db delete <coord>");
                    return;
                }
                auto moves = parseMovesText(c.args[1], ctx_.gameState.boardSize());
                if (moves.empty()) {
                    printError("Invalid coordinate: " + c.args[1]);
                    return;
                }
                Coord pos = moves[0];
                ctx_.controller.sendRawCommand("YXDELETEDATABASEONE");
                ctx_.controller.sendRawCommand(std::to_string(pos.x) + "," + std::to_string(pos.y));
                ctx_.controller.sendRawCommand("DONE");
                ctx_.controller.queryDatabase();
            } else {
                printError("Unknown database subcommand: " + sub);
            }
        });
}

void CommandDispatcher::setEngineConfigKey(const std::string &key, const std::string &value)
{
    // Map common protocol-ish keys to EngineConfig fields.
    auto cfg = ctx_.gameState.engineConfig();

    auto lowerKey = key;
    std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    auto setInt = [&](auto &field) -> bool {
        auto v = parseInt64Arg(value);
        if (!v) return false;
        field = static_cast<std::decay_t<decltype(field)>>(*v);
        return true;
    };

    bool ok = true;
    if (lowerKey == "timeout_turn") ok = setInt(cfg.timeoutTurn);
    else if (lowerKey == "timeout_match") ok = setInt(cfg.timeoutMatch);
    else if (lowerKey == "time_increment") ok = setInt(cfg.increment);
    else if (lowerKey == "max_depth") ok = setInt(cfg.maxDepth);
    else if (lowerKey == "max_node" || lowerKey == "max_nodes") ok = setInt(cfg.maxNodes);
    else if (lowerKey == "thread_num" || lowerKey == "threads") ok = setInt(cfg.threads);
    else if (lowerKey == "hash_size" || lowerKey == "hash_size_mb") ok = setInt(cfg.hashSizeMB);
    else if (lowerKey == "multipv") ok = setInt(cfg.multiPV);
    else if (lowerKey == "show_detail") {
        cfg.customParams["SHOW_DETAIL"] = value;
    }
    else if (lowerKey == "engine_path" || lowerKey == "enginepath") cfg.enginePath = value;
    else {
        cfg.customParams[key] = value;
        ok = true;
    }

    if (!ok) {
        printError("Invalid value for " + key + ": " + value);
        return;
    }

    ctx_.gameState.setEngineConfig(cfg);
    ctx_.controller.sendConfig();
    printInfo("OK: config updated");
}

void CommandDispatcher::beginPosSession()
{
    posSession_ = PosSession{};
    printInfo("Position input mode: paste move text like \"h7h8h12\".");
    printInfo("End with: done");
}

void CommandDispatcher::handlePosSessionLine(const std::string &line)
{
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (lower == "done") {
        finalizePosSession();
        return;
    }

    if (!posSession_) return;
    posSession_->lines.push_back(line);
}

std::vector<Coord> CommandDispatcher::parseMovesText(const std::string &text, int boardSize)
{
    std::vector<Coord> moves;
    if (boardSize <= 0) return moves;

    auto tryMapX = [&](char file, bool skipI) -> std::optional<int> {
        if (file < 'a' || file > 'z') return std::nullopt;
        int x = (file - 'a');
        if (skipI && file > 'i') x -= 1; // skip 'i'
        if (x < 0 || x >= boardSize) return std::nullopt;
        return x;
    };

    auto toLower = [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };

    // Scan for patterns: <letter><digits>
    for (size_t i = 0; i + 1 < text.size(); ++i) {
        char c = toLower(text[i]);
        if (c < 'a' || c > 'z') continue;

        size_t j = i + 1;
        if (j >= text.size() || !std::isdigit(static_cast<unsigned char>(text[j])))
            continue;

        int num = 0;
        while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) {
            num = num * 10 + (text[j] - '0');
            ++j;
        }

        if (num <= 0 || num > boardSize) {
            i = j - 1;
            continue;
        }

        int y = boardSize - num;
        if (y < 0 || y >= boardSize) {
            i = j - 1;
            continue;
        }

        // Try common coordinate mappings: without skipping 'i' first, then skip 'i'.
        std::optional<int> x = tryMapX(c, false);
        if (!x) x = tryMapX(c, true);
        if (!x) {
            i = j - 1;
            continue;
        }

        moves.push_back(Coord{*x, y});
        i = j - 1;
    }

    return moves;
}

bool CommandDispatcher::isDangerousExternalEngineCommand(const std::string &line)
{
    // Extract first token and compare case-insensitively.
    size_t i = 0;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
    size_t j = i;
    while (j < line.size() && !std::isspace(static_cast<unsigned char>(line[j]))) ++j;
    if (j <= i) return false;

    std::string tok = line.substr(i, j - i);
    std::transform(tok.begin(), tok.end(), tok.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    // Commands that change engine-side board/session state and will desync the UI.
    return tok == "start" || tok == "restart" || tok == "rectstart" || tok == "turn" || tok == "begin"
           || tok == "board" || tok == "yxboard" || tok == "takeback" || tok == "swap2board"
           || tok == "end" || tok == "stop" || tok == "yxstop" || tok == "info";
}

void CommandDispatcher::finalizePosSession()
{
    if (!posSession_) return;

    std::string blob;
    for (const auto &l : posSession_->lines) {
        if (!blob.empty()) blob.push_back(' ');
        blob += l;
    }

    auto moves = parseMovesText(blob, ctx_.gameState.boardSize());
    if (moves.empty()) {
        printError("No moves parsed.");
        posSession_.reset();
        return;
    }

    std::vector<std::pair<Coord, Stone>> stones;
    stones.reserve(moves.size());
    for (size_t i = 0; i < moves.size(); ++i) {
        stones.push_back({moves[i], (i % 2 == 0) ? Stone::Black : Stone::White});
    }

    ctx_.controller.stopAnalysis();

    if (!ctx_.gameState.loadPosition(stones)) {
        printError("Failed to load position (invalid/duplicate or analyzing)");
        posSession_.reset();
        return;
    }

    ctx_.controller.sendConfig(); // keep engine in sync if running
    printInfo("OK: position loaded (" + std::to_string(stones.size()) + " moves)");
    posSession_.reset();
}
