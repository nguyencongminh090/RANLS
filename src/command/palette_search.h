#pragma once

// PAL-01: pure, GTK-free search engine behind the Ctrl+K command palette.
// Classical NLP only (features/command-palette/planning.md): Unicode NFC +
// casefold, Vietnamese diacritic folding, lexicon phrase max-matching, stopwords,
// light English stemming, synonym / fuzzy / prefix query expansion, and BM25F
// ranking over title / keywords / group fields. Depends on the standard library
// and GLib (g_utf8_normalize) only, so it is unit-tested without a display
// server (tests/test_pal01_palette_search.cpp) and benchmarked against a Whoosh
// baseline over the shared dataset in tests/data/palette/.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace palette_search {

/// One searchable thing: a window action, a setting or a `!` console command.
struct Entry {
    std::string id;
    std::string kind;        ///< "act" | "set" | "cmd" (`!` filter keeps "cmd").
    std::string titleEn;
    std::string titleVi;
    std::string keywordsEn;  ///< ';'-separated.
    std::string keywordsVi;  ///< ';'-separated.
    std::string group;
    bool        enabled = true;  ///< Disabled entries are demoted, never hidden.
};

/// Hand-authored bilingual vocabulary (src/resources/palette_lexicon.tsv).
/// Lines: `syn<TAB>a;b;c` (synonym group), `stop<TAB>a;b` (stopwords),
/// `phrase<TAB>two words` (kept as one token); `#` starts a comment. Members
/// containing spaces in a `syn` line are also registered as phrases.
struct Lexicon {
    std::vector<std::vector<std::string>> synonyms;
    std::vector<std::string>              stopwords;
    std::vector<std::string>              phrases;

    static Lexicon parse(const std::string &tsv);
};

/// Ablation switches, so the benchmark can show what each stage contributes.
struct Options {
    bool synonyms = true;
    bool fuzzy    = true;
    bool prefix   = true;
    bool trigram  = true;
    bool stem     = true;
    bool fold     = true;  ///< Match on diacritic-folded text (false = exact only).
};

struct Hit {
    std::size_t index;  ///< Position in the entry vector given to Index.
    double      score;
};

/// NFD, strip combining marks, map d-stroke to d, lowercase. "Cài đặt" -> "cai dat".
std::string foldText(const std::string &utf8);

/// Parse the dataset TSVs (header line first). Throws std::runtime_error on a short row.
std::vector<Entry> parseEntriesTsv(const std::string &tsv);

class Index {
public:
    Index(std::vector<Entry> entries, const Lexicon &lexicon, Options opts = {});

    /// Ranked hits, best first, at most `limit`. A leading '!' restricts to
    /// kind "cmd" ("!" alone lists every command). Empty when nothing matches.
    std::vector<Hit> search(const std::string &query, std::size_t limit = 8) const;

    const Entry &entry(std::size_t i) const { return entries_[i]; }
    std::size_t  size() const { return entries_.size(); }

private:
    struct Term {            // one query term with its weight
        std::string text;
        double      weight;
    };
    struct Posting {
        std::uint32_t doc;
        float         tf[3];  // title, keywords, group
    };

    std::vector<std::string> analyze(const std::string &folded, bool isQuery,
                                     std::vector<Term> *queryTerms) const;
    std::string stem(const std::string &t) const;
    void        addDoc(std::size_t doc);

    Options                                        opts_;
    std::vector<Entry>                             entries_;
    std::unordered_set<std::string>                stop_;
    std::unordered_set<std::string>                phraseSet_;
    std::size_t                                    maxPhraseLen_ = 1;
    std::unordered_map<std::string, std::vector<std::size_t>> synGroupsOf_;
    std::vector<std::vector<std::string>>          synGroups_;  // stemmed members
    std::unordered_map<std::string, std::vector<Posting>> inv_;
    std::vector<std::vector<std::string>>          exactTokens_;  // per doc, NFC lowercase
    std::vector<std::vector<std::string>>          foldedTitles_; // per doc title variants
    std::unordered_map<std::string, std::string>   rawToStem_;    // unstemmed vocab -> index term
    std::vector<std::array<float, 3>>              fieldLen_;
    float                                          avgLen_[3] = {1, 1, 1};
};

}  // namespace palette_search
