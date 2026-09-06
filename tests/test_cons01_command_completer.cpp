// CONS-01 — pure unit tests for the GTK-free command-name completer that
// backs the Engine Log command entry's AutoComplete UI.
//
// Model/logic layer only (mirrors test_engine_log_model.cpp): includes just
// the header, no gtkmm, no display server. Pins the REAL behaviour of
// matches / longestCommonPrefix / shouldSuggest / currentPrefix — not merely
// "the function exists".

#include "vendor/doctest.h"

#include "command/command_completer.h"

#include <string>
#include <vector>

using command_completer::currentPrefix;
using command_completer::longestCommonPrefix;
using command_completer::matches;
using command_completer::shouldSuggest;

namespace {
// A representative slice of CommandDispatcher::registerBuiltins(), deliberately
// given out of order and with mixed case to exercise sort + case-insensitivity.
const std::vector<std::string> kNames = {
    "stop", "analyze", "about", "start", "send", "rule", "redo", "db", "help",
};
}  // namespace

TEST_CASE("CONS-01 matches: prefix 'an' → only 'analyze', case-insensitive, sorted")
{
    CHECK(matches("an", kNames) == std::vector<std::string>{"analyze"});
    CHECK(matches("AN", kNames) == std::vector<std::string>{"analyze"});

    // 'a' shares three names, returned sorted ascending.
    CHECK(matches("a", kNames) == std::vector<std::string>{"about", "analyze"});

    // 's' → sorted: send, start, stop.
    CHECK(matches("s", kNames) == std::vector<std::string>{"send", "start", "stop"});

    // empty prefix matches everything, still sorted.
    CHECK(matches("", kNames).size() == kNames.size());
    CHECK(matches("", kNames).front() == "about");

    // no match.
    CHECK(matches("zzz", kNames).empty());
}

TEST_CASE("CONS-01 longestCommonPrefix: actual output")
{
    CHECK(longestCommonPrefix({"analyze"}) == "analyze");
    CHECK(longestCommonPrefix({"start", "stop"}) == "st");
    CHECK(longestCommonPrefix({"send", "start", "stop"}) == "s");
    CHECK(longestCommonPrefix({"about", "analyze"}) == "a");
    CHECK(longestCommonPrefix({}) == "");
    // casing of the first element is preserved; compare is case-insensitive.
    CHECK(longestCommonPrefix({"Analyze", "analytic"}) == "Analy");
}

TEST_CASE("CONS-01 shouldSuggest: truth table (planning Q2 / instruction Testing)")
{
    CHECK(shouldSuggest("!an", 3));               // caret at end of first token
    CHECK_FALSE(shouldSuggest("an", 2));          // no bang → never
    CHECK_FALSE(shouldSuggest("!analyze foo", 10));  // caret past the first token
    CHECK(shouldSuggest("!analyze foo", 5));      // caret still inside the name
    CHECK(shouldSuggest("  !a", 4));              // leading whitespace tolerated
    CHECK(shouldSuggest("!", 1));                 // bare bang, caret after it
    CHECK_FALSE(shouldSuggest("", 0));
    CHECK_FALSE(shouldSuggest("   ", 3));
    CHECK_FALSE(shouldSuggest("YXBOARD", 3));     // raw protocol line (R2/A6)
    // fullwidth '！' (U+FF01) is accepted like ASCII '!'.
    CHECK(shouldSuggest(command_completer::fullwidthBang() + "an", 3));
}

TEST_CASE("CONS-01 currentPrefix: fragment after the bang, empty when inactive")
{
    CHECK(currentPrefix("!an") == "an");
    CHECK(currentPrefix("  !analyze") == "analyze");
    CHECK(currentPrefix("!analyze foo") == "analyze");
    CHECK(currentPrefix("!") == "");
    CHECK(currentPrefix("analyze") == "");
    CHECK(currentPrefix(command_completer::fullwidthBang() + "ru") == "ru");
}
