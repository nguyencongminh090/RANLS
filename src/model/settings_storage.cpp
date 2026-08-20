#include "settings_storage.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
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

    out.view.theme = static_cast<AppTheme>(parseNumber<int>(get("theme"), static_cast<int>(out.view.theme)));
    out.view.showMoveNumbers = parseBool(get("show_move_numbers"), out.view.showMoveNumbers);
    out.view.showCoordinates = parseBool(get("show_coordinates"), out.view.showCoordinates);
    int mode = parseNumber<int>(get("win_graph_mode"), static_cast<int>(out.view.winGraphMode));
    out.view.winGraphMode = (mode == static_cast<int>(WinGraphMode::SingleSide))
                                ? WinGraphMode::SingleSide
                                : WinGraphMode::BothSide;
    if (!get("ui_profile").empty()) out.view.uiProfile = get("ui_profile");
    if (!get("hotkey_analyze").empty()) out.view.hotkeyAnalyze = get("hotkey_analyze");
    if (!get("hotkey_stop").empty()) out.view.hotkeyStop = get("hotkey_stop");
    if (!get("hotkey_undo").empty()) out.view.hotkeyUndo = get("hotkey_undo");
    if (!get("hotkey_redo").empty()) out.view.hotkeyRedo = get("hotkey_redo");
    if (!get("hotkey_new_game").empty()) out.view.hotkeyNewGame = get("hotkey_new_game");

    return out;
}

bool save(const EngineConfig &engine, const ViewConfig &view)
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
    out << "theme=" << static_cast<int>(view.theme) << "\n";
    out << "show_move_numbers=" << (view.showMoveNumbers ? "true" : "false") << "\n";
    out << "show_coordinates=" << (view.showCoordinates ? "true" : "false") << "\n";
    out << "win_graph_mode=" << static_cast<int>(view.winGraphMode) << "\n";
    out << "ui_profile=" << escapeValue(view.uiProfile) << "\n";
    out << "hotkey_analyze=" << escapeValue(view.hotkeyAnalyze) << "\n";
    out << "hotkey_stop=" << escapeValue(view.hotkeyStop) << "\n";
    out << "hotkey_undo=" << escapeValue(view.hotkeyUndo) << "\n";
    out << "hotkey_redo=" << escapeValue(view.hotkeyRedo) << "\n";
    out << "hotkey_new_game=" << escapeValue(view.hotkeyNewGame) << "\n";

    return out.good();
}

} // namespace SettingsStorage
