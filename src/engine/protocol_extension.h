#pragma once

// PROTO-03: Open Protocol Extension — the `.ptc` loader + mini-DSL interpreter.
//
// This translation unit is deliberately standalone: it depends on nothing from
// the engine transport (EngineProcess), the controller (EngineController), the
// command layer, GTK/gtkmm, or GameState. Everything here is pure data +
// parsing + a bounded interpreter, so it can be unit-tested with no display
// server and no subprocess, exactly like GomocupProtocol's own parsing.
//
// See docs/todo/PROTO-03-open-protocol-extension.md and
// features/protocol-extension/ for the full design (Q1-Q9, all resolved
// 2026-09-06).

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace protoext {

// ── Q5 load-time limits (fail-closed if any is exceeded) ────────────────────
inline constexpr int         kMaxIfDepth            = 4;
inline constexpr int         kMaxSinkCallsPerAction = 16;
inline constexpr int         kMaxCommandsPerFile    = 32;
inline constexpr int         kMaxArgsPerCommand     = 8;
inline constexpr std::size_t kMaxFileBytes          = 64 * 1024;

// Defense-in-depth interpreter bounds (a .ptc is semi-trusted, Q5 rationale).
inline constexpr int kMaxInterpDepth      = 32;
inline constexpr int kMaxInterpStatements = 256;

// ── Console arg + reply-capture types ──────────────────────────────────────
enum class ArgType { Int, Float, String, Coord, Repeat };
enum class FieldType { Int, Float, String, Coord };

struct ArgSpec {
    std::string name;
    ArgType     type = ArgType::String;
};

struct Capture {
    std::string name;
    FieldType   type = FieldType::String;
};

// ── send spec ──────────────────────────────────────────────────────────────
struct SendSpec {
    bool        isBlock = false;
    std::string singleLine;      ///< single-line template ("{arg}" interpolation)
    std::string open;
    std::string repeatSource;    ///< a declared repeat(...) arg name, or "$currentPath"
    std::string repeatTemplate;  ///< per-item template ({i.x}/{i.y}/{i.color})
    std::string close;
    std::string after;           ///< optional trailing line (e.g. the real query)
};

// ── mini-DSL AST ───────────────────────────────────────────────────────────
struct Value {
    enum class Kind { Number, String, Field } kind = Kind::Number;
    double      num = 0;
    std::string text;   ///< string literal contents, or field name, or number's raw text
};

struct Comparison {
    Value       lhs;
    std::string op;     ///< == != < > <= >=
    Value       rhs;
};

struct Expr; // fwd

struct SinkCall {
    std::string        sink;   ///< set_status_field | toast | log
    std::vector<Value> args;
};

struct Stmt {
    std::unique_ptr<SinkCall>     call;   ///< set when this is a sink call
    std::unique_ptr<struct IfStmt> ifs;   ///< set when this is an if/elif/else
};

struct Branch {
    std::unique_ptr<Expr>             cond;   ///< null for the final `else`
    std::vector<Stmt>                 body;
};

struct IfStmt {
    std::vector<Branch> branches;
};

struct Expr {
    enum class Kind { Comparison, Not, And, Or } kind = Kind::Comparison;
    Comparison            cmp;
    std::unique_ptr<Expr> a;
    std::unique_ptr<Expr> b;
};

struct Action {
    std::vector<Stmt> stmts;
};

// ── The resolved sink call handed to the UI layer ──────────────────────────
struct SinkAction {
    std::string              sink;   ///< "set_status_field" | "toast" | "log"
    std::vector<std::string> args;   ///< fully resolved to strings
};

using SinkEmitter = std::function<void(const SinkAction &)>;
using DebugLogger = std::function<void(const std::string &)>;

// ── One extension command ──────────────────────────────────────────────────
struct ExtensionCommand {
    std::string          name;
    std::string          group;
    std::vector<ArgSpec> args;
    SendSpec             send;

    bool                     hasReply = false;
    std::string              patternRaw;
    std::vector<std::string> patternLiterals; ///< captures.size() + 1 chunks
    std::vector<Capture>     captures;
    Action                   action;
};

// One board-path item, precomputed by the caller (keeps this TU free of Coord/
// GameState). `color` alternates 1/2 by index, same formula as
// GomocupProtocol::generateAnalyzeRequest.
struct PathItem {
    int x = 0;
    int y = 0;
    int color = 1;
};

// ── The loaded table ───────────────────────────────────────────────────────
class ExtensionTable {
public:
    /// Parse + validate a .ptc file's text. Returns std::nullopt and sets
    /// `error` on ANY problem (unparseable TOML, Q3 name collision, a Q5 limit
    /// exceeded, a bad template/DSL) — fail closed, the whole file is rejected.
    /// `reservedNames` is the console's full built-in command registry (Q3).
    static std::optional<ExtensionTable> loadFromString(
        const std::string              &text,
        const std::vector<std::string> &reservedNames,
        std::string                    &error);

    const std::vector<ExtensionCommand> &commands() const { return commands_; }
    std::vector<std::string>             commandNames() const;
    const ExtensionCommand              *find(const std::string &name) const;

    /// Build the wire line(s) for `name` given the console args and the live
    /// board path. Returns false + sets `error` on a bad arg count etc.
    bool generateSend(const std::string              &name,
                      const std::vector<std::string> &consoleArgs,
                      const std::vector<PathItem>    &currentPath,
                      std::vector<std::string>       &out,
                      std::string                    &error) const;

    /// Try to match `line` against each command's on_reply pattern, in
    /// declaration order. On the first match, run its action DSL, emitting
    /// resolved sink calls through `emit`. A pattern whose literals match but
    /// whose captures fail type conversion is treated as "no match" and a
    /// Debug line is sent to `debug` (never a throw / never UB) — PROTO-01
    /// precedent. Returns true iff a pattern matched and its action ran.
    bool matchReply(const std::string &line,
                    const SinkEmitter &emit,
                    const DebugLogger &debug) const;

private:
    std::vector<ExtensionCommand> commands_;
};

/// The canonical console built-in command registry (Q3) — mirrors
/// CommandDispatcher::registerBuiltins(). Single source shared by the loader's
/// callers and the collision tests.
const std::vector<std::string> &builtinCommandNames();

} // namespace protoext
