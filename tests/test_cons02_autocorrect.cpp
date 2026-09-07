// CONS-02 — pure unit tests for the GTK-free AutoCorrect helpers that back the
// Engine Log command entry's syntax fix-ups on submit.
//
// Model/logic layer only (mirrors test_cons01_command_completer.cpp): includes
// just the header, no gtkmm, no display server. Pins the REAL behaviour of
// normalizeCase / didYouMean / missingBangFix / autoCorrect — not merely "the
// function exists".

#include "vendor/doctest.h"

#include "command/command_completer.h"

#include <optional>
#include <string>
#include <vector>

using command_completer::autoCorrect;
using command_completer::didYouMean;
using command_completer::levenshtein;
using command_completer::missingBangFix;
using command_completer::normalizeCase;

namespace {
// A representative slice of CommandDispatcher::registerBuiltins(), all lowercase
// as the real registry is, deliberately unsorted.
const std::vector<std::string> kNames = {
    "stop", "analyze", "about", "start", "send", "rule", "redo", "undo", "db", "help",
};
}  // namespace

TEST_CASE("CONS-02 levenshtein: case-insensitive edit distance")
{
    CHECK(levenshtein("analyze", "analyze") == 0);
    CHECK(levenshtein("ANALYZE", "analyze") == 0);
    CHECK(levenshtein("anlyze", "analyze") == 1);
    CHECK(levenshtein("analyse", "analyze") == 1);
    CHECK(levenshtein("undoo", "undo") == 1);
    CHECK(levenshtein("", "abc") == 3);
}

TEST_CASE("CONS-02 normalizeCase: registered spelling only when case differs")
{
    CHECK(normalizeCase("ANALYZE", kNames) == std::optional<std::string>{"analyze"});
    CHECK(normalizeCase("Analyze", kNames) == std::optional<std::string>{"analyze"});
    CHECK(normalizeCase("analyze", kNames) == std::nullopt);  // already exact
    CHECK(normalizeCase("anlyze", kNames) == std::nullopt);   // not a match at all
    CHECK(normalizeCase("", kNames) == std::nullopt);
}

TEST_CASE("CONS-02 didYouMean: Levenshtein <= 2 AND <= floor(len/2), best first")
{
    CHECK(didYouMean("anlyze", kNames) == std::vector<std::string>{"analyze"});
    CHECK(didYouMean("undoo", kNames) == std::vector<std::string>{"undo"});
    CHECK(didYouMean("xyzzy", kNames).empty());       // no name within threshold
    CHECK(didYouMean("analyze", kNames).empty());     // exact match is never a suggestion

    // "ab" — len 2, floor/2 = 1: only distance-1 names qualify. "about" is
    // distance 3, "db" is distance 1 → just "db".
    CHECK(didYouMean("ab", kNames) == std::vector<std::string>{"db"});

    // Tie handling: never more than 3 names. "redo"/"undo" both distance 1 from
    // "rndo"; capped and lexicographically ordered.
    auto tie = didYouMean("rndo", kNames);
    CHECK(tie.size() <= 3);
    CHECK(tie == std::vector<std::string>{"redo", "undo"});
}

TEST_CASE("CONS-02 missingBangFix: exact case-insensitive first token only")
{
    CHECK(missingBangFix("analyze 10", kNames) == std::optional<std::string>{"!analyze 10"});
    CHECK(missingBangFix("analyze", kNames) == std::optional<std::string>{"!analyze"});
    CHECK(missingBangFix("ANALYZE 10", kNames) == std::optional<std::string>{"!analyze 10"});
    CHECK(missingBangFix("  rule renju", kNames) == std::optional<std::string>{"  !rule renju"});

    // Not an exact match → left for the raw-line path, never fuzzy-corrected.
    CHECK(missingBangFix("analyse 10", kNames) == std::nullopt);
    CHECK(missingBangFix("foo", kNames) == std::nullopt);
    CHECK(missingBangFix("foo bar", kNames) == std::nullopt);

    // Bang already present (ASCII or fullwidth) → not this function's job (B3).
    CHECK(missingBangFix("!analyze", kNames) == std::nullopt);
    CHECK(missingBangFix(command_completer::fullwidthBang() + "analyze", kNames) == std::nullopt);
    CHECK(missingBangFix("", kNames) == std::nullopt);
}

TEST_CASE("CONS-02 autoCorrect: missingBangFix then normalizeCase, one rewrite")
{
    // B1: missing bang, already-registered spelling.
    CHECK(autoCorrect("analyze 10", kNames) == std::optional<std::string>{"!analyze 10"});
    // B1 + case: missing bang AND wrong case → single registered-spelling rewrite.
    CHECK(autoCorrect("ANALYZE 10", kNames) == std::optional<std::string>{"!analyze 10"});
    // B2: wrong case, bang present.
    CHECK(autoCorrect("!ANALYZE", kNames) == std::optional<std::string>{"!analyze"});
    // B3: fullwidth bang preserved, only the token normalised, no spurious change.
    CHECK(autoCorrect(command_completer::fullwidthBang() + "ANALYZE", kNames)
          == std::optional<std::string>{command_completer::fullwidthBang() + "analyze"});
    CHECK(autoCorrect(command_completer::fullwidthBang() + "analyze", kNames) == std::nullopt);

    // Nothing to do.
    CHECK(autoCorrect("!analyze 10", kNames) == std::nullopt);
    CHECK(autoCorrect("foo bar", kNames) == std::nullopt);
    CHECK(autoCorrect("analyse 10", kNames) == std::nullopt);  // near-miss left alone (B1 never guesses)
    CHECK(autoCorrect("", kNames) == std::nullopt);
}
