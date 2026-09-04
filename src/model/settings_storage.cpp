#include "settings_storage.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#ifdef __linux__
#include <unistd.h>
#endif

namespace {

std::string trim(const std::string &s)
{
    size_t b = 0;
    size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string unescapeValue(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    bool escaped = false;
    for (char ch : s) {
        if (!escaped && ch == '\\') {
            escaped = true;
            continue;
        }
        if (escaped) {
            if (ch == 'n') out.push_back('\n');
            else out.push_back(ch);
            escaped = false;
        } else {
            out.push_back(ch);
        }
    }
    if (escaped) out.push_back('\\');
    return out;
}

std::string escapeValue(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        if (ch == '\\') out += "\\\\";
        else if (ch == '\n') out += "\\n";
        else out.push_back(ch);
    }
    return out;
}

bool parseBool(const std::string &v, bool fallback)
{
    std::string lower;
    lower.reserve(v.size());
    for (char ch : v) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    if (lower == "1" || lower == "true" || lower == "yes" || lower == "on") return true;
    if (lower == "0" || lower == "false" || lower == "no" || lower == "off") return false;
    return fallback;
}

template <typename T>
T parseNumber(const std::string &v, T fallback)
{
    std::string cleaned;
    cleaned.reserve(v.size());
    for (char ch : v) {
        if (ch != ',' && ch != '_')
            cleaned.push_back(ch);
    }

    try {
        size_t idx = 0;
        if constexpr (std::is_same_v<T, int>) {
            int value = std::stoi(cleaned, &idx);
            return idx == cleaned.size() ? value : fallback;
        } else {
            long long value = std::stoll(cleaned, &idx);
            return idx == cleaned.size() ? static_cast<T>(value) : fallback;
        }
    } catch (...) {
        return fallback;
    }
}

} // namespace

namespace {

std::filesystem::path executableDir()
{
#ifdef __linux__
    std::error_code ec;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) return exe.parent_path();
#endif
    return std::filesystem::current_path();
}

} // namespace

namespace SettingsStorage {

std::filesystem::path settingsFilePath()
{
    return executableDir() / "rapfi-gui.settings";
}

SettingsBundle load()
{
    SettingsBundle out;
    std::ifstream in(settingsFilePath());
    if (!in.is_open()) return out;

    std::unordered_map<std::string, std::string> kv;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = trim(line.substr(0, eq));
        auto val = trim(line.substr(eq + 1));
        if (!key.empty()) kv[key] = unescapeValue(val);
    }

    auto get = [&](const char *k) -> std::string {
        auto it = kv.find(k);
        return it == kv.end() ? std::string{} : it->second;
    };

    if (!get("engine_path").empty()) out.engine.enginePath = get("engine_path");
    out.engine.timeoutTurn = parseNumber<int64_t>(get("timeout_turn"), out.engine.timeoutTurn);
    out.engine.timeoutMatch = parseNumber<int64_t>(get("timeout_match"), out.engine.timeoutMatch);
    out.engine.increment = parseNumber<int>(get("increment"), out.engine.increment);
    out.engine.maxDepth = parseNumber<int>(get("max_depth"), out.engine.maxDepth);
    out.engine.maxNodes = parseNumber<int64_t>(get("max_nodes"), out.engine.maxNodes);
    out.engine.threads = parseNumber<int>(get("threads"), out.engine.threads);
    out.engine.hashSizeMB = parseNumber<int>(get("hash_size_mb"), out.engine.hashSizeMB);
    out.engine.multiPV = parseNumber<int>(get("multipv"), out.engine.multiPV);

    // customParams: any key written with the "custom_param." prefix by save()
    // below. Not part of the original on-disk format (see STATE-02) — added
    // so a value set via `!set` mid-session (customParams) actually survives
    // a Settings-dialog Apply + reload, not just the fields the dialog owns.
    static constexpr std::string_view kCustomParamPrefix = "custom_param.";
    for (const auto &[key, value] : kv) {
        if (key.rfind(kCustomParamPrefix, 0) == 0)
            out.engine.customParams[key.substr(kCustomParamPrefix.size())] = value;
    }

