#pragma once

// CONS-01: pure, GTK-free helpers for the Engine Log command entry's
// AutoComplete UI. Mirrors the "usable from a pure-logic unit test with no
// display server" property of src/ui/engine_log_model.h — this header depends
// on nothing but the standard library, so the matching / longest-common-prefix
// / activation-predicate logic is unit-tested without a display server
// (tests/test_cons01_command_completer.cpp). All GTK glue lives in
// src/ui/bottom_panel.cpp.
//
// Scope (features/console-autocomplete/planning.md Q6): command-NAME completion
// only. No argument-level completion, no "did you mean" (that is CONS-02).

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace command_completer {

inline char asciiLower(char c)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

inline std::string toLower(std::string s)
{
    for (char &c : s)
        c = asciiLower(c);
    return s;
}

/// Names from `names` that share `prefix` (case-insensitive), returned sorted
/// ascending and de-duplicated. An empty prefix matches every name.
inline std::vector<std::string> matches(const std::string              &prefix,
                                        const std::vector<std::string> &names)
{
    const std::string p = toLower(prefix);
    std::vector<std::string> out;
    for (const auto &n : names) {
        if (toLower(n).rfind(p, 0) == 0)
            out.push_back(n);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

/// Longest common prefix of `m`, case-insensitive when comparing but keeping
/// the casing of the first element for the returned string. Empty if `m` is.
inline std::string longestCommonPrefix(const std::vector<std::string> &m)
{
    if (m.empty())
        return {};
    std::string lcp = m.front();
    for (std::size_t i = 1; i < m.size(); ++i) {
        const std::string &s = m[i];
        std::size_t k = 0;
        while (k < lcp.size() && k < s.size() && asciiLower(lcp[k]) == asciiLower(s[k]))
            ++k;
        lcp.resize(k);
    }
    return lcp;
}

/// UTF-8 fullwidth exclamation mark '！' (U+FF01), accepted like ASCII '!'
/// exactly as CommandDispatcher::executeLine() already normalises it.
inline const std::string &fullwidthBang()
{
    static const std::string kBang = "\xEF\xBC\x81";
    return kBang;
}

/// If `s` begins with a bang ('!' or '！'), returns its byte length via
/// `bangLen` and true; otherwise false.
inline bool startsWithBang(const std::string &s, std::size_t &bangLen)
{
    if (!s.empty() && s[0] == '!') {
        bangLen = 1;
        return true;
    }
    if (s.rfind(fullwidthBang(), 0) == 0) {
        bangLen = fullwidthBang().size();
        return true;
    }
    return false;
}

namespace detail {
/// [start-of-first-token, end-of-first-token) as byte offsets into `text`,
/// where the first token is the leading `!`command name. `ok` is false when
/// `text` (after leading whitespace) does not start with a bang.
struct FirstToken {
    bool        ok    = false;
    std::size_t start = 0;  ///< offset of the bang
    std::size_t end   = 0;  ///< offset of the first whitespace after the name (or text end)
    std::size_t nameStart = 0;  ///< offset just past the bang
};

inline FirstToken firstToken(const std::string &text)
{
    FirstToken ft;
    std::size_t s = 0;
    while (s < text.size() && std::isspace(static_cast<unsigned char>(text[s])))
        ++s;
    if (s >= text.size())
        return ft;
    std::size_t bangLen = 0;
    if (!startsWithBang(text.substr(s), bangLen))
        return ft;
    ft.start     = s;
    ft.nameStart = s + bangLen;
    std::size_t e = ft.nameStart;
    while (e < text.size() && !std::isspace(static_cast<unsigned char>(text[e])))
        ++e;
    ft.end = e;
    ft.ok  = true;
    return ft;
}
}  // namespace detail

/// The command-name fragment already typed after the leading bang, "" when
/// `shouldSuggest` would be false for the same text.
inline std::string currentPrefix(const std::string &text)
{
    auto ft = detail::firstToken(text);
    if (!ft.ok)
        return {};
    return text.substr(ft.nameStart, ft.end - ft.nameStart);
}

/// Activation predicate (R2): the trimmed entry text starts with a bang and
/// the caret is within the first (command-name) token. `caretPos` is a
/// character offset from the start of the text; for the ASCII bang it equals
/// the byte offset, which is all this predicate needs.
inline bool shouldSuggest(const std::string &text, int caretPos)
{
    auto ft = detail::firstToken(text);
    if (!ft.ok || caretPos < 0)
        return false;
    const auto caret = static_cast<std::size_t>(caretPos);
    return caret >= ft.start && caret <= ft.end;
}

// ── CONS-02: AutoCorrect helpers (syntax fix-ups applied on submit) ────────
//
// All pure — unit-tested in tests/test_cons02_autocorrect.cpp. The UI glue
// (visible entry rewrite + one "corrected:" log line) lives in
// src/ui/bottom_panel.cpp; the unknown-command "did you mean" wiring lives in
// CommandDispatcher::executeLine() via CommandDispatcher::nearestNames().

/// Classic Levenshtein edit distance, case-insensitive. Strings here are short
/// command names, so the O(m*n) two-row DP is more than fast enough.
inline std::size_t levenshtein(const std::string &a, const std::string &b)
{
    const std::size_t m = a.size();
    const std::size_t n = b.size();
    std::vector<std::size_t> prev(n + 1), cur(n + 1);
    for (std::size_t j = 0; j <= n; ++j)
        prev[j] = j;
    for (std::size_t i = 1; i <= m; ++i) {
        cur[0] = i;
        for (std::size_t j = 1; j <= n; ++j) {
            const std::size_t cost = (asciiLower(a[i - 1]) == asciiLower(b[j - 1])) ? 0 : 1;
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
        }
        std::swap(prev, cur);
    }
    return prev[n];
}

/// B2 — wrong case. If `token` equals a registered name case-insensitively but
/// not exactly, return that name's *registered* spelling. nullopt when `token`
/// already is an exact registered name, or matches nothing at all (both mean
/// "nothing to correct here").
inline std::optional<std::string> normalizeCase(const std::string              &token,
                                                const std::vector<std::string> &names)
{
    for (const auto &n : names)
        if (n == token)
            return std::nullopt;
    const std::string lo = toLower(token);
    for (const auto &n : names)
        if (toLower(n) == lo)
            return n;
    return std::nullopt;
}

/// B4 — did you mean. Up to 3 registered names within Levenshtein <= 2 AND
/// <= floor(len/2) of `token` (len = token length), case-insensitive, best
/// (smallest distance, then lexicographic) first. Exact matches (distance 0)
/// are never suggested.
inline std::vector<std::string> didYouMean(const std::string              &token,
                                           const std::vector<std::string> &names)
{
    const std::size_t cap = std::min<std::size_t>(2, token.size() / 2);
    std::vector<std::pair<std::size_t, std::string>> scored;
    for (const auto &n : names) {
        const std::size_t d = levenshtein(token, n);
        if (d >= 1 && d <= cap)
            scored.emplace_back(d, n);
    }
    std::sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) {
        return a.first != b.first ? a.first < b.first : a.second < b.second;
    });
    std::vector<std::string> out;
    for (std::size_t i = 0; i < scored.size() && out.size() < 3; ++i)
        out.push_back(scored[i].second);
    return out;
}

/// B1 — missing bang. If `entryText` (ignoring leading whitespace) does NOT
/// begin with a bang and its first whitespace-delimited token is an *exact*
/// case-insensitive match to a registered name, return the rewrite
/// "<leading ws>!<registered spelling><rest verbatim>". nullopt otherwise:
/// a bang is already present (ASCII '!' or fullwidth '！', B3 — left alone), or
/// the first token is not an exact registered name. **Never fuzzy** — an
/// edit-distance near-miss here would "correct" a deliberate raw protocol line
/// into an internal command and desync the engine.
inline std::optional<std::string> missingBangFix(const std::string              &entryText,
                                                 const std::vector<std::string> &names)
{
    std::size_t s = 0;
    while (s < entryText.size() && std::isspace(static_cast<unsigned char>(entryText[s])))
        ++s;
    if (s >= entryText.size())
        return std::nullopt;

    std::size_t bangLen = 0;
    if (startsWithBang(entryText.substr(s), bangLen))
        return std::nullopt;  // B3: bang already present — not this function's job

    std::size_t e = s;
    while (e < entryText.size() && !std::isspace(static_cast<unsigned char>(entryText[e])))
        ++e;

    const std::string lo = toLower(entryText.substr(s, e - s));
    for (const auto &n : names) {
        if (toLower(n) == lo)
            return entryText.substr(0, s) + "!" + n + entryText.substr(e);
    }
    return std::nullopt;
}

/// Pre-dispatch AutoCorrect composition (instruction step 3): run
/// `missingBangFix` first, then `normalizeCase` on the first command token.
/// Returns the corrected line, or nullopt when nothing changed. At most one
/// visible rewrite results — `missingBangFix` already emits the registered
/// spelling, so the following `normalizeCase` no-ops in that path.
inline std::optional<std::string> autoCorrect(const std::string              &entryText,
                                              const std::vector<std::string> &names)
{
    std::string line    = entryText;
    bool        changed = false;

    if (auto bang = missingBangFix(line, names)) {
        line    = *bang;
        changed = true;
    }

    auto ft = detail::firstToken(line);
    if (ft.ok) {
        const std::string token = line.substr(ft.nameStart, ft.end - ft.nameStart);
        if (auto norm = normalizeCase(token, names)) {
            line    = line.substr(0, ft.nameStart) + *norm + line.substr(ft.end);
            changed = true;
        }
    }

    if (!changed)
        return std::nullopt;
    return line;
}

}  // namespace command_completer
