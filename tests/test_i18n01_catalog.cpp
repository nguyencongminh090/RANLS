// I18N-01 — pure checks (no GTK): TSV catalog parsing + escapes, English
// fallback, ctx keys, language registry / resolveSystem, and the catalog lint
// (orphan key, placeholder mismatch, altered do-not-translate term), including
// the real lint of the bundled vi.tsv against the tr() literals found in src/.

#include "vendor/doctest.h"

#include "i18n/i18n.h"
#include "i18n/i18n_lint.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
std::string slurp(const std::filesystem::path &path)
{
    std::ifstream f(path, std::ios::binary);
    REQUIRE_MESSAGE(f.good(), path.string());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Union of tr()/trc() keys over every C++ file below `dir`, skipping src/i18n
// (its own docs and API signatures mention tr("...")).
std::set<std::string> scanKeys(const std::filesystem::path &dir)
{
    std::set<std::string> keys;
    for (const auto &e : std::filesystem::recursive_directory_iterator(dir)) {
        if (!e.is_regular_file())
            continue;
        auto ext = e.path().extension();
        if (ext != ".cpp" && ext != ".h")
            continue;
        if (e.path().parent_path() == dir / "i18n")
            continue;
        for (const auto &k : i18n::lint::extractKeys(slurp(e.path())))
            keys.insert(k);
    }
    return keys;
}

bool anyContains(const std::vector<std::string> &v, const std::string &needle)
{
    for (const auto &s : v)
        if (s.find(needle) != std::string::npos)
            return true;
    return false;
}

// Restore the process-wide language so other tests are unaffected.
struct LanguageGuard {
    ~LanguageGuard()
    {
        i18n::setCatalogLoader(nullptr);
        i18n::setLanguage("en");
    }
};
}  // namespace

TEST_CASE("I18N-01 catalog parses key<TAB>translation with escapes")
{
    auto cat = i18n::Catalog::parse("# comment\n\nHello\tXin chao\nTwo\\nLines\tHai\\ndong\nTab\\tKey\ta\\tb\r\n"
                                    "no tab here\nBack\\\\slash\tx\\\\y\nlast\tcuoi");
    REQUIRE(cat.find("Hello"));
    CHECK(*cat.find("Hello") == "Xin chao");
    CHECK(*cat.find("Two\nLines") == "Hai\ndong");
    CHECK(*cat.find("Tab\tKey") == "a\tb");  // CRLF stripped
    CHECK(*cat.find("Back\\slash") == "x\\y");
    CHECK(*cat.find("last") == "cuoi");  // no trailing newline
    CHECK(cat.find("no tab here") == nullptr);
    CHECK(cat.find("# comment") == nullptr);
    CHECK(cat.entries().size() == 5);
}

TEST_CASE("I18N-01 tr falls back to the English key")
{
    LanguageGuard g;
    i18n::setActiveCatalog(i18n::Catalog::parse("Save\tLuu\nEmpty\t\n"));
    CHECK(i18n::tr("Save") == "Luu");
    CHECK(i18n::tr("Unknown key") == "Unknown key");  // unknown
    CHECK(i18n::tr("Empty") == "Empty");              // empty = untranslated, not blank
}

TEST_CASE("I18N-01 trc uses ctx|text, then plain text, then English")
{
    LanguageGuard g;
    i18n::setActiveCatalog(i18n::Catalog::parse("menu|Open\tMo\nOpen\tMo chung\ntoolbar|Stop\tDung\n"));
    CHECK(i18n::trc("menu", "Open") == "Mo");
    CHECK(i18n::trc("dialog", "Open") == "Mo chung");  // falls back to the context-free entry
    CHECK(i18n::trc("toolbar", "Stop") == "Dung");
    CHECK(i18n::trc("toolbar", "Play") == "Play");
}

TEST_CASE("I18N-01 language registry: setLanguage loads via the loader")
{
    LanguageGuard g;
    CHECK(i18n::currentLanguage() == "en");
    CHECK(i18n::isSupported("vi"));
    CHECK_FALSE(i18n::isSupported("fr"));
    CHECK(i18n::languages().size() == 2);

    int loads = 0;
    i18n::setCatalogLoader([&](std::string_view code) {
        ++loads;
        return code == "vi" ? std::string("Save\tLuu\n") : std::string();
    });
    CHECK_FALSE(i18n::setLanguage("fr"));  // unsupported: unchanged
    CHECK(i18n::currentLanguage() == "en");
    CHECK(i18n::setLanguage("vi"));
    CHECK(i18n::currentLanguage() == "vi");
    CHECK(i18n::tr("Save") == "Luu");
    CHECK(i18n::setLanguage("en"));  // identity: catalog cleared, loader not asked
    CHECK(i18n::tr("Save") == "Save");
    CHECK(loads == 1);
}

