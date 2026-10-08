// PAL-01 — palette search engine: unit tests + golden-set gate.
//
// GTK-free (glib only). The gate runs every query of the shared dataset
// (tests/data/palette/queries.tsv) and fails if recall@3 drops below 0.9 or a
// query takes > 5 ms; it checks Model A only (the Whoosh comparison is a
// dev-time benchmark, scripts/palette_bench.py).

#include "vendor/doctest.h"

#include "command/palette_search.h"

#include <chrono>
#include <fstream>
#include <map>
#include <sstream>

using namespace palette_search;

namespace {
std::string slurp(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    REQUIRE_MESSAGE(f.good(), "cannot open " << path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct Fixture {
    std::vector<Entry> entries = parseEntriesTsv(slurp(std::string(PALETTE_DATA_DIR) + "/entries.tsv"));
    Lexicon            lexicon = Lexicon::parse(slurp(PALETTE_LEXICON_PATH));
    Index              index{entries, lexicon};

    std::vector<std::string> ids(const std::string &q, std::size_t limit = 8) const
    {
        std::vector<std::string> out;
        for (const auto &h : index.search(q, limit))
            out.push_back(index.entry(h.index).id);
        return out;
    }
};
}  // namespace

TEST_CASE("PAL-01 foldText: NFC/NFD forms and d-stroke fold to the same ASCII")
{
    CHECK(foldText("Cài đặt") == "cai dat");
    CHECK(foldText("C\xC3\xA0i \xC4\x91\xE1\xBA\xB7t") == "cai dat");   // precomposed
    CHECK(foldText("Ca\xCC\x80i \xC4\x91" "a\xCC\xA3" "t") == "cai dat");   // decomposed input
    CHECK(foldText("ĐỔI") == "doi");
    CHECK(foldText("Undo") == "undo");
}

TEST_CASE("PAL-01 Vietnamese matches with and without diacritics")
{
    Fixture f;
    REQUIRE_FALSE(f.ids("cài đặt").empty());
    CHECK(f.ids("cài đặt").front() == "act.settings");
    CHECK(f.ids("cai dat").front() == "act.settings");
}

TEST_CASE("PAL-01 synonyms, typos and prefixes")
{
    Fixture f;
    CHECK(f.ids("preferences").front() == "act.settings");   // synonym
    CHECK(f.ids("setings").front() == "act.settings");       // typo
    CHECK(f.ids("coord").front() == "set.coordinates");      // prefix
    CHECK(f.ids("hoàn tác").front() == "act.nav-undo");      // vi synonym of undo
}

TEST_CASE("PAL-01 leading '!' restricts to console commands")
{
    Fixture f;
    for (const auto &q : {"!", "!un", "! undo"})
        for (const auto &id : f.ids(q))
            CHECK(id.rfind("cmd.", 0) == 0);
    CHECK(f.ids("!undo").front() == "cmd.undo");
    CHECK(f.ids("!").size() == 8);   // limit applies to the full command list
}

TEST_CASE("PAL-01 nonsense queries return nothing")
{
    Fixture f;
    for (const auto &q : {"pizza", "asdfghjk", "xyzzy", ""})
        CHECK(f.ids(q).empty());
}

TEST_CASE("PAL-01 disabled entries are demoted, not hidden")
{
    Fixture f;
    auto entries = f.entries;
    for (auto &e : entries)
        if (e.id == "act.save-game")
            e.enabled = false;
    Index idx(entries, f.lexicon);
    const auto hits = idx.search("save");
    REQUIRE_FALSE(hits.empty());
    bool found = false;
    for (const auto &h : hits)
        found = found || idx.entry(h.index).id == "act.save-game";
    CHECK(found);
}

TEST_CASE("PAL-01 golden set: recall@3 >= 0.9 and < 5 ms per query")
{
    Fixture f;
    std::istringstream in(slurp(std::string(PALETTE_DATA_DIR) + "/queries.tsv"));
    std::string        line;
    std::getline(in, line);
    int  total = 0, ok3 = 0, negOk = 0, neg = 0;
    long maxUs = 0;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::vector<std::string> c;
        std::stringstream        ls(line);
        for (std::string x; std::getline(ls, x, '\t');)
            c.push_back(x);
        if (c.size() < 2)
            continue;
        const std::string rel = c.size() > 2 ? c[2] : "";
        const auto a = std::chrono::steady_clock::now();
        const auto ids = f.ids(c[1]);
        maxUs = std::max<long>(maxUs, std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::steady_clock::now() - a).count());
        ++total;
        if (rel.empty()) {
            ++neg;
            negOk += ids.empty();
            ok3 += ids.empty();
            continue;
        }
        for (std::size_t i = 0; i < ids.size() && i < 3; ++i)
            if (rel.find(ids[i]) != std::string::npos) {
                ++ok3;
                break;
            }
    }
    REQUIRE(total >= 150);
    CHECK(static_cast<double>(ok3) / total >= 0.9);
    CHECK(negOk == neg);
    CHECK(maxUs < 5000);
}
