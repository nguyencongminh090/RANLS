#include "game_graph_cbor.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace rdb {

namespace {

// ── encode ──────────────────────────────────────────────────────────────────
void putHead(std::string &o, int major, uint64_t val)
{
    const auto mb = static_cast<unsigned char>(major << 5);
    if (val < 24) {
        o.push_back(static_cast<char>(mb | val));
    } else if (val <= 0xFF) {
        o.push_back(static_cast<char>(mb | 24));
        o.push_back(static_cast<char>(val));
    } else if (val <= 0xFFFF) {
        o.push_back(static_cast<char>(mb | 25));
        o.push_back(static_cast<char>((val >> 8) & 0xFF));
        o.push_back(static_cast<char>(val & 0xFF));
    } else if (val <= 0xFFFFFFFFull) {
        o.push_back(static_cast<char>(mb | 26));
        for (int i = 3; i >= 0; --i)
            o.push_back(static_cast<char>((val >> (8 * i)) & 0xFF));
    } else {
        o.push_back(static_cast<char>(mb | 27));
        for (int i = 7; i >= 0; --i)
            o.push_back(static_cast<char>((val >> (8 * i)) & 0xFF));
    }
}

void putInt(std::string &o, int64_t v)
{
    if (v >= 0)
        putHead(o, 0, static_cast<uint64_t>(v));
    else
        putHead(o, 1, static_cast<uint64_t>(-1 - v));
}

void putText(std::string &o, std::string_view s)
{
    putHead(o, 3, s.size());
    o.append(s.data(), s.size());
}

void putDouble(std::string &o, double d)
{
    uint64_t bits = 0;
    std::memcpy(&bits, &d, sizeof(bits));
    o.push_back(static_cast<char>(0xFB));
    for (int i = 7; i >= 0; --i)
        o.push_back(static_cast<char>((bits >> (8 * i)) & 0xFF));
}

void putBool(std::string &o, bool b) { o.push_back(static_cast<char>(b ? 0xF5 : 0xF4)); }

void encodeMove(std::string &o, const Move &m)
{
    putHead(o, 4, 2);
    putInt(o, m.x);
    putInt(o, m.y);
}

void encodeAnalysis(std::string &o, const NodeAnalysis &a)
{
    std::string body;
    size_t      n = 0;
    if (a.winrate) { putText(body, "w"); putDouble(body, *a.winrate); ++n; }
    if (a.depth)   { putText(body, "d"); putInt(body, *a.depth); ++n; }
    if (a.nodes)   { putText(body, "n"); putInt(body, *a.nodes); ++n; }
    if (!a.evalText.empty()) { putText(body, "t"); putText(body, a.evalText); ++n; }
    if (!a.pv.empty()) {
        putText(body, "pv");
        putHead(body, 4, a.pv.size());
        for (const auto &m : a.pv)
            encodeMove(body, m);
        ++n;
    }
    if (a.engineRef)   { putText(body, "e");  putInt(body, *a.engineRef); ++n; }
    if (a.analyzedUtc) { putText(body, "ts"); putInt(body, *a.analyzedUtc); ++n; }
    putHead(o, 5, n);
    o.append(body);
}

void encodeNode(std::string &o, const GraphNode &node)
{
    std::string body;
    size_t      n = 0;
    if (node.hasParent) { putText(body, "p"); putHead(body, 0, node.parent); ++n; }
    if (node.move)      { putText(body, "m"); encodeMove(body, *node.move); ++n; }
    if (!node.comment.empty()) { putText(body, "c"); putText(body, node.comment); ++n; }
    if (!node.glyph.empty())   { putText(body, "g"); putText(body, node.glyph); ++n; }
    if (node.zobrist)  { putText(body, "z"); putHead(body, 0, *node.zobrist); ++n; }
    if (node.analysis) { putText(body, "a"); encodeAnalysis(body, *node.analysis); ++n; }
    putHead(o, 5, n);
    o.append(body);
}

// ── decode ──────────────────────────────────────────────────────────────────
constexpr int kMaxDepth = 64;

struct Head {
    int      major = 0;
    int      addl  = 0;   ///< raw additional-information nibble
    uint64_t val   = 0;
};

bool readHead(const unsigned char *&p, const unsigned char *end, Head &h)
{
    if (p >= end)
        return false;
    const unsigned char b = *p++;
    h.major = b >> 5;
    const int ai = b & 0x1F;
    h.addl = ai;
    if (ai < 24) {
        h.val = static_cast<uint64_t>(ai);
    } else if (ai == 24) {
        if (p >= end) return false;
        h.val = *p++;
    } else if (ai == 25) {
        if (end - p < 2) return false;
        h.val = (static_cast<uint64_t>(p[0]) << 8) | p[1];
        p += 2;
    } else if (ai == 26) {
        if (end - p < 4) return false;
        h.val = 0;
        for (int i = 0; i < 4; ++i) h.val = (h.val << 8) | p[i];
        p += 4;
    } else if (ai == 27) {
        if (end - p < 8) return false;
        h.val = 0;
        for (int i = 0; i < 8; ++i) h.val = (h.val << 8) | p[i];
        p += 8;
    } else {
        return false; // 28-30 reserved, 31 indefinite — unsupported
    }
    return true;
}

bool skipValue(const unsigned char *&p, const unsigned char *end, int depth)
{
    if (depth > kMaxDepth)
        return false;
    Head h;
    if (!readHead(p, end, h))
        return false;
    switch (h.major) {
    case 0:
    case 1:
        return true;
    case 2:
    case 3:
        if (static_cast<uint64_t>(end - p) < h.val)
            return false;
        p += h.val;
        return true;
    case 4:
        for (uint64_t i = 0; i < h.val; ++i)
            if (!skipValue(p, end, depth + 1))
                return false;
        return true;
    case 5:
        for (uint64_t i = 0; i < h.val; ++i) {
            if (!skipValue(p, end, depth + 1)) return false;
            if (!skipValue(p, end, depth + 1)) return false;
        }
        return true;
    case 7:
        // simple values / floats: payload already consumed by readHead
        return h.addl <= 27;
    default:
        return false;
    }
}

bool readUint(const unsigned char *&p, const unsigned char *end, uint64_t &out)
{
    Head h;
    if (!readHead(p, end, h) || h.major != 0)
        return false;
    out = h.val;
    return true;
}

bool readInt(const unsigned char *&p, const unsigned char *end, int64_t &out)
{
    Head h;
    if (!readHead(p, end, h))
        return false;
    if (h.major == 0) {
        if (h.val > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
            return false;
        out = static_cast<int64_t>(h.val);
        return true;
    }
    if (h.major == 1) {
        if (h.val > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
            return false;
        out = -1 - static_cast<int64_t>(h.val);
        return true;
    }
    return false;
}

bool readText(const unsigned char *&p, const unsigned char *end, std::string &out)
{
    Head h;
    if (!readHead(p, end, h) || h.major != 3)
        return false;
    if (static_cast<uint64_t>(end - p) < h.val)
        return false;
    out.assign(reinterpret_cast<const char *>(p), static_cast<size_t>(h.val));
    p += h.val;
    return true;
}

bool readNumber(const unsigned char *&p, const unsigned char *end, double &out)
{
    Head h;
    if (!readHead(p, end, h))
        return false;
    if (h.major == 0) { out = static_cast<double>(h.val); return true; }
    if (h.major == 1) { out = -1.0 - static_cast<double>(h.val); return true; }
    if (h.major == 7 && h.addl == 27) {
        std::memcpy(&out, &h.val, sizeof(out));
        return true;
    }
    if (h.major == 7 && h.addl == 26) {
        uint32_t bits = static_cast<uint32_t>(h.val);
        float    f;
        std::memcpy(&f, &bits, sizeof(f));
        out = static_cast<double>(f);
        return true;
    }
    return false;
}

bool expectHeader(const unsigned char *&p, const unsigned char *end, int major,
                  uint64_t &len)
{
    Head h;
    if (!readHead(p, end, h) || h.major != major)
        return false;
    len = h.val;
    return true;
}

bool readMove(const unsigned char *&p, const unsigned char *end, Move &m)
{
    uint64_t len = 0;
    if (!expectHeader(p, end, 4, len) || len != 2)
        return false;
    int64_t x = 0, y = 0;
    if (!readInt(p, end, x) || !readInt(p, end, y))
        return false;
    m.x = static_cast<int32_t>(x);
    m.y = static_cast<int32_t>(y);
    return true;
}

bool parseAnalysis(const unsigned char *&p, const unsigned char *end, NodeAnalysis &a)
{
    uint64_t n = 0;
    if (!expectHeader(p, end, 5, n))
        return false;
    for (uint64_t i = 0; i < n; ++i) {
        std::string key;
        if (!readText(p, end, key))
            return false;
        if (key == "w") {
            double w = 0;
            if (!readNumber(p, end, w))
                return false;
            if (w >= 0.0 && w <= 1.0)
                a.winrate = w; // outside [0,1] => absent
        } else if (key == "d") {
            int64_t v = 0;
            if (!readInt(p, end, v)) return false;
            a.depth = static_cast<int32_t>(v);
        } else if (key == "n") {
            int64_t v = 0;
            if (!readInt(p, end, v)) return false;
            a.nodes = v;
        } else if (key == "t") {
            if (!readText(p, end, a.evalText)) return false;
        } else if (key == "pv") {
            uint64_t m = 0;
            if (!expectHeader(p, end, 4, m)) return false;
            a.pv.clear();
            a.pv.reserve(static_cast<size_t>(m < 4096 ? m : 0));
            for (uint64_t j = 0; j < m; ++j) {
                Move mv;
                if (!readMove(p, end, mv)) return false;
                a.pv.push_back(mv);
            }
        } else if (key == "e") {
            uint64_t v = 0;
            if (!readUint(p, end, v)) return false;
            a.engineRef = static_cast<uint16_t>(v);
        } else if (key == "ts") {
            int64_t v = 0;
            if (!readInt(p, end, v)) return false;
            a.analyzedUtc = v;
        } else {
            if (!skipValue(p, end, 0)) return false;
        }
    }
    return true;
}

bool parseNode(const unsigned char *&p, const unsigned char *end, GraphNode &node)
{
    uint64_t n = 0;
    if (!expectHeader(p, end, 5, n))
        return false;
    for (uint64_t i = 0; i < n; ++i) {
        std::string key;
        if (!readText(p, end, key))
            return false;
        if (key == "p") {
            uint64_t v = 0;
            if (!readUint(p, end, v)) return false;
            node.parent = static_cast<uint32_t>(v);
            node.hasParent = true;
        } else if (key == "m") {
            Move m;
            if (!readMove(p, end, m)) return false;
            node.move = m;
        } else if (key == "c") {
            if (!readText(p, end, node.comment)) return false;
        } else if (key == "g") {
            if (!readText(p, end, node.glyph)) return false;
        } else if (key == "z") {
            uint64_t v = 0;
            if (!readUint(p, end, v)) return false;
            node.zobrist = v;
        } else if (key == "a") {
            NodeAnalysis a;
            if (!parseAnalysis(p, end, a)) return false;
            node.analysis = std::move(a);
        } else {
            if (!skipValue(p, end, 0)) return false;
        }
    }
    return true;
}

bool parseEngine(const unsigned char *&p, const unsigned char *end, EngineInfo &e)
{
    uint64_t n = 0;
    if (!expectHeader(p, end, 5, n))
        return false;
    for (uint64_t i = 0; i < n; ++i) {
        std::string key;
        if (!readText(p, end, key))
            return false;
        if (key == "id") {
            uint64_t v = 0;
            if (!readUint(p, end, v)) return false;
            e.id = static_cast<uint16_t>(v);
        } else if (key == "name") {
            if (!readText(p, end, e.name)) return false;
        } else if (key == "version") {
            if (!readText(p, end, e.version)) return false;
        } else if (key == "params") {
            if (!readText(p, end, e.params)) return false;
        } else {
            if (!skipValue(p, end, 0)) return false;
        }
    }
    return true;
}

bool parseSetupStone(const unsigned char *&p, const unsigned char *end, SetupStone &s)
{
    uint64_t len = 0;
    if (!expectHeader(p, end, 4, len) || len != 3)
        return false;
    int64_t x = 0, y = 0, c = 0;
    if (!readInt(p, end, x) || !readInt(p, end, y) || !readInt(p, end, c))
        return false;
    s.x = static_cast<int32_t>(x);
    s.y = static_cast<int32_t>(y);
    s.color = static_cast<uint8_t>(c);
    return true;
}

} // namespace

// ── public encode ───────────────────────────────────────────────────────────
std::string encodeCbor(const GameGraph &g)
{
    std::string body;
    size_t      n = 0;

    putText(body, "schema"); putHead(body, 0, g.schema); ++n;
    putText(body, "board");  putHead(body, 0, g.board);  ++n;
    putText(body, "rule");   putHead(body, 0, g.rule);   ++n;
    if (g.created)  { putText(body, "created");  putInt(body, *g.created);  ++n; }
    if (g.modified) { putText(body, "modified"); putInt(body, *g.modified); ++n; }
    if (!g.generator.empty()) { putText(body, "generator"); putText(body, g.generator); ++n; }
    if (!g.setup.empty()) {
        putText(body, "setup");
        putHead(body, 4, g.setup.size());
        for (const auto &s : g.setup) {
            putHead(body, 4, 3);
            putInt(body, s.x);
            putInt(body, s.y);
            putInt(body, s.color);
        }
        ++n;
    }
    if (!g.engines.empty()) {
        putText(body, "engines");
        putHead(body, 4, g.engines.size());
        for (const auto &e : g.engines) {
            putHead(body, 5, 4);
            putText(body, "id");      putHead(body, 0, e.id);
            putText(body, "name");    putText(body, e.name);
            putText(body, "version"); putText(body, e.version);
            putText(body, "params");  putText(body, e.params);
        }
        ++n;
    }
    putText(body, "nodes");
    putHead(body, 4, g.nodes.size());
    for (const auto &node : g.nodes)
        encodeNode(body, node);
    ++n;

    std::string out;
    putHead(out, 5, n);
    out.append(body);
    return out;
}

// ── public decode ───────────────────────────────────────────────────────────
std::optional<GameGraph> decodeCbor(std::string_view bytes)
{
    const auto *p   = reinterpret_cast<const unsigned char *>(bytes.data());
    const auto *end = p + bytes.size();

    uint64_t n = 0;
    if (!expectHeader(p, end, 5, n))
        return std::nullopt;

    GameGraph g;
    bool sawSchema = false, sawBoard = false, sawRule = false, sawNodes = false;

    for (uint64_t i = 0; i < n; ++i) {
        std::string key;
        if (!readText(p, end, key))
            return std::nullopt;

        if (key == "schema") {
            uint64_t v = 0;
            if (!readUint(p, end, v)) return std::nullopt;
            g.schema = static_cast<uint16_t>(v);
            sawSchema = true;
        } else if (key == "board") {
            uint64_t v = 0;
            if (!readUint(p, end, v)) return std::nullopt;
            g.board = static_cast<uint8_t>(v);
            sawBoard = true;
        } else if (key == "rule") {
            uint64_t v = 0;
            if (!readUint(p, end, v)) return std::nullopt;
            g.rule = static_cast<uint8_t>(v);
            sawRule = true;
        } else if (key == "created") {
            int64_t v = 0;
            if (!readInt(p, end, v)) return std::nullopt;
            g.created = v;
        } else if (key == "modified") {
            int64_t v = 0;
            if (!readInt(p, end, v)) return std::nullopt;
            g.modified = v;
        } else if (key == "generator") {
            if (!readText(p, end, g.generator)) return std::nullopt;
        } else if (key == "setup") {
            uint64_t m = 0;
            if (!expectHeader(p, end, 4, m)) return std::nullopt;
            for (uint64_t j = 0; j < m; ++j) {
                SetupStone s;
                if (!parseSetupStone(p, end, s)) return std::nullopt;
                g.setup.push_back(s);
            }
        } else if (key == "engines") {
            uint64_t m = 0;
            if (!expectHeader(p, end, 4, m)) return std::nullopt;
            for (uint64_t j = 0; j < m; ++j) {
                EngineInfo e;
                if (!parseEngine(p, end, e)) return std::nullopt;
                g.engines.push_back(std::move(e));
            }
        } else if (key == "nodes") {
            uint64_t m = 0;
            if (!expectHeader(p, end, 4, m)) return std::nullopt;
            g.nodes.clear();
            for (uint64_t j = 0; j < m; ++j) {
                GraphNode node;
                if (!parseNode(p, end, node)) return std::nullopt;
                g.nodes.push_back(std::move(node));
            }
            sawNodes = true;
        } else {
            if (!skipValue(p, end, 0)) return std::nullopt;
        }
    }

    if (!sawSchema || !sawBoard || !sawRule || !sawNodes)
        return std::nullopt;

    return g;
}

} // namespace rdb
