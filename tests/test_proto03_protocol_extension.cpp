// PROTO-03 regression tests — the `.ptc` loader + mini-DSL interpreter.
//
// Model/engine layer only (no gtkmm, no subprocess), matching how
// GomocupProtocol / PROTO-01 / PROTO-02 are tested. Covers:
//   * a valid .ptc loads and its command names are exposed
//   * a name collision with a built-in rejects the WHOLE file
//   * each Q5 limit individually rejects when exceeded
//   * single-line send and open/repeat/close block send (repeat() arg AND
//     $currentPath) produce the exact expected wire-line sequence, with
//     i.color alternation matching generateAnalyzeRequest's own formula
//   * a well-formed on_reply fires the right sink with the right args
//   * a type-mismatched reply line is skipped (Debug log, no crash)

#include "vendor/doctest.h"

#include "engine/protocol_extension.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

using protoext::ExtensionTable;
using protoext::PathItem;
using protoext::SinkAction;

namespace {

const std::vector<std::string> &reserved() { return protoext::builtinCommandNames(); }

std::optional<ExtensionTable> load(const std::string &text, std::string &err)
{
    return ExtensionTable::loadFromString(text, reserved(), err);
}

// A complete, well-formed file exercising all three send shapes.
const char *kGoodPtc = R"PTC(
ptc_version = 1
extends = "gomocup"

[[command]]
name  = "yxAnalyzeOne"
group = "info"
args  = ["x:int", "y:int"]
send  = "YXANALYZEONE {y},{x}"

[[command.on_reply]]
pattern = "MOVEEVAL {y:int},{x:int} {win:float}"
action = """
if win > 0.9 then
    toast("Strong move found")
elif win < 0.1 then
    toast("Weak move warning")
end
set_status_field("yxAnalyzeOne.eval", win)
log("yxAnalyzeOne done")
"""

[[command]]
name  = "yxDbSync"
group = "database"
args  = []
send.open          = "YXBOARD"
send.repeat_source = "$currentPath"
send.repeat        = "{i.y},{i.x},{i.color}"
send.close         = "DONE"
send.after         = "YXQUERYDB"

[[command]]
name  = "yxLineCheck"
group = "board"
args  = ["moves:repeat(coord)"]
send.open          = "YXLINE"
send.repeat_source = "moves"
send.repeat        = "{i.y},{i.x}"
send.close         = "DONE"
)PTC";

} // namespace

TEST_CASE("PROTO-03: a valid .ptc loads and exposes its command names")
{
    std::string err;
    auto table = load(kGoodPtc, err);
    REQUIRE_MESSAGE(table.has_value(), err);
    auto names = table->commandNames();
    REQUIRE(names.size() == 3);
    CHECK(names[0] == "yxAnalyzeOne");
    CHECK(names[1] == "yxDbSync");
    CHECK(names[2] == "yxLineCheck");
    CHECK(table->find("yxAnalyzeOne")->group == "info");
}

TEST_CASE("PROTO-03: a name collision with a built-in rejects the whole file")
{
    const char *ptc = R"PTC(
ptc_version = 1
extends = "gomocup"
[[command]]
name = "play"
group = "info"
send = "FOO"
)PTC";
    std::string err;
    auto table = load(ptc, err);
    CHECK_FALSE(table.has_value());
    CHECK(err.find("play") != std::string::npos);
}

TEST_CASE("PROTO-03: header must be ptc_version=1 / extends=gomocup")
{
    std::string err;
    CHECK_FALSE(load("ptc_version = 2\nextends = \"gomocup\"\n[[command]]\nname=\"x\"\ngroup=\"info\"\nsend=\"F\"\n", err).has_value());
    CHECK_FALSE(load("ptc_version = 1\nextends = \"uci\"\n[[command]]\nname=\"x\"\ngroup=\"info\"\nsend=\"F\"\n", err).has_value());
}

TEST_CASE("PROTO-03: 'analysis' group is rejected in v1")
{
    const char *ptc = R"PTC(
ptc_version = 1
extends = "gomocup"
[[command]]
name = "deepThink"
group = "analysis"
send = "GO"
)PTC";
    std::string err;
    CHECK_FALSE(load(ptc, err).has_value());
}

