#pragma once

#include <string>
#include <string_view>
#include <vector>

/// Parsed user command from the in-app console.
struct Command {
    std::string raw;                  ///< Full unmodified input line.
    std::string name;                 ///< Lowercased command name.
    std::vector<std::string> args;    ///< Tokenized args (no quotes).
    std::string tail;                 ///< Raw substring after command name (trimmed left).
};

struct CommandParseResult {
    bool    ok = false;
    Command cmd;
    std::string error;
};

/// Parse a single-line command string into name/args.
CommandParseResult parseCommandLine(std::string_view line);

