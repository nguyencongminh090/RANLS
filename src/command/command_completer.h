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
#include <string>
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

}  // namespace command_completer