TEST_CASE("PROTO-03: each Q5 limit individually rejects when exceeded")
{
    SUBCASE("file size > 64 KB")
    {
        std::string ptc = "ptc_version = 1\nextends = \"gomocup\"\n";
        ptc += std::string(65 * 1024, '#');
        ptc += "\n[[command]]\nname=\"x\"\ngroup=\"info\"\nsend=\"F\"\n";
        std::string err;
        CHECK_FALSE(load(ptc, err).has_value());
        CHECK(err.find("bytes") != std::string::npos);
    }

    SUBCASE("commands per file > 32")
    {
        std::string ptc = "ptc_version = 1\nextends = \"gomocup\"\n";
        for (int i = 0; i < 33; ++i)
            ptc += "[[command]]\nname=\"c" + std::to_string(i) +
                   "\"\ngroup=\"info\"\nsend=\"F\"\n";
        std::string err;
        CHECK_FALSE(load(ptc, err).has_value());
    }

    SUBCASE("args per command > 8")
    {
        std::string ptc = "ptc_version = 1\nextends = \"gomocup\"\n[[command]]\n"
                          "name=\"c\"\ngroup=\"info\"\nargs=[";
        for (int i = 0; i < 9; ++i) ptc += (i ? "," : "") + std::string("\"a") + std::to_string(i) + ":int\"";
        ptc += "]\nsend=\"F\"\n";
        std::string err;
        CHECK_FALSE(load(ptc, err).has_value());
    }

    SUBCASE("if-nesting depth > 4")
    {
        std::string act = "action = \"\"\"\n";
        for (int i = 0; i < 5; ++i) act += "if win > 0 then\n";
        act += "log(\"x\")\n";
        for (int i = 0; i < 5; ++i) act += "end\n";
        act += "\"\"\"\n";
        std::string ptc = "ptc_version = 1\nextends = \"gomocup\"\n[[command]]\n"
                          "name=\"c\"\ngroup=\"info\"\nsend=\"F\"\n"
                          "[[command.on_reply]]\npattern = \"R {win:float}\"\n" + act;
        std::string err;
        auto t = load(ptc, err);
        CHECK_FALSE(t.has_value());
        CHECK(err.find("depth") != std::string::npos);
    }

    SUBCASE("sink calls per action > 16")
    {
        std::string act = "action = \"\"\"\n";
        for (int i = 0; i < 17; ++i) act += "log(\"x\")\n";
        act += "\"\"\"\n";
        std::string ptc = "ptc_version = 1\nextends = \"gomocup\"\n[[command]]\n"
                          "name=\"c\"\ngroup=\"info\"\nsend=\"F\"\n"
                          "[[command.on_reply]]\npattern = \"R {win:float}\"\n" + act;
        std::string err;
        CHECK_FALSE(load(ptc, err).has_value());
    }
}

TEST_CASE("PROTO-03: single-line send interpolates console args")
{
    std::string err;
    auto table = load(kGoodPtc, err);
    REQUIRE(table.has_value());

    std::vector<std::string> out;
    // args declared [x:int, y:int]; console gives x=7 y=8.
    REQUIRE(table->generateSend("yxAnalyzeOne", {"7", "8"}, {}, out, err));
    REQUIRE(out.size() == 1);
    CHECK(out[0] == "YXANALYZEONE 8,7");

    // wrong arity / type rejected
    CHECK_FALSE(table->generateSend("yxAnalyzeOne", {"7"}, {}, out, err));
    CHECK_FALSE(table->generateSend("yxAnalyzeOne", {"7", "x"}, {}, out, err));
}

TEST_CASE("PROTO-03: block send over $currentPath emits open/repeat/close/after with alternating color")
{
    std::string err;
    auto table = load(kGoodPtc, err);
    REQUIRE(table.has_value());

    // Build a 3-move path; color must alternate 1,2,1 — same formula as
    // GomocupProtocol::generateAnalyzeRequest ((i % 2 == 0) ? 1 : 2).
    std::vector<PathItem> path;
    for (int i = 0; i < 3; ++i) {
        PathItem it;
        it.x = i;
        it.y = i + 5;
        it.color = (i % 2 == 0) ? 1 : 2;
        path.push_back(it);
    }

    std::vector<std::string> out;
    REQUIRE(table->generateSend("yxDbSync", {}, path, out, err));
    std::vector<std::string> expected = {
        "YXBOARD",
        "5,0,1",
        "6,1,2",
        "7,2,1",
        "DONE",
        "YXQUERYDB",
    };
    CHECK(out == expected);
}

TEST_CASE("PROTO-03: block send over a repeat() console arg")
{
    std::string err;
    auto table = load(kGoodPtc, err);
    REQUIRE(table.has_value());

    std::vector<std::string> out;
    // moves:repeat(coord) — each token is x,y.
    REQUIRE(table->generateSend("yxLineCheck", {"0,5", "1,6", "2,7"}, {}, out, err));
    std::vector<std::string> expected = {
        "YXLINE",
        "5,0",
        "6,1",
        "7,2",
        "DONE",
    };
    CHECK(out == expected);
}

