// I18N-03 — pure checks (no GTK, no display): the `language=` setting's
// persistence round-trip and fallback, the setting -> concrete language
// resolution glue, and the PAL-03 registry row for `set.language`.

#include "vendor/doctest.h"

#include "i18n/i18n.h"
#include "model/settings_storage.h"
#include "ui/settings_registry.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

void removeSettingsFile() { std::remove(SettingsStorage::settingsFilePath().string().c_str()); }

void writeSettingsFile(const std::string &text)
{
    std::ofstream out(SettingsStorage::settingsFilePath(), std::ios::trunc);
    out << text;
}

std::string savedText()
{
    std::ifstream in(SettingsStorage::settingsFilePath());
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

TEST_CASE("I18N-03 language= round-trips through SettingsStorage")
{
    for (const char *value : {"system", "en", "vi"}) {
        ViewConfig view;
        view.language = value;
        REQUIRE(SettingsStorage::save(EngineConfig{}, view));
        CHECK(savedText().find(std::string("language=") + value + "\n") != std::string::npos);
        CHECK(SettingsStorage::load().view.language == value);
    }
    removeSettingsFile();
}

TEST_CASE("I18N-03 missing, unknown or garbled language= loads as system (and is not written back)")
{
    removeSettingsFile();
    CHECK(SettingsStorage::load().view.language == "system");  // no file

    for (const char *bad : {"fr", "", "VI", "vi_VN", "klingon", "1", "system "}) {
        writeSettingsFile(std::string("engine_path=/x\nlanguage=") + bad + "\n");
        CHECK_MESSAGE(SettingsStorage::load().view.language == "system", "value '" << bad << "'");
    }
    writeSettingsFile("engine_path=/x\n");  // key absent
    CHECK(SettingsStorage::load().view.language == "system");

    // A bad in-memory value is also normalised on save, so it never sticks.
    ViewConfig view;
    view.language = "fr";
    REQUIRE(SettingsStorage::save(EngineConfig{}, view));
    CHECK(savedText().find("language=system\n") != std::string::npos);
    removeSettingsFile();
}

TEST_CASE("I18N-03 resolveSetting: explicit codes win, system follows the locale, junk acts as system")
{
    const std::vector<std::string> viLocale = {"vi_VN.UTF-8", "vi_VN", "vi", "C"};
    const std::vector<std::string> frLocale = {"fr_FR.UTF-8", "fr_FR", "fr", "C"};
    const std::vector<std::string> cLocale = {"C"};

    CHECK(i18n::resolveSetting("system", viLocale) == "vi");
    CHECK(i18n::resolveSetting("system", frLocale) == "en");
    CHECK(i18n::resolveSetting("system", cLocale) == "en");
    CHECK(i18n::resolveSetting("system", {}) == "en");
    CHECK(i18n::resolveSetting("en", viLocale) == "en");
    CHECK(i18n::resolveSetting("vi", frLocale) == "vi");
    CHECK(i18n::resolveSetting("klingon", viLocale) == "vi");
    CHECK(i18n::resolveSetting("", frLocale) == "en");

    CHECK(i18n::normalizeLanguageSetting("vi") == "vi");
    CHECK(i18n::normalizeLanguageSetting("xx") == "system");
}

TEST_CASE("I18N-03 registry has set.language on the UI tab with bilingual search text")
{
    const auto *e = settings_registry::find("set.language");
    REQUIRE(e != nullptr);
    CHECK(e->tab == 3);
    CHECK(e->label == "Language");
    CHECK(e->titleVi == "Ngôn ngữ");
    CHECK_FALSE(e->keywordsEn.empty());
    CHECK_FALSE(e->keywordsVi.empty());
}
