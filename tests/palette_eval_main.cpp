// PAL-01: benchmark driver for the command-palette search engine (Model A).
// Reads the shared dataset, runs every query (optionally only one split) and
// prints one TSV row per query: category, query, ranked ids, median latency in
// microseconds. scripts/palette_bench.py diffs this against the Whoosh baseline.
// Usage: ranls-palette-eval <entries.tsv> <queries.tsv> <lexicon.tsv> [dev|test|all] [repeats]

#include "command/palette_search.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
std::string slurp(const char *path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error(std::string("cannot open ") + path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
}  // namespace

int main(int argc, char **argv)
{
    if (argc < 4) {
        std::cerr << "usage: " << argv[0] << " entries.tsv queries.tsv lexicon.tsv [dev|test|all] [repeats]\n";
        return 2;
    }
    using clock = std::chrono::steady_clock;
    const std::string split   = argc > 4 ? argv[4] : "all";
    const int         repeats = argc > 5 ? std::max(1, std::atoi(argv[5])) : 5;
    try {
        const auto entries = palette_search::parseEntriesTsv(slurp(argv[1]));
        const auto lexicon = palette_search::Lexicon::parse(slurp(argv[3]));
        const auto t0      = clock::now();
        palette_search::Index index(entries, lexicon);
        const auto buildUs = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - t0).count();
        std::cout << "#build_us\t" << buildUs << "\n";

        std::istringstream in(slurp(argv[2]));
        std::string        line;
        std::getline(in, line);  // header
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            std::vector<std::string> f;
            std::stringstream        ls(line);
            for (std::string c; std::getline(ls, c, '\t');)
                f.push_back(c);
            if (line.empty() || f.size() < 2)
                continue;
            const std::string s = f.size() >= 4 ? f[3] : (f.size() == 3 ? f[2] : "dev");
            if (split != "all" && s != split)
                continue;
            std::vector<long long> us;
            std::vector<palette_search::Hit> hits;
            for (int r = 0; r < repeats; ++r) {
                const auto a = clock::now();
                hits         = index.search(f[1], 8);
                us.push_back(std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - a).count());
            }
            std::sort(us.begin(), us.end());
            std::cout << f[0] << '\t' << f[1] << '\t';
            for (std::size_t i = 0; i < hits.size(); ++i)
                std::cout << (i ? "," : "") << index.entry(hits[i].index).id;
            std::cout << '\t' << us[us.size() / 2] << '\n';
        }
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
