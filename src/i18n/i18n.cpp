#include "i18n/i18n.h"

#include <cctype>
#include <mutex>
#include <utility>

namespace i18n {

std::string unescape(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char c = s[++i];
            out += c == 'n' ? '\n' : c == 't' ? '\t' : c;
        } else {
            out += s[i];
        }
    }
    return out;
}

Catalog Catalog::parse(std::string_view text)
{
    Catalog cat;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string_view::npos)
            eol = text.size();
        std::string_view line = text.substr(pos, eol - pos);
        pos = eol + 1;
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if (line.empty() || line[0] == '#')
            continue;
        size_t tab = line.find('\t');
        if (tab == std::string_view::npos || tab == 0)
            continue;
        cat.entries_[unescape(line.substr(0, tab))] = unescape(line.substr(tab + 1));
    }
    return cat;
}

const std::string *Catalog::find(std::string_view key) const
{
    auto it = entries_.find(key);
    if (it == entries_.end() || it->second.empty())
        return nullptr;
    return &it->second;
}

// ── Registry ────────────────────────────────────────────────────────────────

namespace {
struct State {
    std::mutex mu;
    CatalogLoader loader;
    std::string language = "en";
    Catalog catalog;
    std::map<ListenerId, std::function<void()>> listeners;
    ListenerId nextId = 1;
    std::map<std::string, Catalog, std::less<>> others;  // I18N-04: lazily loaded non-active languages
};
State &state()
{
    static State s;
    return s;
}
}  // namespace

const std::vector<Language> &languages()
{
    static const std::vector<Language> v = {{"en", "English"}, {"vi", "Ti\xE1\xBA\xBFng Vi\xE1\xBB\x87t"}};
    return v;
}

bool isSupported(std::string_view code)
{
    for (const auto &l : languages())
        if (l.code == code)
            return true;
    return false;
}

std::string resolveSystem(const std::vector<std::string> &localeNames)
{
    for (const auto &name : localeNames) {
        size_t end = name.find_first_of("_-.@");
        std::string lang = name.substr(0, end);
        for (char &c : lang)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (isSupported(lang))
            return lang;
    }
    return "en";
}

std::string normalizeLanguageSetting(std::string_view setting)
{
    return isSupported(setting) ? std::string(setting) : std::string(kSystemLanguage);
}

std::string resolveSetting(std::string_view setting, const std::vector<std::string> &localeNames)
{
    const std::string s = normalizeLanguageSetting(setting);
    return s == kSystemLanguage ? resolveSystem(localeNames) : s;
}

void setCatalogLoader(CatalogLoader loader)
{
    auto &s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    s.loader = std::move(loader);
    s.others.clear();
}

bool setLanguage(std::string_view code)
{
    if (!isSupported(code))
        return false;
    auto &s = state();
    std::vector<std::function<void()>> toNotify;
    {
        std::lock_guard<std::mutex> lk(s.mu);
        s.language = std::string(code);
        s.catalog = Catalog();
        if (code != "en" && s.loader)
            s.catalog = Catalog::parse(s.loader(code));
        for (const auto &[id, fn] : s.listeners)
            toNotify.push_back(fn);
    }
    for (const auto &fn : toNotify)
        fn();  // outside the lock: listeners call tr()
    return true;
}

ListenerId addLanguageListener(std::function<void()> listener)
{
    auto &s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    const ListenerId id = s.nextId++;
    s.listeners[id] = std::move(listener);
    return id;
}

void removeLanguageListener(ListenerId id)
{
    auto &s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    s.listeners.erase(id);
}

std::string currentLanguage()
{
    auto &s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    return s.language;
}

void setActiveCatalog(Catalog catalog)
{
    auto &s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    s.catalog = std::move(catalog);
}

std::string tr(std::string_view key)
{
    auto &s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    if (const std::string *t = s.catalog.find(key))
        return *t;
    return std::string(key);
}

std::optional<std::string> lookupIn(std::string_view lang, std::string_view key)
{
    if (lang == "en" || !isSupported(lang))
        return std::nullopt;
    auto &s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    const Catalog *cat = nullptr;
    if (lang == s.language) {
        cat = &s.catalog;
    } else {
        auto it = s.others.find(lang);
        if (it == s.others.end()) {
            Catalog parsed;
            if (s.loader)
                parsed = Catalog::parse(s.loader(lang));
            it = s.others.emplace(std::string(lang), std::move(parsed)).first;
        }
        cat = &it->second;
    }
    if (const std::string *t = cat->find(key))
        return *t;
    return std::nullopt;
}

std::string trIn(std::string_view lang, std::string_view ctx, std::string_view text)
{
    std::string combined;
    combined.append(ctx).push_back('|');
    combined.append(text);
    if (auto t = lookupIn(lang, combined))
        return *t;
    if (auto t = lookupIn(lang, text))
        return *t;
    return std::string(text);
}

std::string trc(std::string_view ctx, std::string_view text)
{
    auto &s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    std::string combined;
    combined.reserve(ctx.size() + 1 + text.size());
    combined.append(ctx).push_back('|');
    combined.append(text);
    if (const std::string *t = s.catalog.find(combined))
        return *t;
    if (const std::string *t = s.catalog.find(text))
        return *t;
    return std::string(text);
}

}  // namespace i18n
