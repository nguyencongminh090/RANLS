// ARCH-03: pins the full `!help` output plus registeredNames()/commandUsage()
// of CommandDispatcher so that splitting registerBuiltins() into per-group
// registrars and narrowing CommandContext provably changes nothing observable.
// Golden text was captured from the pre-refactor code.
#include "vendor/doctest.h"

#include "command/command_dispatcher.h"
#include "engine/engine_controller.h"
#include "engine/engine_process.h"
#include "model/game_state.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
struct HelpCtx {
    GameState gs;
    EngineProcess eng;
    EngineController ctrl{gs, eng};
    std::vector<std::string> out;

    CommandContext ctx()
    {
        return CommandContext{
            .gameState = gs,
            .controller = ctrl,
            .print = [this](const std::string &l) { out.push_back(l); },
            .clearConsole = [] {},
        };
    }
};

std::string join(const std::vector<std::string> &v)
{
    std::string s;
    for (const auto &l : v) { s += l; s += '\n'; }
    return s;
}

const char *const kGoldenHelp = R"GOLDEN(Commands (v1).
  - Internal commands must start with '!': they keep UI + engine in sync.
  - Lines without '!' are sent as raw protocol to the engine (restricted).
Tip: use quotes for values with spaces.

[analysis]
  !analyze [n] — Start analysis (optional n = MultiPV)
  !play <moveText...> — Load moves then start analysis
  !stop — Stop analysis (STOP)
  !yxAnalz <moveText...> — Analyze listed root moves (YXANALZ protocol extension—requires compatible engine); interruptible; no stone; alphabetic moves

[board]
  !getpos — Print current position as y,x,color lines
  !loadpos — Load position from move text (end with: done)
  !new — Restart current game (keep size)
  !pos <moveText...> | !pos (then lines, end with: done) — Position input helper
  !redo — Redo one move
  !rule <freestyle|standard|renju> — Set rule and sync engine
  !start <size> — New game with board size; sync engine if running
  !undo — Undo one move

[config]
  !info set <key> <value> — Edit engine config and sendConfig

[database]
  !db <query|load|save|on|off|label|comment|delete> — Database commands

[debug]
  !clear — Clear the console log

[engine]
  !engine <start|stop|reload> — Engine lifecycle
  !send <raw engine line> — Send a raw protocol line to the engine

[info]
  !about — Send ABOUT to the engine
  !help — Show this help (grouped)
)GOLDEN";
const char *const kGoldenNamesUsage = R"GOLDEN(about	!about
analyze	!analyze [n]
clear	!clear
db	!db <query|load|save|on|off|label|comment|delete>
engine	!engine <start|stop|reload>
getpos	!getpos
help	!help
info	!info set <key> <value>
loadpos	!loadpos
new	!new
play	!play <moveText...>
pos	!pos <moveText...> | !pos (then lines, end with: done)
redo	!redo
rule	!rule <freestyle|standard|renju>
send	!send <raw engine line>
start	!start <size>
stop	!stop
undo	!undo
yxanalz	!yxAnalz <moveText...>
)GOLDEN";
}  // namespace

TEST_CASE("ARCH-03: !help output is byte-identical to the pinned golden")
{
    HelpCtx c;
    CommandDispatcher disp(c.ctx());
    disp.printHelp();
    const std::string actual = join(c.out);
    if (std::getenv("ARCH03_DUMP")) std::fprintf(stderr, "@@HELP_BEGIN@@\n%s@@HELP_END@@\n", actual.c_str());
    CHECK(actual == std::string(kGoldenHelp));
}

TEST_CASE("ARCH-03: registeredNames() and commandUsage() match the pinned golden")
{
    HelpCtx c;
    CommandDispatcher disp(c.ctx());
    std::string actual;
    for (const auto &n : disp.registeredNames())
        actual += n + "\t" + disp.commandUsage(n) + "\n";
    if (std::getenv("ARCH03_DUMP")) std::fprintf(stderr, "@@NAMES_BEGIN@@\n%s@@NAMES_END@@\n", actual.c_str());
    CHECK(actual == std::string(kGoldenNamesUsage));
}
