#include "command/palette_search.h"

#include <glib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <set>
#include <sstream>
#include <stdexcept>

namespace palette_search {

namespace {

constexpr double kK1        = 1.2;
constexpr double kB[3]      = {0.75, 0.75, 0.75};
constexpr double kBoost[3]  = {3.0, 2.0, 1.0};  // title, keywords, group (BM25F field weights)
constexpr double kSynWeight = 0.7;
constexpr double kSylWeight = 0.5;  // a syllable of a merged phrase
constexpr double kPrefixWeight = 0.6;
constexpr double kExactTitleBoost = 1.5;
constexpr double kDiacriticBonus  = 0.1;  // per exactly-matching accented query token
constexpr double kCommandPenalty  = 0.9;  // `!` commands rank below actions/settings unless '!' is typed
constexpr double kMinScore        = 0.15;

std::string gstr(gchar *p)
{
    std::string s = p ? p : "";
    g_free(p);
    return s;
}

std::string nfcLower(const std::string &s)
{
    return gstr(g_utf8_strdown(gstr(g_utf8_normalize(s.c_str(), -1, G_NORMALIZE_NFC)).c_str(), -1));
}

/// Split on anything that is not a Unicode letter/digit. Input already normalised.
std::vector<std::string> tokenize(const std::string &s)
{
    std::vector<std::string> out;
    std::string              cur;
    for (const char *p = s.c_str(); *p;) {
        gunichar c = g_utf8_get_char(p);
        const char *next = g_utf8_next_char(p);
        if (g_unichar_isalnum(c)) {
            cur.append(p, next);
        } else if (!cur.empty()) {
            out.push_back(cur);
            cur.clear();
        }
        p = next;
    }
    if (!cur.empty())
        out.push_back(cur);
    return out;
}

std::vector<std::string> splitChar(const std::string &s, char sep)
{
    std::vector<std::string> out;
    std::string              cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

std::string trim(const std::string &s)
{
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

/// Optimal-string-alignment (Damerau) distance, early-exit above `limit`.
std::size_t osa(const std::string &a, const std::string &b, std::size_t limit)
{
    const std::size_t n = a.size(), m = b.size();
    if ((n > m ? n - m : m - n) > limit)
        return limit + 1;
    std::vector<std::vector<std::size_t>> d(n + 1, std::vector<std::size_t>(m + 1));
    for (std::size_t i = 0; i <= n; ++i) d[i][0] = i;
    for (std::size_t j = 0; j <= m; ++j) d[0][j] = j;
    for (std::size_t i = 1; i <= n; ++i)
        for (std::size_t j = 1; j <= m; ++j) {
            const std::size_t cost = a[i - 1] == b[j - 1] ? 0 : 1;
            d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost});
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
                d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
        }
    return d[n][m];
}

std::set<std::string> trigrams(const std::string &s)
{
    std::set<std::string> g;
    for (std::size_t i = 0; i + 3 <= s.size(); ++i)
        g.insert(s.substr(i, 3));
    return g;
}

bool hasNonAscii(const std::string &s)
{
    return std::any_of(s.begin(), s.end(), [](unsigned char c) { return c >= 0x80; });
}

}  // namespace

std::string foldText(const std::string &utf8)
{
    const std::string nfd = gstr(g_utf8_normalize(utf8.c_str(), -1, G_NORMALIZE_NFD));
    std::string       out;
    for (const char *p = nfd.c_str(); *p;) {
        gunichar    c    = g_utf8_get_char(p);
        const char *next = g_utf8_next_char(p);
        if (c >= 0x0300 && c <= 0x036F) {
            // combining mark: drop
        } else if (c == 0x0111 || c == 0x0110) {
            out += 'd';
        } else {
            out.append(p, next);
        }
        p = next;
    }
    return gstr(g_utf8_strdown(out.c_str(), -1));
}

Lexicon Lexicon::parse(const std::string &tsv)
{
    Lexicon          lx;
    std::istringstream in(tsv);
    std::string        line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        const auto tab = line.find('\t');
        if (tab == std::string::npos)
            continue;
        const std::string kind = line.substr(0, tab);
        std::vector<std::string> items;
        for (auto &x : splitChar(line.substr(tab + 1), ';')) {
            x = trim(x);
            if (!x.empty())
                items.push_back(x);
        }
        if (kind == "syn")
            lx.synonyms.push_back(items);
        else if (kind == "stop")
            lx.stopwords.insert(lx.stopwords.end(), items.begin(), items.end());
        else if (kind == "phrase")
            lx.phrases.insert(lx.phrases.end(), items.begin(), items.end());
    }
    return lx;
}