TEST_CASE("PROTO-03: a well-formed on_reply fires the right sinks with the right args")
{
    std::string err;
    auto table = load(kGoodPtc, err);
    REQUIRE(table.has_value());

    std::vector<SinkAction> fired;
    int debugCount = 0;
    bool matched = table->matchReply(
        "MOVEEVAL 8,7 0.93",
        [&](const SinkAction &a) { fired.push_back(a); },
        [&](const std::string &) { ++debugCount; });

    CHECK(matched);
    CHECK(debugCount == 0);
    REQUIRE(fired.size() == 3);
    CHECK(fired[0].sink == "toast");
    CHECK(fired[0].args == std::vector<std::string>{"Strong move found"});
    CHECK(fired[1].sink == "set_status_field");
    CHECK(fired[1].args == std::vector<std::string>{"yxAnalyzeOne.eval", "0.93"});
    CHECK(fired[2].sink == "log");
    CHECK(fired[2].args == std::vector<std::string>{"yxAnalyzeOne done"});
}

TEST_CASE("PROTO-03: elif / else branch selection")
{
    std::string err;
    auto table = load(kGoodPtc, err);
    REQUIRE(table.has_value());

    std::vector<SinkAction> fired;
    table->matchReply("MOVEEVAL 8,7 0.05",
                      [&](const SinkAction &a) { fired.push_back(a); },
                      [&](const std::string &) {});
    REQUIRE(fired.size() == 3);
    CHECK(fired[0].sink == "toast");
    CHECK(fired[0].args == std::vector<std::string>{"Weak move warning"});

    fired.clear();
    table->matchReply("MOVEEVAL 8,7 0.5",
                      [&](const SinkAction &a) { fired.push_back(a); },
                      [&](const std::string &) {});
    // neither if nor elif → only the two unconditional sinks
    REQUIRE(fired.size() == 2);
    CHECK(fired[0].sink == "set_status_field");
}

TEST_CASE("PROTO-03: a type-mismatched reply line is skipped with a Debug log, no crash")
{
    std::string err;
    auto table = load(kGoodPtc, err);
    REQUIRE(table.has_value());

    std::vector<SinkAction> fired;
    int debugCount = 0;
    bool matched = table->matchReply(
        "MOVEEVAL 8,7 not-a-float",
        [&](const SinkAction &a) { fired.push_back(a); },
        [&](const std::string &) { ++debugCount; });

    CHECK_FALSE(matched);
    CHECK(fired.empty());
    CHECK(debugCount == 1);
}

TEST_CASE("PROTO-03: a non-matching reply line is silently ignored")
{
    std::string err;
    auto table = load(kGoodPtc, err);
    REQUIRE(table.has_value());

    int debugCount = 0, sinkCount = 0;
    bool matched = table->matchReply(
        "SOMETHING ELSE ENTIRELY",
        [&](const SinkAction &) { ++sinkCount; },
        [&](const std::string &) { ++debugCount; });
    CHECK_FALSE(matched);
    CHECK(debugCount == 0);
    CHECK(sinkCount == 0);
}

TEST_CASE("PROTO-03: builtin registry matches CommandDispatcher::registerBuiltins")
{
    const auto &n = protoext::builtinCommandNames();
    CHECK(n.size() == 18);
    for (const char *b : {"analyze", "play", "stop", "db", "send", "help", "engine"})
        CHECK(std::find(n.begin(), n.end(), b) != n.end());
}

TEST_CASE("PROTO-03: malformed TOML / DSL is rejected fail-closed")
{
    std::string err;
    // unterminated action string
    CHECK_FALSE(load("ptc_version=1\nextends=\"gomocup\"\n[[command]]\nname=\"c\"\ngroup=\"info\"\nsend=\"F\"\n"
                     "[[command.on_reply]]\npattern=\"R {v:int}\"\naction=\"\"\"\nlog(\"x\"\n\"\"\"\n", err).has_value());
    // unknown sink
    CHECK_FALSE(load("ptc_version=1\nextends=\"gomocup\"\n[[command]]\nname=\"c\"\ngroup=\"info\"\nsend=\"F\"\n"
                     "[[command.on_reply]]\npattern=\"R {v:int}\"\naction=\"\"\"\nhack(\"x\")\n\"\"\"\n", err).has_value());
    // template references unknown arg
    CHECK_FALSE(load("ptc_version=1\nextends=\"gomocup\"\n[[command]]\nname=\"c\"\ngroup=\"info\"\nsend=\"F {nope}\"\n", err).has_value());
    // action references unknown field
    CHECK_FALSE(load("ptc_version=1\nextends=\"gomocup\"\n[[command]]\nname=\"c\"\ngroup=\"info\"\nsend=\"F\"\n"
                     "[[command.on_reply]]\npattern=\"R {v:int}\"\naction=\"\"\"\nif w > 1 then\nlog(\"x\")\nend\n\"\"\"\n", err).has_value());
}
