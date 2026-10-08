// PAL-03 — pure checks on the declarative settings table (no GTK, no display).
// Guards: unique well-formed ids, tabs in range, and that every registry id is
// present in the shared palette dataset (tests/data/palette/entries.tsv) so the
// benchmark corpus cannot silently drift from the real Settings dialog.

#include "vendor/doctest.h"

#include "ui/settings_registry.h"

#include <fstream>
#include <set>
#include <sstream>
#include <string>

using namespace settings_registry;

TEST_CASE("PAL-03 registry rows are well-formed and unique")
{
    std::set<std::string_view> seen;
    for (const auto &e : kSettings) {
        CHECK_MESSAGE(e.id.rfind("set.", 0) == 0, e.id);
        CHECK_MESSAGE(seen.insert(e.id).second, "duplicate id " << e.id);
        CHECK(e.tab >= 0);
        CHECK(e.tab < static_cast<int>(kTabs.size()));
        CHECK_FALSE(e.label.empty());
        CHECK_FALSE(e.titleEn.empty());
        CHECK_FALSE(e.keywordsEn.empty());
        CHECK_FALSE(e.keywordsVi.empty());
    }
    CHECK(kSettings.size() == 21);
}

TEST_CASE("PAL-03 find() resolves known ids only")
{
    REQUIRE(find("set.threads") != nullptr);
    CHECK(find("set.threads")->label == "Threads");
    CHECK(find("set.hotkey-undo")->tab == 4);
    CHECK(find("set.nonexistent") == nullptr);
}

TEST_CASE("PAL-03 every registry id exists in the palette dataset")
{
    std::ifstream f(std::string(PALETTE_DATA_DIR) + "/entries.tsv");
    REQUIRE(f.good());
    std::set<std::string> dataset;
    std::string           line;
    while (std::getline(f, line)) {
        const auto tab = line.find('\t');
        if (tab != std::string::npos)
            dataset.insert(line.substr(0, tab));
    }
    for (const auto &e : kSettings)
        CHECK_MESSAGE(dataset.count(std::string(e.id)) == 1, "missing from entries.tsv: " << e.id);
    // ...and the reverse: no stale `set.*` row in the dataset.
    for (const auto &id : dataset)
        if (id.rfind("set.", 0) == 0)
            CHECK_MESSAGE(find(id) != nullptr, "dataset id not in registry: " << id);
}
