#include "command.h"

#include <algorithm>
#include <cctype>
#include <sstream>

static std::string trimLeft(std::string_view s)
{
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
        ++i;
    return std::string(s.substr(i));
}

static std::string toLowerCopy(std::string_view s)
{
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Simple shell-like tokenizer:
// - splits on whitespace
// - supports double quotes and backslash escapes inside quotes
static bool tokenize(std::string_view s, std::vector<std::string> &out, std::string &err)
{
    out.clear();
    std::string cur;
    bool inQuotes = false;

    auto flush = [&]() {
        if (!cur.empty()) {
            out.push_back(cur);
            cur.clear();
        }
    };

    for (size_t i = 0; i < s.size(); ++i) {
        char ch = s[i];

        if (inQuotes) {
            if (ch == '\\') {
                if (i + 1 < s.size()) {
                    cur.push_back(s[i + 1]);
                    ++i;
                } else {
                    err = "Dangling escape in quotes";
                    return false;
                }
            } else if (ch == '"') {
                inQuotes = false;
            } else {
                cur.push_back(ch);
            }
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(ch))) {
            flush();
            continue;
        }
        if (ch == '"') {
            inQuotes = true;
            continue;
        }
        cur.push_back(ch);
    }

    if (inQuotes) {
        err = "Unterminated quote";
        return false;
    }
    flush();
    return true;
}

CommandParseResult parseCommandLine(std::string_view line)
{
    CommandParseResult res;
    res.cmd.raw = std::string(line);

    // Trim whitespace-only.
    bool anyNonSpace = false;
    for (char c : line) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            anyNonSpace = true;
            break;
        }
    }
    if (!anyNonSpace) {
        res.ok = false;
        res.error = "Empty command";
        return res;
    }

    std::vector<std::string> tokens;
    std::string err;
    if (!tokenize(line, tokens, err)) {
        res.ok = false;
        res.error = err;
        return res;
    }
    if (tokens.empty()) {
        res.ok = false;
        res.error = "Empty command";
        return res;
    }

    res.cmd.name = toLowerCopy(tokens[0]);
    res.cmd.args.assign(tokens.begin() + 1, tokens.end());

    // Build tail as raw remainder after the first token occurrence.
    // This is used for commands like: send <raw line>
    size_t pos = line.find(tokens[0]);
    if (pos != std::string_view::npos) {
        std::string_view rest = line.substr(pos + tokens[0].size());
        res.cmd.tail = trimLeft(rest);
    }

    res.ok = true;
    return res;
}

