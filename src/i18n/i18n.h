#pragma once

// I18N-01: GTK-free UI-language catalog. English source text is the key
// (`tr("Open...")`); a language is a TSV catalog `key<TAB>translation`. A key
// that is unknown, or whose translation is empty, falls back to the English
// key itself. Depends on the standard library only (no gtk, no engine/), so it
// is unit-tested without a display server. Design: features/ui-language/.

#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace i18n {

/// Parsed `key<TAB>translation` catalog.
///
/// Lines: blank and `#`-comment lines are skipped; a line without a TAB is
/// ignored. In both fields `\n`, `\t` and `\\` are escapes. A context key is
/// written `ctx|text` (see trc()). A trailing `\r` is stripped (CRLF files).
class Catalog {
public:
    /// Parse catalog text. Later duplicate keys replace earlier ones.
    static Catalog parse(std::string_view text);

    /// Translation for `key`, or nullptr if absent or empty (= untranslated).
    const std::string *find(std::string_view key) const;

    /// All entries, including untranslated (empty) ones, for the lint.
    const std::map<std::string, std::string, std::less<>> &entries() const { return entries_; }
    bool empty() const { return entries_.empty(); }

private:
    std::map<std::string, std::string, std::less<>> entries_;
};

/// Replace `\n`, `\t`, `\\` escapes (unknown escapes keep the character).
std::string unescape(std::string_view s);

// ── Language registry ───────────────────────────────────────────────────────

struct Language {
    std::string code;        ///< "en", "vi"
    std::string nativeName;  ///< "English", "Tiếng Việt"
};

/// Supported languages. "en" is the identity (no catalog).
const std::vector<Language> &languages();
bool isSupported(std::string_view code);

/// Pick a language from locale names in preference order (as returned by
/// g_get_language_names(): "vi_VN.UTF-8", "vi_VN", "vi", "C"). The first
/// supported language wins; otherwise "en". Pure.
std::string resolveSystem(const std::vector<std::string> &localeNames);

/// I18N-03: the persisted `language=` setting is "system" or a supported code.
inline constexpr const char *kSystemLanguage = "system";

/// Map any stored value to a valid setting: a supported code is kept, "system"
/// and everything else (unknown, empty, wrong case) becomes "system". Pure.
std::string normalizeLanguageSetting(std::string_view setting);

/// Resolve a language setting to a concrete supported code: "system" (or an
/// invalid value) -> resolveSystem(localeNames); a supported code -> itself. Pure.
std::string resolveSetting(std::string_view setting, const std::vector<std::string> &localeNames);

/// Supplies a language's catalog text (e.g. from the GResource). Tests pass
/// strings instead. Returning an empty string means "no catalog".
using CatalogLoader = std::function<std::string(std::string_view code)>;
void setCatalogLoader(CatalogLoader loader);

/// Select the active language, loading its catalog via the loader. Returns
/// false (language unchanged) for an unsupported code. "en" clears the catalog.
/// On success every registered language-changed listener runs (I18N-02).
bool setLanguage(std::string_view code);
std::string currentLanguage();

/// Install an already-parsed catalog as the active one (tests).
void setActiveCatalog(Catalog catalog);

/// Translate `key` (the English text). Falls back to `key`.
std::string tr(std::string_view key);
/// Translate with disambiguating context: looks up `ctx|text`, then falls back
/// to `text` (not the combined key).
std::string trc(std::string_view ctx, std::string_view text);

// ── Language-changed notification (I18N-02) ─────────────────────────────────

/// Called (on the thread that called setLanguage, after the catalog switched)
/// so long-lived UI re-reads its texts. Returns an id for removal.
using ListenerId = unsigned;
ListenerId addLanguageListener(std::function<void()> listener);
void removeLanguageListener(ListenerId id);

// ── printf-style templates ──────────────────────────────────────────────────

namespace detail {
inline const char *fmtArg(const char *s) { return s; }
inline const char *fmtArg(const std::string &s) { return s.c_str(); }
inline const char *fmtArg(std::string_view) = delete;  // not NUL-terminated
template <class T>
inline T fmtArg(T v)
{
    return v;
}
}  // namespace detail

/// Substitute printf conversions (%s %d ...) of an already-translated
/// template; std::string arguments are accepted. Pass tr("...%s...") as `fmt`
/// so the lint sees one key per template. Arguments must be used in order.
template <class... Args>
std::string format(const std::string &fmt, const Args &...args)
{
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    int n = std::snprintf(nullptr, 0, fmt.c_str(), detail::fmtArg(args)...);
    if (n < 0)
        return fmt;
    std::string out(static_cast<size_t>(n) + 1, '\0');
    std::snprintf(out.data(), out.size(), fmt.c_str(), detail::fmtArg(args)...);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    out.resize(static_cast<size_t>(n));
    return out;
}

}  // namespace i18n
