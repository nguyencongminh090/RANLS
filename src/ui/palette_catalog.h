#pragma once

// PAL-02: GTK-free catalog that turns the app's three searchable surfaces into
// palette_search::Entry rows: window actions (bilingual metadata kept here, next
// to nothing else because the Gio::Menu labels carry no Vietnamese/keywords —
// planning Q2), settings (settings_registry, PAL-03) and `!` console commands
// (live CommandSpec list, so .ptc extension commands appear after engine start).
// The act.* / cmd.* ids and wording mirror tests/data/palette/entries.tsv; the
// drift test (tests/test_pal02_palette_catalog.cpp) keeps the two in sync.

#include "command/command_dispatcher.h"
#include "command/palette_search.h"

#include <array>
#include <string_view>
#include <vector>

namespace palette_catalog {

struct ActionMeta {
    std::string_view id;  ///< "act.new-game" ...
    std::string_view titleEn;
    std::string_view titleVi;
    std::string_view keywordsEn;  ///< ';'-separated.
    std::string_view keywordsVi;
};

struct CommandMeta {
    std::string_view name;  ///< Built-in command name without '!'.
    std::string_view titleVi;
    std::string_view keywordsEn;
    std::string_view keywordsVi;
};

extern const std::array<ActionMeta, 22>  kActions;
extern const std::array<CommandMeta, 19> kCommands;

/// Actions + settings + one "cmd.<name>" entry per live `commands` spec.
std::vector<palette_search::Entry> buildEntries(const std::vector<CommandSpec> &commands);

/// True if the `!` command needs arguments (its usage has `<...>` or `[...]`).
bool commandTakesArguments(const CommandSpec &spec);

}  // namespace palette_catalog
