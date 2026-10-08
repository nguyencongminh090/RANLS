// PAL-02 — pure checks (no GTK): the catalog stays in sync with the shared
// palette dataset, `!` command entries are built from live specs, the recency
// helpers behave, and the recent list round-trips through the settings file.

#include "vendor/doctest.h"

#include "command/palette_recent.h"
#include "model/settings_storage.h"
#include "ui/palette_catalog.h"

#include <cstdio>
#include <fstream>
#include <map>
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

std::vector<CommandSpec> builtinSpecs()
{
    std::vector<CommandSpec> v;
    for (const auto &c : palette_catalog::kCommands)
        v.push_back({"x", std::string(c.name), "!" + std::string(c.name) + " <arg>", "summary of " + std::string(c.name)});
    return v;
}
}  // namespace

TEST_CASE("PAL-02 catalog act.*/cmd.* ids are exactly the dataset's")
{
    const auto entries = palette_search::parseEntriesTsv(slurp(std::string(PALETTE_DATA_DIR) + "/entries.tsv"));
    std::set<std::string> dsAct, dsCmd;
    for (const auto &e : entries) {
        if (e.kind == "act") dsAct.insert(e.id);
        if (e.kind == "cmd") dsCmd.insert(e.id);
    }
    std::set<std::string> catAct, catCmd;
    for (const auto &a : palette_catalog::kActions) catAct.insert(std::string(a.id));
    for (const auto &c : palette_catalog::kCommands) catCmd.insert("cmd." + std::string(c.name));
    CHECK(catAct == dsAct);
    CHECK(catCmd == dsCmd);
}

TEST_CASE("PAL-02 buildEntries: actions + settings + one entry per live command")
{
    auto specs = builtinSpecs();
    specs.push_back({"extension", "mycmd", "!mycmd", "An extension command"});  // .ptc, no catalog meta
    const auto entries = palette_catalog::buildEntries(specs);
    CHECK(entries.size() == palette_catalog::kActions.size() + 20 + specs.size());

    std::map<std::string, const palette_search::Entry *> byId;
    for (const auto &e : entries) {
        CHECK_MESSAGE(byId.emplace(e.id, &e).second, "duplicate id " << e.id);
        CHECK_FALSE(e.titleEn.empty());
    }
    REQUIRE(byId.count("cmd.mycmd"));
    CHECK(byId["cmd.mycmd"]->kind == "cmd");
    CHECK(byId["cmd.mycmd"]->titleVi == byId["cmd.mycmd"]->titleEn);  // no Vietnamese available
    CHECK(byId["cmd.undo"]->titleVi != byId["cmd.undo"]->titleEn);    // built-in has it
    CHECK(byId["set.threads"]->kind == "set");
    CHECK(byId["act.settings"]->kind == "act");
}

TEST_CASE("PAL-02 the live catalog is searchable end to end")
{
    palette_search::Index idx(palette_catalog::buildEntries(builtinSpecs()),
                              palette_search::Lexicon::parse(slurp(PALETTE_LEXICON_PATH)));
    auto first = [&](const char *q) { return idx.entry(idx.search(q).front().index).id; };
    CHECK(first("cài đặt") == "act.settings");
    CHECK(first("threads") == "set.threads");
    CHECK(first("!undo") == "cmd.undo");
}

TEST_CASE("PAL-02 commandTakesArguments")
{
    CHECK(palette_catalog::commandTakesArguments({"g", "analyze", "!analyze [n]", ""}));
    CHECK(palette_catalog::commandTakesArguments({"g", "rule", "!rule <freestyle|standard|renju>", ""}));
    CHECK_FALSE(palette_catalog::commandTakesArguments({"g", "undo", "!undo", ""}));
}

TEST_CASE("PAL-02 recent: touch de-dupes, orders newest first and caps")
{
    std::vector<std::string> r;
    palette_recent::touch(r, "a");
    palette_recent::touch(r, "b");
    palette_recent::touch(r, "a");
    CHECK(r == std::vector<std::string>{"a", "b"});
    for (int i = 0; i < 80; ++i)
        palette_recent::touch(r, "x" + std::to_string(i));
    CHECK(r.size() == palette_recent::kCap);
    CHECK(r.front() == "x79");
}

TEST_CASE("PAL-02 recent: recency lifts a tied hit, never invents one")
{
    palette_search::Index idx(palette_catalog::buildEntries(builtinSpecs()),
                              palette_search::Lexicon::parse(slurp(PALETTE_LEXICON_PATH)));
    auto hits = idx.search("redo");
    REQUIRE(hits.size() >= 2);
    const std::string second = idx.entry(hits[1].index).id;
    palette_recent::applyRecency(hits, idx, {second});
    CHECK(hits.size() >= 2);
    CHECK(idx.entry(hits.front().index).id == second);   // boosted past the former leader
    std::set<std::string> ids;
    for (const auto &h : hits) CHECK(ids.insert(idx.entry(h.index).id).second);
}

TEST_CASE("PAL-02 recent list round-trips through the settings file")
{
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
    ViewConfig v;
    v.paletteRecent = {"act.settings", "cmd.undo", "set.threads"};
    REQUIRE(SettingsStorage::save(EngineConfig{}, v));
    const auto loaded = SettingsStorage::load();
    CHECK(loaded.view.paletteRecent == v.paletteRecent);
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}