std::vector<Entry> parseEntriesTsv(const std::string &tsv)
{
    std::vector<Entry> out;
    std::istringstream in(tsv);
    std::string        line;
    bool               header = true;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (header) {
            header = false;
            continue;
        }
        if (line.empty())
            continue;
        auto f = splitChar(line, '\t');
        if (f.size() < 7)
            throw std::runtime_error("entries.tsv: expected 7 columns: " + line);
        out.push_back({f[0], f[1], f[2], f[3], f[4], f[5], f[6], true});
    }
    return out;
}

std::string Index::stem(const std::string &t) const
{
    if (!opts_.stem || t.size() < 4 || hasNonAscii(t) || t.find('_') != std::string::npos)
        return t;
    auto ends = [&](const char *suf) {
        const std::size_t n = std::char_traits<char>::length(suf);
        return t.size() >= n && t.compare(t.size() - n, n, suf) == 0;
    };
    std::string s = t;
    if (ends("ies") && s.size() > 4)
        s = s.substr(0, s.size() - 3) + "y";
    else if (ends("sses"))
        s = s.substr(0, s.size() - 2);
    else if (ends("es") && s.size() > 4 && (ends("xes") || ends("zes") || ends("ches") || ends("shes")))
        s = s.substr(0, s.size() - 2);
    else if (ends("s") && !ends("ss") && !ends("us") && !ends("is") && s.size() > 3)
        s = s.substr(0, s.size() - 1);
    auto sEnds = [&](const char *suf) {
        const std::size_t n = std::char_traits<char>::length(suf);
        return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
    };
    if (sEnds("ing") && s.size() > 5) {
        s.resize(s.size() - 3);
        if (s.size() > 2 && s[s.size() - 1] == s[s.size() - 2] && !std::strchr("lsz", s.back()))
            s.pop_back();
    } else if (sEnds("ed") && s.size() > 4) {
        s.resize(s.size() - 2);
    }
    return s;
}

/// Tokenise one text into index/query terms. For queries, `queryTerms` also
/// receives the per-term weights (phrase 1.0, syllable of a phrase 0.5).
std::vector<std::string> Index::analyze(const std::string &norm, bool isQuery,
                                        std::vector<Term> *queryTerms) const
{
    const auto toks = tokenize(norm);
    std::vector<std::string> out;
    auto emit = [&](const std::string &t, double w) {
        out.push_back(t);
        if (queryTerms)
            queryTerms->push_back({t, w});
    };
    std::size_t i = 0;
    std::vector<std::string> kept;  // (for the all-stopwords fallback)
    while (i < toks.size()) {
        bool merged = false;
        for (std::size_t len = std::min(maxPhraseLen_, toks.size() - i); len >= 2; --len) {
            std::string joined = toks[i];
            for (std::size_t k = 1; k < len; ++k)
                joined += "_" + toks[i + k];
            if (phraseSet_.count(joined)) {
                emit(joined, 1.0);
                for (std::size_t k = 0; k < len; ++k)
                    emit(stem(toks[i + k]), kSylWeight);
                i += len;
                merged = true;
                break;
            }
        }
        if (merged)
            continue;
        if (stop_.count(toks[i]) && !(isQuery && toks.size() == 1))
            ;  // stopword: dropped
        else
            emit(stem(toks[i]), 1.0);
        ++i;
    }
    return out;
}

