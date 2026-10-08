#include "i18n/i18n_lint.h"

#include <algorithm>
#include <cctype>

namespace i18n::lint {

namespace {
bool isIdent(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool isAlnum(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

void skipWs(std::string_view s, size_t &i)
{
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
        ++i;
}

// Parse one or more adjacent C string literals at s[i]; false if none.
bool parseLiterals(std::string_view s, size_t &i, std::string &out)
{
    out.clear();
    bool any = false;
    for (;;) {
        skipWs(s, i);
        if (i >= s.size() || s[i] != '"')
            return any;
        ++i;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char c = s[++i];
                out += c == 'n' ? '\n' : c == 't' ? '\t' : c;
            } else {
                out += s[i];
            }
            ++i;
        }
        if (i >= s.size())
            return false;
        ++i;  // closing quote
        any = true;
    }
}

// Whole-word match of `term` in `text` (word edges only where the term itself
// starts/ends with an alphanumeric, so ".rdb" matches inside "file.rdb").
bool containsTerm(const std::string &text, const std::string &term)
{
    bool needL = isAlnum(term.front());
    bool needR = isAlnum(term.back());
    for (size_t p = text.find(term); p != std::string::npos; p = text.find(term, p + 1)) {
        bool okL = !needL || p == 0 || !isAlnum(text[p - 1]);
        size_t e = p + term.size();
        bool okR = !needR || e >= text.size() || !isAlnum(text[e]);
        if (okL && okR)
            return true;
    }
    return false;
}
}  // namespace

const std::vector<std::string> &doNotTranslate()
{
    static const std::vector<std::string> v = {"Freestyle Gomoku", "Standard Gomoku", "Free Renju", "Renju",
                                               "Gomoku",           "PV",              "Rapfi",      "Yixin",
                                               ".rdb"};
    return v;
}

std::set<std::string> extractKeys(std::string_view src)
{
    std::set<std::string> keys;
    for (size_t p = 0; p + 1 < src.size(); ++p) {
        if (src[p] != 't' || src[p + 1] != 'r' || (p > 0 && isIdent(src[p - 1])))
            continue;
        size_t i = p + 2;
        bool ctx = false;
        if (i < src.size() && src[i] == 'c') {
            ctx = true;
            ++i;
        }
        skipWs(src, i);
        if (i >= src.size() || src[i] != '(')
            continue;
        ++i;
        std::string a;
        if (!parseLiterals(src, i, a))
            continue;
        if (!ctx) {
            keys.insert(a);
            continue;
        }
        skipWs(src, i);
        if (i >= src.size() || src[i] != ',')
            continue;
        ++i;
        std::string b;
        if (parseLiterals(src, i, b))
            keys.insert(a + "|" + b);
    }
    return keys;
}

std::vector<std::string> placeholders(std::string_view s)
{
    std::vector<std::string> out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%') {
            if (i + 1 < s.size() && s[i + 1] == '%') {
                ++i;
                continue;
            }
            size_t j = i + 1;
            while (j < s.size() && std::string_view("0123456789$-+ #.").find(s[j]) != std::string_view::npos)
                ++j;
            while (j < s.size() && std::string_view("hlLzjtq").find(s[j]) != std::string_view::npos)
                ++j;
            if (j < s.size() && std::isalpha(static_cast<unsigned char>(s[j]))) {
                out.emplace_back(s.substr(i, j - i + 1));
                i = j;
            }
        } else if (s[i] == '{') {
            size_t j = i + 1;
            while (j < s.size() && isIdent(s[j]))
                ++j;
            if (j < s.size() && s[j] == '}') {
                out.emplace_back(s.substr(i, j - i + 1));
                i = j;
            }
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::string> check(const Catalog &catalog, const std::set<std::string> &ref)
{
    std::vector<std::string> problems;
    for (const auto &[key, translation] : catalog.entries()) {
        if (!ref.count(key)) {
            problems.push_back("orphan key (not in reference): " + key);
            continue;
        }
        if (translation.empty())
            continue;  // untranslated: falls back to English
        if (placeholders(key) != placeholders(translation))
            problems.push_back("placeholder mismatch: " + key);
        for (const auto &term : doNotTranslate())
            if (containsTerm(key, term) && !containsTerm(translation, term))
                problems.push_back("do-not-translate term '" + term + "' altered: " + key);
    }
    return problems;
}

std::vector<std::string> checkCoverage(const Catalog &catalog, const std::set<std::string> &ref)
{
    std::vector<std::string> problems;
    for (const auto &key : ref)
        if (!catalog.find(key))  // find() is null for absent and for empty
            problems.push_back("missing translation: " + key);
    return problems;
}

}  // namespace i18n::lint