    out.view.theme = static_cast<AppTheme>(parseNumber<int>(get("theme"), static_cast<int>(out.view.theme)));
    out.view.showMoveNumbers = parseBool(get("show_move_numbers"), out.view.showMoveNumbers);
    out.view.showCoordinates = parseBool(get("show_coordinates"), out.view.showCoordinates);
    int mode = parseNumber<int>(get("win_graph_mode"), static_cast<int>(out.view.winGraphMode));
    out.view.winGraphMode = (mode == static_cast<int>(WinGraphMode::SingleSide))
                                ? WinGraphMode::SingleSide
                                : WinGraphMode::BothSide;
    // ANLZ-01: continuous background analysis toggle (default off).
    out.view.analyzeMode = parseBool(get("analyze_mode"), out.view.analyzeMode);
    // UX-06: `ui_profile` was removed (never had a spec). An old settings
    // file may still carry the key — it is silently ignored here, not an error.
    if (!get("hotkey_analyze").empty()) out.view.hotkeyAnalyze = get("hotkey_analyze");
    if (!get("hotkey_stop").empty()) out.view.hotkeyStop = get("hotkey_stop");
    if (!get("hotkey_undo").empty()) out.view.hotkeyUndo = get("hotkey_undo");
    if (!get("hotkey_redo").empty()) out.view.hotkeyRedo = get("hotkey_redo");
    if (!get("hotkey_new_game").empty()) out.view.hotkeyNewGame = get("hotkey_new_game");

    // UI-06: MatchConfig — which side (if any) the engine auto-plays. Any
    // value outside {0,1,2} falls back to the struct default (Off).
    int enginePlays = parseNumber<int>(get("engine_plays"), static_cast<int>(out.match.enginePlays));
    switch (enginePlays) {
        case static_cast<int>(EnginePlaysSide::Black): out.match.enginePlays = EnginePlaysSide::Black; break;
        case static_cast<int>(EnginePlaysSide::White): out.match.enginePlays = EnginePlaysSide::White; break;
        default:                                       out.match.enginePlays = EnginePlaysSide::Off;   break;
    }

    // STATE-04: GameSetupConfig — last-selected rule (global preference) and
    // board size (new-game default). Same validate-or-fallback idiom as the
    // engine_plays block: any out-of-range value falls back to the struct
    // default (Freestyle / DEFAULT_BOARD_SIZE).
    int rule = parseNumber<int>(get("rule"), static_cast<int>(out.setup.rule));
    switch (rule) {
        case static_cast<int>(GameRule::Freestyle): out.setup.rule = GameRule::Freestyle; break;
        case static_cast<int>(GameRule::Standard):  out.setup.rule = GameRule::Standard;  break;
        case static_cast<int>(GameRule::Renju):     out.setup.rule = GameRule::Renju;     break;
        default:                                    out.setup.rule = GameSetupConfig{}.rule; break;
    }
    int boardSize = parseNumber<int>(get("board_size"), out.setup.boardSize);
    out.setup.boardSize = (boardSize >= 5 && boardSize <= MAX_BOARD_SIZE)
                              ? boardSize
                              : GameSetupConfig{}.boardSize;

    return out;
}

bool save(const EngineConfig &engine, const ViewConfig &view, const MatchConfig &match,
          const GameSetupConfig &setup)
{
    auto path = settingsFilePath();
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return false;

    out << "# Rapfi GUI user settings\n";
    out << "engine_path=" << escapeValue(engine.enginePath) << "\n";
    out << "timeout_turn=" << engine.timeoutTurn << "\n";
    out << "timeout_match=" << engine.timeoutMatch << "\n";
    out << "increment=" << engine.increment << "\n";
    out << "max_depth=" << engine.maxDepth << "\n";
    out << "max_nodes=" << engine.maxNodes << "\n";
    out << "threads=" << engine.threads << "\n";
    out << "hash_size_mb=" << engine.hashSizeMB << "\n";
    out << "multipv=" << engine.multiPV << "\n";
    for (const auto &[key, value] : engine.customParams)
        out << "custom_param." << key << "=" << escapeValue(value) << "\n";
    out << "theme=" << static_cast<int>(view.theme) << "\n";
    out << "show_move_numbers=" << (view.showMoveNumbers ? "true" : "false") << "\n";
    out << "show_coordinates=" << (view.showCoordinates ? "true" : "false") << "\n";
    out << "win_graph_mode=" << static_cast<int>(view.winGraphMode) << "\n";
    out << "analyze_mode=" << (view.analyzeMode ? "true" : "false") << "\n";
    out << "hotkey_analyze=" << escapeValue(view.hotkeyAnalyze) << "\n";
    out << "hotkey_stop=" << escapeValue(view.hotkeyStop) << "\n";
    out << "hotkey_undo=" << escapeValue(view.hotkeyUndo) << "\n";
    out << "hotkey_redo=" << escapeValue(view.hotkeyRedo) << "\n";
    out << "hotkey_new_game=" << escapeValue(view.hotkeyNewGame) << "\n";
    out << "engine_plays=" << static_cast<int>(match.enginePlays) << "\n";
    // STATE-04: last-selected rule (global preference) + board size (new-game default).
    out << "rule=" << static_cast<int>(setup.rule) << "\n";
    out << "board_size=" << setup.boardSize << "\n";

    return out.good();
}

} // namespace SettingsStorage
