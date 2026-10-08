// I18N-04 — pure checks (no GTK): the palette's displayed title follows the UI language
// (English / Vietnamese from vi.tsv) while the search data and the ranking are identical
// under every UI language (story L7).

#include "vendor/doctest.h"

#include "i18n/i18n.h"
#include "i18n/i18n_lint.h"
#include "ui/palette_catalog.h"

#include <fstream>
#include <set>
#include <sstream>

namespace {
std::string slurp(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    REQUIRE_MESSAGE(f.good(), path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Installs the real vi.tsv as the "vi" catalog and restores English afterwards.
struct ViCatalog {
    ViCatalog()
    {
        const std::string text = slurp(I18N_VI_PATH);
        i18n::setCatalogLoader([text](std::string_view code) { return code == "vi" ? text : std::string(); });
        i18n::setLanguage("en");
    }
    ~ViCatalog()
    {
        i18n::setCatalogLoader(nullptr);
        i18n::setLanguage("en");
    }
};

std::vector<CommandSpec> specs()
{
    std::vector<CommandSpec> v;
    for (const auto &c : palette_catalog::kCommands)
        v.push_back({"x", std::string(c.name), "!" + std::string(c.name), "English summary of " + std::string(c.name)});
    v.push_back({"extension", "mycmd", "!mycmd", "An extension command"});  // .ptc: no Vietnamese
    return v;
}

const palette_search::Entry &byId(const std::vector<palette_search::Entry> &es, const std::string &id)
{
    for (const auto &e : es)
        if (e.id == id)
            return e;
    FAIL("no entry " << id);
    return es.front();
}
}  // namespace

TEST_CASE("I18N-04 displayed title: English under en, Vietnamese under vi (action, setting, console command)")
{
    ViCatalog guard;
    const auto entries = palette_catalog::buildEntries(specs());

    i18n::setLanguage("en");
    CHECK(palette_catalog::displayTitle(byId(entries, "act.settings")) == "Settings");
    CHECK(palette_catalog::displayTitle(byId(entries, "set.threads")) == "Threads");
    CHECK(palette_catalog::displayTitle(byId(entries, "cmd.undo")) == "!undo \xE2\x80\x94 English summary of undo");

    i18n::setLanguage("vi");
    CHECK(palette_catalog::displayTitle(byId(entries, "act.settings")) == "Cài đặt");
    CHECK(palette_catalog::displayTitle(byId(entries, "set.threads")) == "Số luồng");
    CHECK(palette_catalog::displayTitle(byId(entries, "cmd.undo")) == "!undo \xE2\x80\x94 Lùi một nước");
    // An extension command has no Vietnamese text: its English summary is shown.
    CHECK(palette_catalog::displayTitle(byId(entries, "cmd.mycmd")) == "!mycmd \xE2\x80\x94 An extension command");
    // Rule names are do-not-translate terms.
    CHECK(palette_catalog::displayTitle(byId(entries, "act.rule-renju")) == "Rule: Free Renju");
}

TEST_CASE("I18N-04 search data does not depend on the UI language (L7)")
{
    ViCatalog guard;
    i18n::setLanguage("en");
    const auto en = palette_catalog::buildEntries(specs());
    i18n::setLanguage("vi");
    const auto vi = palette_catalog::buildEntries(specs());

    REQUIRE(en.size() == vi.size());
    for (std::size_t i = 0; i < en.size(); ++i) {
        CHECK(en[i].id == vi[i].id);
        CHECK_MESSAGE(en[i].titleEn == vi[i].titleEn, en[i].id);
        CHECK_MESSAGE(en[i].titleVi == vi[i].titleVi, en[i].id);
        CHECK_MESSAGE(en[i].keywordsEn == vi[i].keywordsEn, en[i].id);
        CHECK_MESSAGE(en[i].keywordsVi == vi[i].keywordsVi, en[i].id);
        CHECK_MESSAGE(en[i].group == vi[i].group, en[i].id);
    }
    // The Vietnamese search title is still the Vietnamese one (read from vi.tsv under en).
    CHECK(byId(en, "act.settings").titleVi == "Cài đặt");
    CHECK(byId(en, "set.threads").titleVi == "Số luồng");
    CHECK(byId(en, "cmd.undo").titleVi == "!undo \xE2\x80\x94 Lùi một nước");
    CHECK(byId(en, "cmd.mycmd").titleVi == byId(en, "cmd.mycmd").titleEn);
}

TEST_CASE("I18N-04 ranking (ids, order, scores) is identical under en and vi for a fixed query set")
{
    ViCatalog guard;
    const auto lexicon = palette_search::Lexicon::parse(slurp(PALETTE_LEXICON_PATH));
    const char *queries[] = {"cài đặt", "settings", "threads", "số luồng", "!undo", "!hoàn tác", "ván mới", "new game",
                             "luật",    "rule",     "phan tich", "analyze", "@hash", ">stop",      "ngon ngu", "language",
                             "giao dien", "theme",  "phím tắt", "hotkey",  "thoát", "quit",        "abuot",    "kích thước",
                             "tỉ lệ thắng", "winrate", "!", "!engine", "!xóa"};

    auto run = [&](const char *lang) {
        i18n::setLanguage(lang);
        palette_search::Index idx(palette_catalog::buildEntries(specs()), lexicon);
        std::vector<std::string> out;
        for (const char *q : queries) {
            std::string line = q;
            for (const auto &h : idx.search(q, 8))
                line += " " + idx.entry(h.index).id + "=" + std::to_string(h.score);
            out.push_back(line);
        }
        return out;
    };
    const auto en = run("en");
    const auto vi = run("vi");
    REQUIRE(en.size() == vi.size());
    for (std::size_t i = 0; i < en.size(); ++i)
        CHECK_MESSAGE(en[i] == vi[i], en[i]);
    CHECK(en[0].find("act.settings") != std::string::npos);  // sanity: results are not all empty
}

TEST_CASE("I18N-04 every built-in palette title has a Vietnamese catalog entry")
{
    ViCatalog guard;
    for (const auto &a : palette_catalog::kActions)
        CHECK_MESSAGE(i18n::lookupIn("vi", "palette|" + std::string(a.titleEn)), a.id);
    for (const auto &c : palette_catalog::kCommands)
        CHECK_MESSAGE(i18n::lookupIn("vi", "palette-cmd|" + std::string(c.name)), c.name);
}

TEST_CASE("I18N-04 lint: a do-not-translate term joined camel-case (MultiPV) still counts as kept")
{
    const std::set<std::string> ref = {"palette|Multi PV"};
    CHECK(i18n::lint::check(i18n::Catalog::parse("palette|Multi PV\tNhiều biến (MultiPV)\n"), ref).empty());
    CHECK_FALSE(i18n::lint::check(i18n::Catalog::parse("palette|Multi PV\tNhiều biến\n"), ref).empty());
}
