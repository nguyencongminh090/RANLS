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
#include "i18n/i18n.h"

#include <array>
#include <string_view>
#include <vector>

namespace palette_catalog {

// I18N-04: the title is a catalog key (ctx "palette", written trcNoop(...) so the
// i18n lint sees it). The Vietnamese title is NOT stored here: it lives in vi.tsv and
// is read back with i18n::trIn("vi", ...) as search data, so the palette matches
// Vietnamese whatever the UI language is (story L7); the *displayed* title is
// displayTitle(). The keyword lists are search data only and stay bilingual here.
struct ActionMeta {
    std::string_view id;  ///< "act.new-game" ...
    std::string_view titleEn;  ///< English title = catalog key (ctx "palette").
    std::string_view keywordsEn;  ///< ';'-separated.
    std::string_view keywordsVi;
};

struct CommandMeta {
    std::string_view name;  ///< Built-in command name without '!' (also the ctx "palette-cmd" key).
    std::string_view keywordsEn;
    std::string_view keywordsVi;
};

extern const std::array<ActionMeta, 22>  kActions;
extern const std::array<CommandMeta, 19> kCommands;

/// Actions + settings + one "cmd.<name>" entry per live `commands` spec.
std::vector<palette_search::Entry> buildEntries(const std::vector<CommandSpec> &commands);

/// Title to show for `e` in the current UI language (I18N-04): the catalog text of
/// the English title for actions/settings; "!name - " + the catalog's text (or the
/// live English summary when the language has none) for console commands. Never
/// used for ranking.
std::string displayTitle(const palette_search::Entry &e);
/// Same, in a specific language (`lang` "en" = the English title).
std::string displayTitle(const palette_search::Entry &e, std::string_view lang);

/// True if the `!` command needs arguments (its usage has `<...>` or `[...]`).
bool commandTakesArguments(const CommandSpec &spec);

}  // namespace palette_catalog
