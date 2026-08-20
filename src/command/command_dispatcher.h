#pragma once

#include "command.h"
#include "model/board_state.h"
#include "model/config.h"
#include "model/game_state.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class EngineProcess;
class EngineController;

/// Execution environment for a command handler.
struct CommandContext {
    GameState &gameState;
    EngineProcess &engine;
    EngineController &controller;

    // UI hooks (implemented by MainWindow via BottomPanel).
    std::function<void(const std::string &)> print;       ///< Append a console output line.
    std::function<void()>                   clearConsole; ///< Clear console output.
};

/// Metadata to show in help.
struct CommandSpec {
    std::string group;       ///< Help group name (e.g. "engine", "board").
    std::string name;        ///< Command name.
    std::string usage;       ///< Usage string.
    std::string summary;     ///< One-line summary.
};

/// Minimal command bus: registry + parser + stateful handlers.
class CommandDispatcher {
public:
    explicit CommandDispatcher(CommandContext ctx);

    /// Execute a raw line. Returns true if the command was recognized/handled.
    bool executeLine(const std::string &line);

    /// Print help for all commands.
    void printHelp() const;

private:
    using Handler = std::function<void(const Command &)>;

    void registerBuiltins();
    void registerCommand(CommandSpec spec, Handler handler);

    void printError(const std::string &msg) const;
    void printInfo(const std::string &msg) const;

    // Helpers for handlers.
    std::optional<int> parseIntArg(const std::string &s) const;
    std::optional<int64_t> parseInt64Arg(const std::string &s) const;
    std::optional<GameRule> parseRule(const std::string &s) const;

    void setEngineConfigKey(const std::string &key, const std::string &value);

    // Position input session for loadpos/pos ... DONE.
    struct PosSession {
        std::vector<std::string> lines; // raw user lines (move text, url, etc.)
    };

    void beginPosSession();
    void handlePosSessionLine(const std::string &line);
    void finalizePosSession();

    // Parse move text into a list of moves. Accepts "h7h8h12", "h7 h8 h12",
    // or embedded in URLs like "playok.com/.../h7blackh8h12".
    static std::vector<Coord> parseMovesText(const std::string &text, int boardSize);

    // Is this external/raw line dangerous to send without syncing UI state?
    static bool isDangerousExternalEngineCommand(const std::string &line);

    CommandContext ctx_;
    std::vector<CommandSpec> specs_;
    std::unordered_map<std::string, Handler> handlers_;
    std::optional<PosSession> posSession_;
};