Index::Index(std::vector<Entry> entries, const Lexicon &lexicon, Options opts)
    : opts_(opts), entries_(std::move(entries))
{
    auto norm = [&](const std::string &s) { return opts_.fold ? foldText(s) : nfcLower(s); };
    for (const auto &w : lexicon.stopwords)
        stop_.insert(norm(w));
    auto addPhrase = [&](const std::string &p) {
        auto t = tokenize(norm(p));
        if (t.size() < 2)
            return;
        std::string j = t[0];
        for (std::size_t k = 1; k < t.size(); ++k)
            j += "_" + t[k];
        phraseSet_.insert(j);
        maxPhraseLen_ = std::max(maxPhraseLen_, t.size());
    };
    for (const auto &p : lexicon.phrases)
        addPhrase(p);
    for (const auto &g : lexicon.synonyms)
        for (const auto &m : g)
            addPhrase(m);
    // Synonym groups hold the analysed (phrase-merged, stemmed) members.
    for (const auto &g : lexicon.synonyms) {
        std::vector<std::string> members;
        for (const auto &m : g) {
            auto toks = tokenize(norm(m));
            if (toks.empty())
                continue;
            if (toks.size() == 1) {
                members.push_back(stem(toks[0]));
            } else {
                std::string j = toks[0];
                for (std::size_t k = 1; k < toks.size(); ++k)
                    j += "_" + toks[k];
                members.push_back(j);
            }
        }
        synGroups_.push_back(members);
        for (const auto &m : members)
            synGroupsOf_[m].push_back(synGroups_.size() - 1);
    }
    for (std::size_t d = 0; d < entries_.size(); ++d)
        addDoc(d);
    double sum[3] = {0, 0, 0};
    for (const auto &l : fieldLen_)
        for (int f = 0; f < 3; ++f)
            sum[f] += l[f];
    for (int f = 0; f < 3; ++f)
        avgLen_[f] = entries_.empty() ? 1.f : std::max(1.f, static_cast<float>(sum[f] / entries_.size()));
}

void Index::addDoc(std::size_t doc)
{
    const Entry &e = entries_[doc];
    auto norm = [&](const std::string &s) { return opts_.fold ? foldText(s) : nfcLower(s); };
    const std::string fields[3] = {e.titleEn + " " + e.titleVi, e.keywordsEn + ";" + e.keywordsVi, e.group};
    std::array<float, 3> len{0, 0, 0};
    std::unordered_map<std::string, std::array<float, 3>> tf;
    for (int f = 0; f < 3; ++f) {
        const std::string n = norm(fields[f]);
        for (const auto &t : analyze(n, false, nullptr)) {
            tf[t][f] += 1;
            len[f] += 1;
        }
        if (f < 2)
            for (const auto &raw : tokenize(n))
                rawToStem_[raw] = stem(raw);
    }
    fieldLen_.push_back(len);
    for (const auto &[term, c] : tf)
        inv_[term].push_back({static_cast<std::uint32_t>(doc), {c[0], c[1], c[2]}});

    std::vector<std::string> exact;
    for (const auto &t : tokenize(nfcLower(fields[0] + " " + fields[1])))
        exact.push_back(t);
    exactTokens_.push_back(exact);

    // Title variants for the exact-title boost (split at the em dash of command titles).
    std::vector<std::string> variants;
    for (const std::string &title : {e.titleEn, e.titleVi}) {
        std::string t = title;
        std::vector<std::string> parts;
        for (std::size_t p; (p = t.find("\xE2\x80\x94")) != std::string::npos;) {
            parts.push_back(t.substr(0, p));
            t = t.substr(p + 3);
        }
        parts.push_back(t);
        parts.push_back(title);
        for (const auto &part : parts) {
            std::string j;
            for (const auto &tok : tokenize(norm(part)))
                j += (j.empty() ? "" : " ") + tok;
            if (!j.empty())
                variants.push_back(j);
        }
    }
    foldedTitles_.push_back(variants);
}

