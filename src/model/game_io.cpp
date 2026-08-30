#include "game_io.h"

#include <cctype>
#include <fstream>
#include <set>
#include <string>

namespace {

std::string trim(const std::string &s)
{
    size_t b = 0;
    size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

void setError(std::string *error, const std::string &msg)
{
    if (error) *error = msg;
}

/// Parse a strictly-formatted non-negative integer ("123"). Returns false on
/// empty input, a stray sign, non-digits, or trailing garbage.
bool parseInt(const std::string &s, int &out)
{
    if (s.empty()) return false;
    for (char ch : s)
        if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
    try {
        size_t idx = 0;
        int    v   = std::stoi(s, &idx);
        if (idx != s.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

/// Parse "x,y" into a Coord. Returns false on any malformation.
bool parseCoord(const std::string &s, Coord &out)
{
    auto comma = s.find(',');
    if (comma == std::string::npos) return false;
    int x = 0;
    int y = 0;
    if (!parseInt(trim(s.substr(0, comma)), x)) return false;
    if (!parseInt(trim(s.substr(comma + 1)), y)) return false;
    out = Coord {x, y};
    return true;
}

} // namespace

namespace GameIO {

bool saveGame(const std::filesystem::path &path,
              int                           boardSize,
              GameRule                      rule,
              const std::vector<Coord>     &moves,
              std::string                  *error)
{
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        setError(error, "Cannot open file for writing: " + path.string());
        return false;
    }

    out << "# YixinBoard saved game\n";
    out << "yxgame_version=" << kFormatVersion << "\n";
    out << "board_size=" << boardSize << "\n";
    out << "rule=" << static_cast<int>(rule) << "\n";
    for (const auto &mv : moves)
        out << "move=" << mv.x << "," << mv.y << "\n";

    out.flush();
    if (!out.good()) {
        setError(error, "Write failed: " + path.string());
        return false;
    }
    return true;
}

std::optional<LoadedGame> loadGame(const std::filesystem::path &path,
                                   std::string                 *error)
{
    std::ifstream in(path);
    if (!in.is_open()) {
        setError(error, "Cannot open file: " + path.string());
        return std::nullopt;
    }

    bool     haveVersion   = false;
    bool     haveBoardSize = false;
    bool     haveRule      = false;
    LoadedGame result;
    std::set<std::pair<int, int>> seen;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) {
            setError(error, "Malformed line (no '='): " + line);
            return std::nullopt;
        }
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (key == "yxgame_version") {
            int v = 0;
            if (!parseInt(val, v) || v != kFormatVersion) {
                setError(error, "Unsupported or missing format version: " + val);
                return std::nullopt;
            }
            haveVersion = true;
        } else if (key == "board_size") {
            int v = 0;
            if (!parseInt(val, v) || v < 5 || v > MAX_BOARD_SIZE) {
                setError(error, "Invalid board_size: " + val);
                return std::nullopt;
            }
            result.boardSize = v;
            haveBoardSize    = true;
        } else if (key == "rule") {
            int v = 0;
            if (!parseInt(val, v) || v < 0 || v > 2) {
                setError(error, "Invalid rule: " + val);
                return std::nullopt;
            }
            result.rule = static_cast<GameRule>(v);
            haveRule    = true;
        } else if (key == "move") {
            Coord c;
            if (!parseCoord(val, c)) {
                setError(error, "Malformed move: " + val);
                return std::nullopt;
            }
            // board_size must precede moves so range-checking is meaningful.
            if (!haveBoardSize) {
                setError(error, "move appears before board_size");
                return std::nullopt;
            }
            if (!c.isValid(result.boardSize)) {
                setError(error, "Move out of range for board: " + val);
                return std::nullopt;
            }
            if (!seen.insert({c.x, c.y}).second) {
                setError(error, "Duplicate move: " + val);
                return std::nullopt;
            }
            result.moves.push_back(c);
        }
        // Unknown keys are ignored for forward tolerance.
    }

    if (!haveVersion) {
        setError(error, "Not a YixinBoard game file (no yxgame_version).");
        return std::nullopt;
    }
    if (!haveBoardSize) {
        setError(error, "Missing board_size.");
        return std::nullopt;
    }
    if (!haveRule) {
        setError(error, "Missing rule.");
        return std::nullopt;
    }

    return result;
}

} // namespace GameIO
