// I18N-02 — pure checks (no GTK): language-changed listeners, printf-style
// template substitution, and that the Settings dialog's registry-label table
// has a tr() key for every settings_registry row.

#include "vendor/doctest.h"

#include "i18n/i18n.h"
#include "i18n/i18n_lint.h"
#include "ui/settings_registry.h"

#include <fstream>
#include <sstream>

namespace {
struct LanguageGuard {
    ~LanguageGuard()
    {
        i18n::setCatalogLoader(nullptr);
        i18n::setLanguage("en");
    }
};
}  // namespace

TEST_CASE("I18N-02 setLanguage notifies listeners (after the catalog switched); removal stops it")
{
    LanguageGuard g;
    i18n::setCatalogLoader([](std::string_view code) { return code == "vi" ? std::string("Save\tLuu\n") : std::string(); });

    std::vector<std::string> seen;
    auto id = i18n::addLanguageListener([&]() { seen.push_back(i18n::currentLanguage() + ":" + i18n::tr("Save")); });

    CHECK_FALSE(i18n::setLanguage("fr"));  // unsupported: no notification
    CHECK(seen.empty());
    CHECK(i18n::setLanguage("vi"));
    CHECK(i18n::setLanguage("en"));
    CHECK(seen == std::vector<std::string>{"vi:Luu", "en:Save"});

    i18n::removeLanguageListener(id);
    i18n::setLanguage("vi");
    CHECK(seen.size() == 2);
}

TEST_CASE("I18N-02 format substitutes %s / %d in an (already translated) template")
{
    CHECK(i18n::format("Rule: %s", "Free Renju") == "Rule: Free Renju");
    CHECK(i18n::format("Move %d of %d", 3, 12) == "Move 3 of 12");
    CHECK(i18n::format("Engine crashed: %s", std::string("/opt/rapfi")) == "Engine crashed: /opt/rapfi");
    CHECK(i18n::format("no placeholders") == "no placeholders");
    CHECK(i18n::format("%s sẽ xóa ván. Tiếp tục?", "Bắt đầu ván mới") == "Bắt đầu ván mới sẽ xóa ván. Tiếp tục?");
}

TEST_CASE("I18N-02 every settings_registry label has a tr() key in settings_dialog.cpp")
{
    std::ifstream f(I18N_SRC_DIR "/ui/settings_dialog.cpp", std::ios::binary);
    REQUIRE(f.good());
    std::stringstream ss;
    ss << f.rdbuf();
    const auto keys = i18n::lint::extractKeys(ss.str());
    for (const auto &e : settings_registry::kSettings)
        CHECK_MESSAGE(keys.count(std::string(e.label)), "no tr() key for registry label '" << e.label << "'");
    for (const auto &tab : settings_registry::kTabs)
        CHECK_MESSAGE(keys.count(std::string(tab)), "no tr() key for tab '" << tab << "'");
}