TEST_CASE("I18N-01 resolveSystem picks the first supported locale")
{
    using V = std::vector<std::string>;
    CHECK(i18n::resolveSystem(V{"vi_VN.UTF-8", "vi_VN", "vi", "C"}) == "vi");
    CHECK(i18n::resolveSystem(V{"fr_FR.UTF-8", "fr_FR", "fr", "C"}) == "en");
    CHECK(i18n::resolveSystem(V{"fr_FR", "vi"}) == "vi");  // first *supported* entry
    CHECK(i18n::resolveSystem(V{"en_US.UTF-8"}) == "en");
    CHECK(i18n::resolveSystem(V{"VI-vn"}) == "vi");
    CHECK(i18n::resolveSystem(V{"C"}) == "en");
    CHECK(i18n::resolveSystem(V{}) == "en");
}

TEST_CASE("I18N-01 lint: extractKeys finds tr/trc literals only")
{
    auto keys = i18n::lint::extractKeys(
        "a = i18n::tr(\"Open...\"); b = tr( \"Two\\nLines\" ); c = trc(\"menu\", \"Open\");\n"
        "d = tr(\"Split \" \"literal\"); s = str(\"no\"); e = ctr(\"no\"); f = tr(variable);\n");
    CHECK(keys == std::set<std::string>{"Open...", "Two\nLines", "menu|Open", "Split literal"});
}

TEST_CASE("I18N-01 lint: placeholders compare as multisets")
{
    using i18n::lint::placeholders;
    CHECK(placeholders("%s of %d, 100%% {n} %1$s") == placeholders("%d then {n}, %s %1$s, 5%%"));
    CHECK(placeholders("%s") != placeholders("%d"));
    CHECK(placeholders("{n}").size() == 1);
    CHECK(placeholders("plain text").empty());
}

TEST_CASE("I18N-01 lint: clean catalog passes")
{
    std::set<std::string> ref = {"Move %d of %d", "Free Renju board", "Analysis after {n} moves"};
    auto cat = i18n::Catalog::parse("Move %d of %d\tNuoc %d tren %d\n"
                                    "Free Renju board\tBang Free Renju\n"
                                    "Analysis after {n} moves\t\n");  // untranslated: skipped
    CHECK(i18n::lint::check(cat, ref).empty());
    // placeholder order may differ between languages
    auto swapped = i18n::Catalog::parse("Move %d of %d\t%d / %d\n");
    CHECK(i18n::lint::check(swapped, ref).empty());
}

TEST_CASE("I18N-01 lint negative fixtures: orphan key, missing placeholder, altered term")
{
    std::set<std::string> ref = {"Move %d of %d", "Analysis after {n} moves", "Open a .rdb file", "Show PV",
                                 "Free Renju board", "Gomoku rules"};

    auto orphan = i18n::lint::check(i18n::Catalog::parse("Not in the sources\tKhong co\n"), ref);
    CHECK(anyContains(orphan, "orphan key"));

    auto missing = i18n::lint::check(i18n::Catalog::parse("Move %d of %d\tNuoc %d\n"), ref);
    CHECK(anyContains(missing, "placeholder mismatch"));
    auto missingBrace = i18n::lint::check(i18n::Catalog::parse("Analysis after {n} moves\tSau khi phan tich\n"), ref);
    CHECK(anyContains(missingBrace, "placeholder mismatch"));

    auto rdb = i18n::lint::check(i18n::Catalog::parse("Open a .rdb file\tMo tep\n"), ref);
    CHECK(anyContains(rdb, "'.rdb'"));
    auto pv = i18n::lint::check(i18n::Catalog::parse("Show PV\tHien bien chinh\n"), ref);
    CHECK(anyContains(pv, "'PV'"));
    auto renju = i18n::lint::check(i18n::Catalog::parse("Free Renju board\tBang Renju tu do\n"), ref);
    CHECK(anyContains(renju, "'Free Renju'"));
    auto gomoku = i18n::lint::check(i18n::Catalog::parse("Gomoku rules\tLuat Co Ca Ro\n"), ref);
    CHECK(anyContains(gomoku, "'Gomoku'"));
}

TEST_CASE("I18N-01 lint: bundled vi.tsv against tr() literals in src/ (+ seed fixture)")
{
    auto ref = scanKeys(I18N_SRC_DIR);
    // Until I18N-02 wraps real UI strings, the seed fixture stands in for them.
    for (const auto &k : scanKeys(I18N_FIXTURE_DIR))
        ref.insert(k);
    CHECK_FALSE(ref.empty());

    auto vi = i18n::Catalog::parse(slurp(I18N_VI_PATH));
    CHECK_FALSE(vi.empty());
    auto problems = i18n::lint::check(vi, ref);
    for (const auto &p : problems)
        MESSAGE(p);
    CHECK(problems.empty());
}
