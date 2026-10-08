// I18N-04 — widget-level: the Ctrl+K palette rows show the title in the UI language,
// while the ids/order a query returns are the same in English and Vietnamese (L7).
// Links gtkmm; self-skips with no display server.

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "i18n/i18n.h"
#include "ui/command_palette.h"
#include "ui/palette_catalog.h"

#include <chrono>
#include <fstream>
#include <sstream>

namespace {

bool gtkReady() { return gtk_init_check(); }

void pump(int ms = 50)
{
    auto *ctx = g_main_context_default();
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end)
        while (g_main_context_iteration(ctx, FALSE)) {}
}

std::string slurp(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct Fixture {
    Gtk::Window win;
    Gtk::Box    root{Gtk::Orientation::VERTICAL};
    std::unique_ptr<CommandPalette> palette;

    Fixture()
    {
        const std::string vi = slurp(I18N_VI_PATH);
        i18n::setCatalogLoader([vi](std::string_view code) { return code == "vi" ? vi : std::string(); });
        i18n::setLanguage("en");
        win.set_child(root);
        win.set_default_size(800, 600);
        win.present();
        pump();
        palette = std::make_unique<CommandPalette>(root, slurp(PALETTE_LEXICON_PATH));
        palette->setItemsProvider([]() {
            std::vector<CommandSpec> specs;
            for (const auto &c : palette_catalog::kCommands)
                specs.push_back({"x", std::string(c.name), "!" + std::string(c.name),
                                 "English summary of " + std::string(c.name)});
            std::vector<PaletteItem> items;
            for (auto &e : palette_catalog::buildEntries(specs)) {
                PaletteItem it;
                it.entry = std::move(e);
                items.push_back(std::move(it));
            }
            return items;
        });
    }
    ~Fixture()
    {
        palette.reset();
        i18n::setCatalogLoader(nullptr);
        i18n::setLanguage("en");
    }
    // Ids + primary titles shown for `query` under UI language `lang`.
    std::pair<std::vector<std::string>, std::vector<std::string>> run(const char *lang, const std::string &query)
    {
        i18n::setLanguage(lang);
        palette->open();
        palette->setQuery(query);
        return {palette->resultIds(), palette->resultTitles()};
    }
};

}  // namespace

TEST_CASE("I18N-04 palette rows: English titles under en, Vietnamese under vi (action, setting, command)")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    Fixture f;

    auto [ids, titles] = f.run("en", "settings");
    REQUIRE_FALSE(ids.empty());
    CHECK(ids.front() == "act.settings");
    CHECK(titles.front() == "Settings");

    std::tie(ids, titles) = f.run("vi", "settings");  // English query, Vietnamese UI
    REQUIRE_FALSE(ids.empty());
    CHECK(ids.front() == "act.settings");
    CHECK(titles.front() == "Cài đặt");

    std::tie(ids, titles) = f.run("vi", "threads");
    REQUIRE_FALSE(ids.empty());
    CHECK(ids.front() == "set.threads");
    CHECK(titles.front() == "Số luồng");

    std::tie(ids, titles) = f.run("en", "threads");
    REQUIRE_FALSE(ids.empty());
    CHECK(titles.front() == "Threads");

    std::tie(ids, titles) = f.run("vi", "!undo");
    REQUIRE_FALSE(ids.empty());
    CHECK(ids.front() == "cmd.undo");
    CHECK(titles.front() == "!undo \xE2\x80\x94 Lùi một nước");

    std::tie(ids, titles) = f.run("en", "!undo");
    REQUIRE_FALSE(ids.empty());
    CHECK(titles.front() == "!undo \xE2\x80\x94 English summary of undo");
}

TEST_CASE("I18N-04 palette: the same query returns the same ids in the same order in en and vi (L7)")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    Fixture f;
    for (const char *q : {"cài đặt", "settings", "cai dat", "ván mới", "new game", "luật", "rule", "phân tích",
                          "analyze", "số luồng", "threads", "!trợ giúp", "!help", "@ngôn ngữ", "@language",
                          ">thoát", ">quit"}) {
        const auto en = f.run("en", q).first;
        const auto vi = f.run("vi", q).first;
        CHECK_MESSAGE(en == vi, "query: " << q);
    }
    CHECK_FALSE(f.run("vi", "cài đặt").first.empty());
}
