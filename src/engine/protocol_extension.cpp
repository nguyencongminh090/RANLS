#include "protocol_extension.h"

#include <cctype>
#include <cstdlib>
#include <set>

namespace protoext {

// ═══════════════════════════════════════════════════════════════════════════
// Canonical built-in registry (Q3) — mirrors CommandDispatcher::registerBuiltins.
// ═══════════════════════════════════════════════════════════════════════════
const std::vector<std::string> &builtinCommandNames()
{
    static const std::vector<std::string> kNames = {
        "analyze", "play", "stop", "getpos", "loadpos", "new", "pos", "redo",
        "rule", "start", "undo", "info", "db", "clear", "engine", "send",
        "about", "help",
    };
    return kNames;
}

namespace {

// ── small string helpers ───────────────────────────────────────────────────
std::string trim(const std::string &s)
{
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

bool parseStrictLong(const std::string &s, long &out)
{
    if (s.empty()) return false;
    try {
        size_t idx = 0;
        long v = std::stol(s, &idx);
        if (idx != s.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

bool parseStrictDouble(const std::string &s, double &out)
{
    if (s.empty()) return false;
    try {
        size_t idx = 0;
        double v = std::stod(s, &idx);
        if (idx != s.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

bool isCoordText(const std::string &s)
{
    auto comma = s.find(',');
    if (comma == std::string::npos) return false;
    long a = 0, b = 0;
    return parseStrictLong(s.substr(0, comma), a)
        && parseStrictLong(s.substr(comma + 1), b);
}

// ═══════════════════════════════════════════════════════════════════════════
// Minimal strict TOML subset parser.
//
// Supported: `#` comments, top-level `key = value`, `[[command]]` and
// `[[command.on_reply]]` array-of-tables, dotted keys (`send.open`), string
// values ("..." with \" \\ \n escapes), multi-line basic strings ("""..."""),
// single-line string arrays (["a", "b"]), and integer scalars. Anything else
// is a hard error (fail closed).
// ═══════════════════════════════════════════════════════════════════════════

struct RawTable {
    // key -> either a scalar string, or an array of strings.
    std::map<std::string, std::string>               scalars;
    std::map<std::string, std::vector<std::string>>   arrays;
    std::set<std::string>                             isArray;
};

struct RawCommand {
    RawTable                cmd;
    std::vector<RawTable>   onReply;
};

struct RawDoc {
    RawTable                root;
    std::vector<RawCommand> commands;
};

class TomlParser {
public:
    TomlParser(const std::string &text) : text_(text) {}

    bool parse(RawDoc &doc, std::string &err)
    {
        // Split into logical lines but keep track of raw offset so we can
        // consume a `"""` block that spans lines.
        std::vector<std::string> lines;
        {
            std::string cur;
            for (char c : text_) {
                if (c == '\n') { lines.push_back(cur); cur.clear(); }
                else if (c != '\r') cur.push_back(c);
            }
            lines.push_back(cur);
        }

        RawTable   *ctx     = &doc.root;
        RawCommand *curCmd   = nullptr;

        for (size_t li = 0; li < lines.size(); ++li) {
            std::string line = stripComment(lines[li]);
            std::string t    = trim(line);
            if (t.empty()) continue;

            if (t == "[[command]]") {
                doc.commands.push_back({});
                curCmd = &doc.commands.back();
                ctx    = &curCmd->cmd;
                continue;
            }
            if (t == "[[command.on_reply]]") {
                if (!curCmd) { err = "[[command.on_reply]] before any [[command]]"; return false; }
                curCmd->onReply.push_back({});
                ctx = &curCmd->onReply.back();
                continue;
            }
            if (!t.empty() && t.front() == '[') {
                err = "unsupported table header: " + t;
                return false;
            }

            auto eq = t.find('=');
            if (eq == std::string::npos) { err = "expected key = value: " + t; return false; }
            std::string key = trim(t.substr(0, eq));
            std::string rhs = trim(t.substr(eq + 1));
            if (key.empty()) { err = "empty key"; return false; }

            // Multi-line basic string.
            if (rhs.rfind("\"\"\"", 0) == 0) {
                std::string acc = rhs.substr(3);
                // Same-line close?
                auto closePos = acc.find("\"\"\"");
                bool closed = (closePos != std::string::npos);
                if (closed) {
                    acc = acc.substr(0, closePos);
                } else {
                    while (++li < lines.size()) {
                        std::string raw = lines[li];
                        auto cp = raw.find("\"\"\"");
                        if (cp != std::string::npos) {
                            acc += "\n" + raw.substr(0, cp);
                            closed = true;
                            break;
                        }
                        acc += "\n" + raw;
                    }
                }
                if (!closed) { err = "unterminated \"\"\" string for key " + key; return false; }
                // A leading newline immediately after the opening delimiter is
                // trimmed, per TOML.
                if (!acc.empty() && acc.front() == '\n') acc.erase(acc.begin());
                ctx->scalars[key] = acc;
                continue;
            }

            // Array (single line only).
            if (!rhs.empty() && rhs.front() == '[') {
                if (rhs.back() != ']') { err = "multi-line arrays not supported: " + key; return false; }
                std::string inner = rhs.substr(1, rhs.size() - 2);
                std::vector<std::string> items;
                if (!parseInlineArray(inner, items, err)) return false;
                ctx->arrays[key] = std::move(items);
                ctx->isArray.insert(key);
                continue;
            }

            // Basic string.
            if (!rhs.empty() && rhs.front() == '"') {
                std::string v;
                if (!parseBasicString(rhs, v, err)) return false;
                ctx->scalars[key] = v;
                continue;
            }

            // Bare scalar (integer / bool). Stored verbatim; callers validate.
            ctx->scalars[key] = rhs;
        }
        return true;
    }

private:
    std::string text_;

    static std::string stripComment(const std::string &line)
    {
        bool inStr = false;
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"' ) {
                // count preceding backslashes
                size_t bs = 0, j = i;
                while (j > 0 && line[j - 1] == '\\') { ++bs; --j; }
                if (bs % 2 == 0) inStr = !inStr;
            } else if (c == '#' && !inStr) {
                return line.substr(0, i);
            }
        }
        return line;
    }

    static bool parseBasicString(const std::string &raw, std::string &out, std::string &err)
    {
        if (raw.size() < 2 || raw.front() != '"') { err = "bad string: " + raw; return false; }
        std::string s = raw;
        // find closing unescaped quote
        size_t end = std::string::npos;
        for (size_t i = 1; i < s.size(); ++i) {
            if (s[i] == '\\') { ++i; continue; }
            if (s[i] == '"') { end = i; break; }
        }
        if (end == std::string::npos) { err = "unterminated string: " + raw; return false; }
        std::string trailing = trim(s.substr(end + 1));
        if (!trailing.empty()) { err = "trailing text after string: " + trailing; return false; }
        out.clear();
        for (size_t i = 1; i < end; ++i) {
            char c = s[i];
            if (c == '\\' && i + 1 < end) {
                char n = s[++i];
                switch (n) {
                    case 'n': out.push_back('\n'); break;
                    case 't': out.push_back('\t'); break;
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    default: out.push_back(n); break;
                }
            } else {
                out.push_back(c);
            }
        }
        return true;
    }

    static bool parseInlineArray(const std::string &inner, std::vector<std::string> &out, std::string &err)
    {
        // Comma-separated list of basic strings (the only array shape the
        // schema uses: `args = ["x:int", ...]`).
        std::string cur;
        bool inStr = false;
        std::string acc;
        auto flush = [&](std::string piece) -> bool {
            piece = trim(piece);
            if (piece.empty()) return true; // trailing comma / empty list
            std::string v;
            if (!parseBasicString(piece, v, err)) return false;
            out.push_back(v);
            return true;
        };
        size_t depth = 0;
        for (size_t i = 0; i < inner.size(); ++i) {
            char c = inner[i];
            if (c == '"') {
                size_t bs = 0, j = i;
                while (j > 0 && inner[j - 1] == '\\') { ++bs; --j; }
                if (bs % 2 == 0) inStr = !inStr;
                acc.push_back(c);
            } else if (c == ',' && !inStr && depth == 0) {
                if (!flush(acc)) return false;
                acc.clear();
            } else {
                acc.push_back(c);
            }
        }
        if (inStr) { err = "unterminated string in array"; return false; }
        if (!flush(acc)) return false;
        (void)cur;
        return true;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Template interpolation: replace every {token} with a looked-up value.
// Unknown tokens are a hard error at load time.
// ═══════════════════════════════════════════════════════════════════════════
bool collectTemplateTokens(const std::string &tmpl, std::vector<std::string> &tokens, std::string &err)
{
    for (size_t i = 0; i < tmpl.size(); ++i) {
        if (tmpl[i] == '{') {
            auto close = tmpl.find('}', i);
            if (close == std::string::npos) { err = "unterminated { in template: " + tmpl; return false; }
            tokens.push_back(tmpl.substr(i + 1, close - i - 1));
            i = close;
        } else if (tmpl[i] == '}') {
            err = "stray } in template: " + tmpl;
            return false;
        }
    }
    return true;
}

std::string interpolate(const std::string &tmpl, const std::map<std::string, std::string> &vars)
{
    std::string out;
    for (size_t i = 0; i < tmpl.size(); ++i) {
        if (tmpl[i] == '{') {
            auto close = tmpl.find('}', i);
            std::string tok = tmpl.substr(i + 1, close - i - 1);
            auto it = vars.find(tok);
            out += (it != vars.end()) ? it->second : std::string{};
            i = close;
        } else {
            out.push_back(tmpl[i]);
        }
    }
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════
// mini-DSL: tokenizer + recursive-descent parser (grammar exactly per Q2).
// ═══════════════════════════════════════════════════════════════════════════
struct Tok {
    enum class T { Ident, Number, String, Op, LParen, RParen, Comma, End } type;
    std::string text;
};

class Lexer {
public:
    Lexer(const std::string &s) : s_(s) {}

    bool lex(std::vector<Tok> &out, std::string &err)
    {
        size_t i = 0;
        while (i < s_.size()) {
            char c = s_[i];
            if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
            if (c == '(') { out.push_back({Tok::T::LParen, "("}); ++i; continue; }
            if (c == ')') { out.push_back({Tok::T::RParen, ")"}); ++i; continue; }
            if (c == ',') { out.push_back({Tok::T::Comma, ","}); ++i; continue; }
            if (c == '"') {
                std::string v;
                ++i;
                bool closed = false;
                while (i < s_.size()) {
                    char d = s_[i++];
                    if (d == '\\' && i < s_.size()) {
                        char n = s_[i++];
                        if (n == 'n') v.push_back('\n');
                        else v.push_back(n);
                    } else if (d == '"') { closed = true; break; }
                    else v.push_back(d);
                }
                if (!closed) { err = "unterminated string literal in action"; return false; }
                out.push_back({Tok::T::String, v});
                continue;
            }
            if (c == '=' || c == '!' || c == '<' || c == '>') {
                std::string op(1, c);
                if (i + 1 < s_.size() && s_[i + 1] == '=') { op.push_back('='); i += 2; }
                else i += 1;
                if (op == "=" || op == "!") { err = "invalid operator '" + op + "'"; return false; }
                out.push_back({Tok::T::Op, op});
                continue;
            }
            if (std::isdigit(static_cast<unsigned char>(c)) ||
                (c == '-' && i + 1 < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i + 1])))) {
                std::string n(1, c);
                ++i;
                while (i < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[i])) || s_[i] == '.')) {
                    n.push_back(s_[i++]);
                }
                out.push_back({Tok::T::Number, n});
                continue;
            }
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                std::string id(1, c);
                ++i;
                while (i < s_.size() &&
                       (std::isalnum(static_cast<unsigned char>(s_[i])) || s_[i] == '_' || s_[i] == '.')) {
                    id.push_back(s_[i++]);
                }
                out.push_back({Tok::T::Ident, id});
                continue;
            }
            err = std::string("unexpected character '") + c + "' in action";
            return false;
        }
        out.push_back({Tok::T::End, ""});
        return true;
    }

private:
    std::string s_;
};

class ActionParser {
public:
    ActionParser(std::vector<Tok> toks,
                 const std::set<std::string> &fields,
                 const std::set<std::string> &sinks)
        : toks_(std::move(toks)), fields_(fields), sinks_(sinks) {}

    bool parse(Action &out, std::string &err)
    {
        while (!at(Tok::T::End)) {
            Stmt st;
            if (!parseStmt(st, 0, err)) return false;
            out.stmts.push_back(std::move(st));
        }
        if (sinkCalls_ > kMaxSinkCallsPerAction) {
            err = "action exceeds max " + std::to_string(kMaxSinkCallsPerAction) + " sink calls";
            return false;
        }
        return true;
    }

private:
    std::vector<Tok>             toks_;
    size_t                       pos_ = 0;
    const std::set<std::string> &fields_;
    const std::set<std::string> &sinks_;
    int                          sinkCalls_ = 0;

    const Tok &cur() const { return toks_[pos_]; }
    bool at(Tok::T t) const { return cur().type == t; }
    bool atIdent(const char *s) const { return cur().type == Tok::T::Ident && cur().text == s; }
    void adv() { if (pos_ + 1 < toks_.size()) ++pos_; }

    bool expect(Tok::T t, const char *what, std::string &err)
    {
        if (!at(t)) { err = std::string("expected ") + what; return false; }
        adv();
        return true;
    }

    bool parseStmt(Stmt &st, int depth, std::string &err)
    {
        if (atIdent("if")) {
            if (depth + 1 > kMaxIfDepth) {
                err = "if-nesting exceeds max depth " + std::to_string(kMaxIfDepth);
                return false;
            }
            st.ifs = std::make_unique<IfStmt>();
            return parseIf(*st.ifs, depth + 1, err);
        }
        // sink call
        if (!at(Tok::T::Ident)) { err = "expected statement (sink call or 'if')"; return false; }
        std::string name = cur().text;
        if (!sinks_.count(name)) { err = "unknown sink '" + name + "'"; return false; }
        adv();
        if (!expect(Tok::T::LParen, "'(' after sink name", err)) return false;
        st.call = std::make_unique<SinkCall>();
        st.call->sink = name;
        if (!at(Tok::T::RParen)) {
            for (;;) {
                Value v;
                if (!parseValue(v, err)) return false;
                st.call->args.push_back(std::move(v));
                if (at(Tok::T::Comma)) { adv(); continue; }
                break;
            }
        }
        if (!expect(Tok::T::RParen, "')' after sink args", err)) return false;
        ++sinkCalls_;
        return validateSinkArity(*st.call, err);
    }

    bool parseIf(IfStmt &node, int depth, std::string &err)
    {
        adv(); // consume 'if'
        for (;;) {
            Branch br;
            br.cond = std::make_unique<Expr>();
            if (!parseExpr(*br.cond, err)) return false;
            if (!atIdent("then")) { err = "expected 'then'"; return false; }
            adv();
            while (!atIdent("elif") && !atIdent("else") && !atIdent("end")) {
                if (at(Tok::T::End)) { err = "unterminated 'if' (missing 'end')"; return false; }
                Stmt s;
                if (!parseStmt(s, depth, err)) return false;
                br.body.push_back(std::move(s));
            }
            node.branches.push_back(std::move(br));
            if (atIdent("elif")) { adv(); continue; }
            break;
        }
        if (atIdent("else")) {
            adv();
            Branch br; // cond == null
            while (!atIdent("end")) {
                if (at(Tok::T::End)) { err = "unterminated 'if' (missing 'end')"; return false; }
                Stmt s;
                if (!parseStmt(s, depth, err)) return false;
                br.body.push_back(std::move(s));
            }
            node.branches.push_back(std::move(br));
        }
        if (!atIdent("end")) { err = "expected 'end'"; return false; }
        adv();
        return true;
    }

    // expr := andExpr ('or' andExpr)*
    bool parseExpr(Expr &out, std::string &err)
    {
        Expr lhs;
        if (!parseAnd(lhs, err)) return false;
        while (atIdent("or")) {
            adv();
            auto rhs = std::make_unique<Expr>();
            if (!parseAnd(*rhs, err)) return false;
            Expr combined;
            combined.kind = Expr::Kind::Or;
            combined.a = std::make_unique<Expr>(std::move(lhs));
            combined.b = std::move(rhs);
            lhs = std::move(combined);
        }
        out = std::move(lhs);
        return true;
    }

    bool parseAnd(Expr &out, std::string &err)
    {
        Expr lhs;
        if (!parseNot(lhs, err)) return false;
        while (atIdent("and")) {
            adv();
            auto rhs = std::make_unique<Expr>();
            if (!parseNot(*rhs, err)) return false;
            Expr combined;
            combined.kind = Expr::Kind::And;
            combined.a = std::make_unique<Expr>(std::move(lhs));
            combined.b = std::move(rhs);
            lhs = std::move(combined);
        }
        out = std::move(lhs);
        return true;
    }

    bool parseNot(Expr &out, std::string &err)
    {
        if (atIdent("not")) {
            adv();
            out.kind = Expr::Kind::Not;
            out.a = std::make_unique<Expr>();
            return parseNot(*out.a, err);
        }
        return parsePrimary(out, err);
    }

    bool parsePrimary(Expr &out, std::string &err)
    {
        if (at(Tok::T::LParen)) {
            adv();
            if (!parseExpr(out, err)) return false;
            return expect(Tok::T::RParen, "')'", err);
        }
        // comparison := value CMP value
        out.kind = Expr::Kind::Comparison;
        if (!parseValue(out.cmp.lhs, err)) return false;
        if (!at(Tok::T::Op)) { err = "expected comparison operator"; return false; }
        out.cmp.op = cur().text;
        adv();
        return parseValue(out.cmp.rhs, err);
    }

    bool parseValue(Value &v, std::string &err)
    {
        if (at(Tok::T::Number)) {
            v.kind = Value::Kind::Number;
            v.text = cur().text;
            v.num  = std::strtod(cur().text.c_str(), nullptr);
            adv();
            return true;
        }
        if (at(Tok::T::String)) {
            v.kind = Value::Kind::String;
            v.text = cur().text;
            adv();
            return true;
        }
        if (at(Tok::T::Ident)) {
            // reserved keywords cannot be a value
            static const std::set<std::string> kw = {
                "if", "elif", "else", "then", "end", "and", "or", "not"};
            if (kw.count(cur().text)) { err = "unexpected keyword '" + cur().text + "'"; return false; }
            if (!fields_.count(cur().text)) {
                err = "unknown field '" + cur().text + "' (no such on_reply capture)";
                return false;
            }
            v.kind = Value::Kind::Field;
            v.text = cur().text;
            adv();
            return true;
        }
        err = "expected a value (number, string, or field name)";
        return false;
    }

    static bool validateSinkArity(const SinkCall &c, std::string &err)
    {
        int want = (c.sink == "set_status_field") ? 2 : 1;
        if (static_cast<int>(c.args.size()) != want) {
            err = c.sink + "() takes " + std::to_string(want) + " argument(s)";
            return false;
        }
        return true;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Interpreter — evaluate a parsed Action against a set of captured fields.
// ═══════════════════════════════════════════════════════════════════════════
struct FieldValue {
    std::string raw;
    double      num = 0;
    bool        isNum = false;
};

class Interp {
public:
    Interp(const std::map<std::string, FieldValue> &fields, const SinkEmitter &emit)
        : fields_(fields), emit_(emit) {}

    void run(const Action &a)
    {
        for (const auto &s : a.stmts) exec(s, 0);
    }

private:
    const std::map<std::string, FieldValue> &fields_;
    const SinkEmitter                       &emit_;
    int                                      stmtCount_ = 0;

    void exec(const Stmt &s, int depth)
    {
        if (depth > kMaxInterpDepth) return;
        if (++stmtCount_ > kMaxInterpStatements) return;
        if (s.call) {
            SinkAction act;
            act.sink = s.call->sink;
            for (const auto &v : s.call->args) act.args.push_back(resolve(v));
            if (emit_) emit_(act);
            return;
        }
        if (s.ifs) {
            for (const auto &br : s.ifs->branches) {
                if (!br.cond || evalExpr(*br.cond)) {
                    for (const auto &inner : br.body) exec(inner, depth + 1);
                    return;
                }
            }
        }
    }

    std::string resolve(const Value &v) const
    {
        switch (v.kind) {
            case Value::Kind::Number: return v.text;
            case Value::Kind::String: return v.text;
            case Value::Kind::Field: {
                auto it = fields_.find(v.text);
                return it == fields_.end() ? std::string{} : it->second.raw;
            }
        }
        return {};
    }

    bool evalExpr(const Expr &e) const
    {
        switch (e.kind) {
            case Expr::Kind::Not: return e.a ? !evalExpr(*e.a) : true;
            case Expr::Kind::And: return evalExpr(*e.a) && evalExpr(*e.b);
            case Expr::Kind::Or:  return evalExpr(*e.a) || evalExpr(*e.b);
            case Expr::Kind::Comparison: return evalCmp(e.cmp);
        }
        return false;
    }

    void valueOf(const Value &v, double &num, std::string &str, bool &isNum) const
    {
        if (v.kind == Value::Kind::Number) { num = v.num; isNum = true; str = v.text; return; }
        if (v.kind == Value::Kind::String) { str = v.text; isNum = false; return; }
        auto it = fields_.find(v.text);
        if (it == fields_.end()) { str = ""; isNum = false; return; }
        num = it->second.num;
        isNum = it->second.isNum;
        str = it->second.raw;
    }

    bool evalCmp(const Comparison &c) const
    {
        double ln = 0, rn = 0;
        std::string ls, rs;
        bool li = false, ri = false;
        valueOf(c.lhs, ln, ls, li);
        valueOf(c.rhs, rn, rs, ri);

        if (li && ri) {
            if (c.op == "==") return ln == rn;
            if (c.op == "!=") return ln != rn;
            if (c.op == "<")  return ln < rn;
            if (c.op == ">")  return ln > rn;
            if (c.op == "<=") return ln <= rn;
            if (c.op == ">=") return ln >= rn;
        }
        int cmp = ls.compare(rs);
        if (c.op == "==") return cmp == 0;
        if (c.op == "!=") return cmp != 0;
        if (c.op == "<")  return cmp < 0;
        if (c.op == ">")  return cmp > 0;
        if (c.op == "<=") return cmp <= 0;
        if (c.op == ">=") return cmp >= 0;
        return false;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Pattern compile + match.
// ═══════════════════════════════════════════════════════════════════════════
bool compilePattern(const std::string &raw,
                    std::vector<std::string> &literals,
                    std::vector<Capture> &captures,
                    std::string &err)
{
    std::string lit;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '{') {
            auto close = raw.find('}', i);
            if (close == std::string::npos) { err = "unterminated { in pattern"; return false; }
            std::string body = raw.substr(i + 1, close - i - 1);
            auto colon = body.find(':');
            if (colon == std::string::npos) { err = "pattern capture needs {name:type}: " + body; return false; }
            Capture cap;
            cap.name = trim(body.substr(0, colon));
            std::string ty = trim(body.substr(colon + 1));
            if (ty == "int") cap.type = FieldType::Int;
            else if (ty == "float") cap.type = FieldType::Float;
            else if (ty == "string") cap.type = FieldType::String;
            else if (ty == "coord") cap.type = FieldType::Coord;
            else { err = "unknown capture type '" + ty + "'"; return false; }
            if (cap.name.empty()) { err = "empty capture name"; return false; }
            literals.push_back(lit);
            lit.clear();
            // Reject two adjacent captures with no separating literal (ambiguous).
            if (!captures.empty() && literals.back().empty()) {
                err = "two adjacent captures with no separator in pattern";
                return false;
            }
            captures.push_back(cap);
            i = close;
        } else {
            lit.push_back(raw[i]);
        }
    }
    literals.push_back(lit);
    return true;
}

// Returns: 0 = no match, 1 = matched+converted OK, -1 = literals matched but a
// capture failed type conversion (caller logs Debug, treats as skip).
int matchPattern(const std::vector<std::string> &literals,
                 const std::vector<Capture> &captures,
                 const std::string &line,
                 std::map<std::string, FieldValue> &fields)
{
    std::string s = trim(line);
    size_t cursor = 0;

    // leading literal
    const std::string &first = literals.front();
    if (s.compare(0, first.size(), first) != 0) return 0;
    cursor = first.size();

    for (size_t c = 0; c < captures.size(); ++c) {
        const std::string &nextLit = literals[c + 1];
        std::string captured;
        if (c + 1 == captures.size() && nextLit.empty()) {
            captured = s.substr(cursor);
            cursor = s.size();
        } else {
            size_t at = s.find(nextLit, cursor);
            if (at == std::string::npos) return 0;
            captured = s.substr(cursor, at - cursor);
            cursor = at + nextLit.size();
        }
        captured = trim(captured);

        FieldValue fv;
        fv.raw = captured;
        switch (captures[c].type) {
            case FieldType::Int: {
                long v = 0;
                if (!parseStrictLong(captured, v)) return -1;
                fv.num = static_cast<double>(v);
                fv.isNum = true;
                break;
            }
            case FieldType::Float: {
                double v = 0;
                if (!parseStrictDouble(captured, v)) return -1;
                fv.num = v;
                fv.isNum = true;
                break;
            }
            case FieldType::Coord: {
                if (!isCoordText(captured)) return -1;
                fv.isNum = false;
                break;
            }
            case FieldType::String:
                if (captured.empty()) return -1;
                fv.isNum = false;
                break;
        }
        fields[captures[c].name] = std::move(fv);
    }

    // trailing literal must be present at the cursor (unless already consumed).
    if (!(captures.size() && literals.back().empty())) {
        const std::string &last = literals.back();
        if (s.compare(cursor, last.size(), last) != 0) return 0;
        cursor += last.size();
        if (trim(s.substr(cursor)) != "") return 0;
    }
    return 1;
}

// ═══════════════════════════════════════════════════════════════════════════
// Builder: RawDoc -> validated ExtensionTable
// ═══════════════════════════════════════════════════════════════════════════
bool parseArgSpec(const std::string &raw, ArgSpec &out, std::string &err)
{
    auto colon = raw.find(':');
    if (colon == std::string::npos) { err = "arg needs name:type — '" + raw + "'"; return false; }
    out.name = trim(raw.substr(0, colon));
    std::string ty = trim(raw.substr(colon + 1));
    if (out.name.empty()) { err = "empty arg name"; return false; }
    if (ty == "int") out.type = ArgType::Int;
    else if (ty == "float") out.type = ArgType::Float;
    else if (ty == "string") out.type = ArgType::String;
    else if (ty == "coord") out.type = ArgType::Coord;
    else if (ty.rfind("repeat(", 0) == 0 && ty.back() == ')') out.type = ArgType::Repeat;
    else { err = "unknown arg type '" + ty + "'"; return false; }
    return true;
}

} // namespace

// ── ExtensionTable public API ──────────────────────────────────────────────
std::vector<std::string> ExtensionTable::commandNames() const
{
    std::vector<std::string> n;
    n.reserve(commands_.size());
    for (const auto &c : commands_) n.push_back(c.name);
    return n;
}

const ExtensionCommand *ExtensionTable::find(const std::string &name) const
{
    for (const auto &c : commands_)
        if (c.name == name) return &c;
    return nullptr;
}

std::optional<ExtensionTable> ExtensionTable::loadFromString(
    const std::string              &text,
    const std::vector<std::string> &reservedNames,
    std::string                    &error)
{
    if (text.size() > kMaxFileBytes) {
        error = "PTC file exceeds " + std::to_string(kMaxFileBytes) + " bytes";
        return std::nullopt;
    }

    RawDoc doc;
    TomlParser parser(text);
    if (!parser.parse(doc, error)) return std::nullopt;

    // Top-level header.
    {
        auto it = doc.root.scalars.find("ptc_version");
        if (it == doc.root.scalars.end() || trim(it->second) != "1") {
            error = "ptc_version must be 1";
            return std::nullopt;
        }
        auto ex = doc.root.scalars.find("extends");
        if (ex == doc.root.scalars.end() || ex->second != "gomocup") {
            error = "extends must be \"gomocup\"";
            return std::nullopt;
        }
    }

    if (doc.commands.size() > static_cast<size_t>(kMaxCommandsPerFile)) {
        error = "more than " + std::to_string(kMaxCommandsPerFile) + " commands in file";
        return std::nullopt;
    }
    if (doc.commands.empty()) {
        error = "no [[command]] entries";
        return std::nullopt;
    }

    std::set<std::string> reserved(reservedNames.begin(), reservedNames.end());
    static const std::set<std::string> kGroups = {
        "board", "config", "database", "debug", "engine", "info"};

    ExtensionTable table;
    std::set<std::string> seenNames;

    for (auto &rc : doc.commands) {
        ExtensionCommand cmd;

        auto nameIt = rc.cmd.scalars.find("name");
        if (nameIt == rc.cmd.scalars.end() || trim(nameIt->second).empty()) {
            error = "command missing name";
            return std::nullopt;
        }
        cmd.name = trim(nameIt->second);
        for (char c : cmd.name) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                error = "command name '" + cmd.name + "' has invalid characters";
                return std::nullopt;
            }
        }
        if (reserved.count(cmd.name)) {
            error = "command '" + cmd.name + "' collides with a built-in console command";
            return std::nullopt;
        }
        if (seenNames.count(cmd.name)) {
            error = "duplicate command name '" + cmd.name + "' in file";
            return std::nullopt;
        }
        seenNames.insert(cmd.name);

        auto grpIt = rc.cmd.scalars.find("group");
        if (grpIt == rc.cmd.scalars.end() || !kGroups.count(grpIt->second)) {
            error = "command '" + cmd.name + "' has missing/invalid group (v1: "
                    "board|config|database|debug|engine|info — no 'analysis')";
            return std::nullopt;
        }
        cmd.group = grpIt->second;

        // args
        std::set<std::string> argNames;
        bool sawRepeat = false;
        if (rc.cmd.isArray.count("args")) {
            const auto &items = rc.cmd.arrays.at("args");
            if (items.size() > static_cast<size_t>(kMaxArgsPerCommand)) {
                error = "command '" + cmd.name + "' exceeds " +
                        std::to_string(kMaxArgsPerCommand) + " args";
                return std::nullopt;
            }
            for (size_t i = 0; i < items.size(); ++i) {
                ArgSpec a;
                if (!parseArgSpec(items[i], a, error)) return std::nullopt;
                if (a.type == ArgType::Repeat) {
                    if (sawRepeat) { error = "command '" + cmd.name + "' has >1 repeat(...) arg"; return std::nullopt; }
                    if (i + 1 != items.size()) { error = "repeat(...) arg must be last"; return std::nullopt; }
                    sawRepeat = true;
                }
                if (!argNames.insert(a.name).second) {
                    error = "duplicate arg name '" + a.name + "'";
                    return std::nullopt;
                }
                cmd.args.push_back(std::move(a));
            }
        }

        // send
        bool hasSingle = rc.cmd.scalars.count("send") > 0;
        bool hasOpen   = rc.cmd.scalars.count("send.open") > 0;
        bool hasRepeat = rc.cmd.scalars.count("send.repeat") > 0;
        bool hasClose  = rc.cmd.scalars.count("send.close") > 0;
        bool hasRepSrc = rc.cmd.scalars.count("send.repeat_source") > 0;
        bool hasAfter  = rc.cmd.scalars.count("send.after") > 0;
        if (hasSingle && (hasOpen || hasRepeat || hasClose || hasRepSrc || hasAfter)) {
            error = "command '" + cmd.name + "': mix of single-line and block send";
            return std::nullopt;
        }
        if (!hasSingle && !hasOpen) {
            error = "command '" + cmd.name + "' has no send";
            return std::nullopt;
        }

        auto validateTokens = [&](const std::string &tmpl, bool repeatCtx) -> bool {
            std::vector<std::string> toks;
            if (!collectTemplateTokens(tmpl, toks, error)) return false;
            for (const auto &t : toks) {
                if (repeatCtx && (t == "i.x" || t == "i.y" || t == "i.color")) continue;
                if (!argNames.count(t)) {
                    error = "command '" + cmd.name + "': template references unknown '{" + t + "}'";
                    return false;
                }
            }
            return true;
        };

        if (hasSingle) {
            cmd.send.isBlock = false;
            cmd.send.singleLine = rc.cmd.scalars.at("send");
            if (!validateTokens(cmd.send.singleLine, false)) return std::nullopt;
        } else {
            cmd.send.isBlock = true;
            cmd.send.open = rc.cmd.scalars.at("send.open");
            cmd.send.close = hasClose ? rc.cmd.scalars.at("send.close") : "";
            cmd.send.after = hasAfter ? rc.cmd.scalars.at("send.after") : "";
            if (!hasRepeat || !hasRepSrc) {
                error = "command '" + cmd.name + "': block send needs send.repeat and send.repeat_source";
                return std::nullopt;
            }
            cmd.send.repeatTemplate = rc.cmd.scalars.at("send.repeat");
            cmd.send.repeatSource   = trim(rc.cmd.scalars.at("send.repeat_source"));
            if (cmd.send.repeatSource != "$currentPath") {
                auto ai = argNames.find(cmd.send.repeatSource);
                if (ai == argNames.end()) {
                    error = "command '" + cmd.name + "': repeat_source '" + cmd.send.repeatSource +
                            "' is not a declared arg or $currentPath";
                    return std::nullopt;
                }
            }
            if (!validateTokens(cmd.send.repeatTemplate, true)) return std::nullopt;
            if (!validateTokens(cmd.send.open, false)) return std::nullopt;
            if (hasClose && !validateTokens(cmd.send.close, false)) return std::nullopt;
            if (hasAfter && !validateTokens(cmd.send.after, false)) return std::nullopt;
        }

        // on_reply (at most one, v1)
        if (rc.onReply.size() > 1) {
            error = "command '" + cmd.name + "' has more than one [[command.on_reply]]";
            return std::nullopt;
        }
        if (!rc.onReply.empty()) {
            const RawTable &rep = rc.onReply.front();
            auto pIt = rep.scalars.find("pattern");
            auto aIt = rep.scalars.find("action");
            if (pIt == rep.scalars.end() || aIt == rep.scalars.end()) {
                error = "command '" + cmd.name + "' on_reply needs pattern and action";
                return std::nullopt;
            }
            cmd.hasReply = true;
            cmd.patternRaw = pIt->second;
            if (!compilePattern(cmd.patternRaw, cmd.patternLiterals, cmd.captures, error)) {
                error = "command '" + cmd.name + "': " + error;
                return std::nullopt;
            }
            std::set<std::string> fieldNames;
            for (const auto &cap : cmd.captures) {
                if (!fieldNames.insert(cap.name).second) {
                    error = "command '" + cmd.name + "': duplicate capture '" + cap.name + "'";
                    return std::nullopt;
                }
            }
            static const std::set<std::string> kSinks = {"set_status_field", "toast", "log"};
            std::vector<Tok> toks;
            Lexer lex(aIt->second);
            if (!lex.lex(toks, error)) {
                error = "command '" + cmd.name + "' action: " + error;
                return std::nullopt;
            }
            ActionParser ap(std::move(toks), fieldNames, kSinks);
            if (!ap.parse(cmd.action, error)) {
                error = "command '" + cmd.name + "' action: " + error;
                return std::nullopt;
            }
        }

        table.commands_.push_back(std::move(cmd));
    }

    return std::optional<ExtensionTable>(std::move(table));
}

bool ExtensionTable::generateSend(const std::string              &name,
                                  const std::vector<std::string> &consoleArgs,
                                  const std::vector<PathItem>    &currentPath,
                                  std::vector<std::string>       &out,
                                  std::string                    &error) const
{
    const ExtensionCommand *cmd = find(name);
    if (!cmd) { error = "unknown extension command '" + name + "'"; return false; }

    // Bind console args positionally.
    std::map<std::string, std::string> scalarVars;
    std::vector<std::string>            repeatTokens;
    bool                                haveRepeatArg = false;
    std::string                         repeatArgName;

    size_t idx = 0;
    for (const auto &a : cmd->args) {
        if (a.type == ArgType::Repeat) {
            haveRepeatArg = true;
            repeatArgName = a.name;
            while (idx < consoleArgs.size()) repeatTokens.push_back(consoleArgs[idx++]);
            break;
        }
        if (idx >= consoleArgs.size()) {
            error = "command '" + name + "' expects arg '" + a.name + "'";
            return false;
        }
        const std::string &raw = consoleArgs[idx++];
        long lv = 0; double dv = 0;
        switch (a.type) {
            case ArgType::Int:   if (!parseStrictLong(raw, lv)) { error = "arg '" + a.name + "' must be int"; return false; } break;
            case ArgType::Float: if (!parseStrictDouble(raw, dv)) { error = "arg '" + a.name + "' must be float"; return false; } break;
            case ArgType::Coord: if (!isCoordText(raw)) { error = "arg '" + a.name + "' must be x,y"; return false; } break;
            default: break;
        }
        scalarVars[a.name] = raw;
    }
    if (!haveRepeatArg && idx < consoleArgs.size()) {
        error = "command '" + name + "' given too many args";
        return false;
    }

    // Resolve the repeat source into a list of PathItem.
    std::vector<PathItem> items;
    if (cmd->send.isBlock) {
        if (cmd->send.repeatSource == "$currentPath") {
            items = currentPath;
        } else {
            if (!haveRepeatArg || cmd->send.repeatSource != repeatArgName) {
                error = "command '" + name + "': repeat_source arg not provided";
                return false;
            }
            for (size_t i = 0; i < repeatTokens.size(); ++i) {
                const std::string &tok = repeatTokens[i];
                auto comma = tok.find(',');
                if (comma == std::string::npos) { error = "repeat item '" + tok + "' must be x,y"; return false; }
                long x = 0, y = 0;
                if (!parseStrictLong(tok.substr(0, comma), x) ||
                    !parseStrictLong(tok.substr(comma + 1), y)) {
                    error = "repeat item '" + tok + "' must be x,y";
                    return false;
                }
                PathItem it;
                it.x = static_cast<int>(x);
                it.y = static_cast<int>(y);
                it.color = (i % 2 == 0) ? 1 : 2;
                items.push_back(it);
            }
        }
    }

    out.clear();
    if (!cmd->send.isBlock) {
        out.push_back(interpolate(cmd->send.singleLine, scalarVars));
        return true;
    }

    out.push_back(interpolate(cmd->send.open, scalarVars));
    for (const auto &it : items) {
        std::map<std::string, std::string> v = scalarVars;
        v["i.x"] = std::to_string(it.x);
        v["i.y"] = std::to_string(it.y);
        v["i.color"] = std::to_string(it.color);
        out.push_back(interpolate(cmd->send.repeatTemplate, v));
    }
    if (!cmd->send.close.empty()) out.push_back(interpolate(cmd->send.close, scalarVars));
    if (!cmd->send.after.empty()) out.push_back(interpolate(cmd->send.after, scalarVars));
    return true;
}

bool ExtensionTable::matchReply(const std::string &line,
                                const SinkEmitter &emit,
                                const DebugLogger &debug) const
{
    for (const auto &cmd : commands_) {
        if (!cmd.hasReply) continue;
        std::map<std::string, FieldValue> fields;
        int r = matchPattern(cmd.patternLiterals, cmd.captures, line, fields);
        if (r == 0) continue;
        if (r < 0) {
            if (debug)
                debug("protocol_extension: reply matched '" + cmd.name +
                      "' pattern shape but a capture failed type conversion; skipped");
            return false;
        }
        Interp interp(fields, emit);
        interp.run(cmd.action);
        return true;
    }
    return false;
}

} // namespace protoext