std::vector<Hit> Index::search(const std::string &queryIn, std::size_t limit) const
{
    std::string query = trim(queryIn);
    bool        bang  = false;
    if (!query.empty() && query[0] == '!') {
        bang  = true;
        query = trim(query.substr(1));
    }
    auto norm = [&](const std::string &s) { return opts_.fold ? foldText(s) : nfcLower(s); };
    std::vector<Hit> hits;
    if (bang && query.empty()) {
        for (std::size_t d = 0; d < entries_.size() && hits.size() < limit; ++d)
            if (entries_[d].kind == "cmd")
                hits.push_back({d, 1.0});
        return hits;
    }

    const std::string nq = norm(query);
    std::vector<Term> base;
    analyze(nq, true, &base);
    const auto rawToks = tokenize(nq);
    const bool endsWithSpace = !queryIn.empty() && std::isspace(static_cast<unsigned char>(queryIn.back()));

    std::unordered_map<std::string, double> terms;  // final weighted query terms
    auto put = [&](const std::string &t, double w) {
        auto &cur = terms[t];
        cur = std::max(cur, w);
    };
    for (const auto &b : base)
        put(b.text, b.weight);

    if (opts_.synonyms)
        for (const auto &b : base) {
            auto it = synGroupsOf_.find(b.text);
            if (it == synGroupsOf_.end())
                continue;
            for (std::size_t g : it->second)
                for (const auto &m : synGroups_[g])
                    if (m != b.text)
                        put(m, kSynWeight * b.weight);
        }

    if (opts_.fuzzy)
        for (const auto &raw : rawToks) {
            if (raw.size() < 4 || rawToStem_.count(raw))
                continue;
            const std::size_t lim = raw.size() >= 8 ? 2 : 1;
            for (const auto &[cand, stemmed] : rawToStem_) {
                const std::size_t d = osa(raw, cand, lim);
                if (d >= 1 && d <= lim)
                    put(stemmed, d == 1 ? 0.5 : 0.35);
            }
        }

    if (opts_.prefix && !rawToks.empty() && !endsWithSpace) {
        const std::string &last = rawToks.back();
        if (last.size() >= 2)
            for (const auto &[cand, stemmed] : rawToStem_)
                if (cand.size() > last.size() && cand.compare(0, last.size(), last) == 0)
                    put(stemmed, kPrefixWeight);
    }

    std::vector<double> score(entries_.size(), 0.0);
    const double N = static_cast<double>(entries_.size());
    for (const auto &[term, qw] : terms) {
        auto it = inv_.find(term);
        if (it == inv_.end())
            continue;
        const double df  = static_cast<double>(it->second.size());
        const double idf = std::log(1.0 + (N - df + 0.5) / (df + 0.5));
        for (const auto &p : it->second) {
            double tfp = 0;
            for (int f = 0; f < 3; ++f)
                if (p.tf[f] > 0)
                    tfp += kBoost[f] * p.tf[f] / (1.0 - kB[f] + kB[f] * fieldLen_[p.doc][f] / avgLen_[f]);
            score[p.doc] += qw * idf * tfp / (kK1 + tfp);
        }
    }

    // Boosts.
    std::string joined;
    for (const auto &t : rawToks)
        joined += (joined.empty() ? "" : " ") + t;
    std::vector<std::string> accented;
    for (const auto &t : tokenize(nfcLower(query)))
        if (hasNonAscii(t))
            accented.push_back(t);
    for (std::size_t d = 0; d < entries_.size(); ++d) {
        if (score[d] <= 0)
            continue;
        if (!joined.empty() && std::find(foldedTitles_[d].begin(), foldedTitles_[d].end(), joined) != foldedTitles_[d].end())
            score[d] *= kExactTitleBoost;
        int hit = 0;
        for (const auto &a : accented)
            if (std::find(exactTokens_[d].begin(), exactTokens_[d].end(), a) != exactTokens_[d].end())
                ++hit;
        score[d] *= 1.0 + kDiacriticBonus * hit;
        if (!bang && entries_[d].kind == "cmd")
            score[d] *= kCommandPenalty;
        if (!entries_[d].enabled)
            score[d] *= 0.5;
    }

    for (std::size_t d = 0; d < entries_.size(); ++d) {
        if (bang && entries_[d].kind != "cmd")
            continue;
        if (score[d] >= kMinScore)
            hits.push_back({d, score[d]});
    }

    if (hits.empty() && opts_.trigram && joined.size() >= 3) {
        std::string compact;
        for (char c : joined)
            if (c != ' ')
                compact += c;
        const auto qg = trigrams(compact);
        for (std::size_t d = 0; d < entries_.size(); ++d) {
            if (bang && entries_[d].kind != "cmd")
                continue;
            double best = 0;
            for (const auto &v : foldedTitles_[d]) {
                std::string c2;
                for (char c : v)
                    if (c != ' ')
                        c2 += c;
                const auto tg = trigrams(c2);
                std::size_t inter = 0;
                for (const auto &g : qg)
                    inter += tg.count(g);
                const double uni = static_cast<double>(qg.size() + tg.size() - inter);
                if (uni > 0)
                    best = std::max(best, inter / uni);
            }
            if (best >= 0.4)
                hits.push_back({d, 0.3 * best});
        }
    }

    std::stable_sort(hits.begin(), hits.end(), [](const Hit &a, const Hit &b) { return a.score > b.score; });
    if (hits.size() > limit)
        hits.resize(limit);
    return hits;
}

}  // namespace palette_search
