#pragma once

// I18N-01: GTK-free UI-language catalog. English source text is the key
// (`tr("Open...")`); a language is a TSV catalog `key<TAB>translation`. A key
// that is unknown, or whose translation is empty, falls back to the English
// key itself. Depends on the standard library only (no gtk, no engine/), so it
// is unit-tested without a display server. Design: features/ui-language/.

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

/// Supplies a language's catalog text (e.g. from the GResource). Tests pass
/// strings instead. Returning an empty string means "no catalog".
using CatalogLoader = std::function<std::string(std::string_view code)>;
void setCatalogLoader(CatalogLoader loader);

/// Select the active language, loading its catalog via the loader. Returns
/// false (language unchanged) for an unsupported code. "en" clears the catalog.
bool setLanguage(std::string_view code);
std::string currentLanguage();

/// Install an already-parsed catalog as the active one (tests).
void setActiveCatalog(Catalog catalog);

/// Translate `key` (the English text). Falls back to `key`.
std::string tr(std::string_view key);
/// Translate with disambiguating context: looks up `ctx|text`, then falls back
/// to `text` (not the combined key).
std::string trc(std::string_view ctx, std::string_view text);

}  // namespace i18n
