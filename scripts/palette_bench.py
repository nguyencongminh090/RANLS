#!/usr/bin/env python3.13
"""PAL-01 benchmark: our C++ palette search (Model A) vs a Whoosh baseline (Model B).

Both models run over ONE shared dataset (tests/data/palette/{entries,queries}.tsv) and the
same lexicon (src/resources/palette_lexicon.tsv). Dev-only tool: not a runtime dependency,
not a CI gate.

  python3.13 scripts/palette_bench.py [--build-dir build_cmd] [--split test|dev|all] [--repeats 20]
                                      [--out report.md]

Do NOT run with `-I`: Whoosh normally lives in the user site-packages, which -I hides.

Models
  A        ranls-palette-eval (C++): fold + phrase max-match + stopwords + light stemming,
           synonym/fuzzy/prefix expansion, BM25F, boosts, trigram fallback.
  B        Whoosh with the SAME pre-tokenizer (fold, phrase max-match, stopwords) and the same
           synonym lexicon; Whoosh Porter stemming, BM25F, FuzzyTerm / Prefix on the last token.
           No exact-title / diacritic / trigram boosts: those are what the comparison measures.
  B-stock  Stock Whoosh: StemmingAnalyzer on raw text, OR query, no folding/lexicon (shows
           what the Vietnamese-aware pipeline adds).
Metrics: recall@1/3/8, MRR, nDCG@8 (binary gain), no-match specificity for `neg`, paired
bootstrap on MRR (A - B), build time, p50/p95 latency (in-process, warm), peak RSS.
Latency methodology: A is timed inside the C++ process (steady_clock around search()), B around
Searcher.search(); neither includes process start-up or index build.
"""
import argparse
import collections
import math
import os
import random
import re
import resource
import statistics
import subprocess
import sys
import time
import unicodedata

from whoosh import scoring
from whoosh.analysis import (LowercaseFilter, RegexTokenizer, SpaceSeparatedTokenizer,
                             StemFilter, StemmingAnalyzer)
from whoosh.fields import ID, TEXT, Schema
from whoosh.filedb.filestore import RamStorage
from whoosh.qparser import MultifieldParser, OrGroup
from whoosh.query import And, FuzzyTerm, Or, Prefix, Term, Every

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "tests", "data", "palette")
LEXICON = os.path.join(ROOT, "src", "resources", "palette_lexicon.tsv")


def fold(s):
    s = unicodedata.normalize("NFD", s)
    s = "".join(c for c in s if unicodedata.category(c) != "Mn")
    return s.replace("đ", "d").replace("Đ", "D").lower()


def tokenize(s):
    return re.findall(r"[a-z0-9]+", fold(s))


def read_tsv(path):
    with open(path, encoding="utf-8") as f:
        rows = [l.rstrip("\n").split("\t") for l in f if l.strip()]
    return rows[0], rows[1:]


def load_lexicon(path):
    syn, stop, phrases = [], set(), set()
    for l in open(path, encoding="utf-8"):
        l = l.strip()
        if not l or l.startswith("#") or "\t" not in l:
            continue
        k, rest = l.split("\t", 1)
        items = [x.strip() for x in rest.split(";") if x.strip()]
        if k == "syn":
            syn.append(items)
        elif k == "stop":
            stop.update(tokenize(" ".join(items)))
        elif k == "phrase":
            phrases.update(items)
    for g in syn:
        phrases.update(m for m in g if len(tokenize(m)) > 1)
    ph = {"_".join(tokenize(p)) for p in phrases if len(tokenize(p)) > 1}
    return syn, stop, ph


class Prep:
    """The shared pre-tokenizer: fold, tokenize, phrase max-match, stopwords."""

    def __init__(self, stop, phrases):
        self.stop, self.phrases = stop, phrases
        self.maxlen = max([p.count("_") + 1 for p in phrases] + [1])

    def __call__(self, text, is_query=False):
        toks, out, i = tokenize(text), [], 0
        while i < len(toks):
            for n in range(min(self.maxlen, len(toks) - i), 1, -1):
                j = "_".join(toks[i:i + n])
                if j in self.phrases:
                    out.append((j, 1.0))
                    out.extend((t, 0.5) for t in toks[i:i + n])
                    i += n
                    break
            else:
                if not (toks[i] in self.stop and not (is_query and len(toks) == 1)):
                    out.append((toks[i], 1.0))
                i += 1
        return out


# --------------------------------------------------------------------------- Model B
class WhooshModel:
    def __init__(self, entries, prep, syn, expand=True, name="B"):
        self.entries, self.prep, self.expand, self.name = entries, prep, expand, name
        self.an = SpaceSeparatedTokenizer() | LowercaseFilter() | StemFilter()
        schema = Schema(id=ID(stored=True), kind=ID,
                        title=TEXT(analyzer=self.an, field_boost=3.0),
                        kw=TEXT(analyzer=self.an, field_boost=2.0),
                        grp=TEXT(analyzer=self.an, field_boost=1.0))
        t0 = time.perf_counter()
        ix = RamStorage().create_index(schema)
        w = ix.writer()
        for e in entries:
            f = lambda s: " ".join(t for t, _ in prep(s))
            w.add_document(id=e[0], kind=e[1], title=f(e[2] + " " + e[3]),
                           kw=f(e[4] + ";" + e[5]), grp=f(e[6]))
        w.commit()
        self.build_s = time.perf_counter() - t0
        self.searcher = ix.searcher(weighting=scoring.BM25F(B=0.75, K1=1.2))
        self.groups = collections.defaultdict(list)
        if expand:
            for g in syn:
                members = [self.stem("_".join(tokenize(m))) for m in g]
                for m in members:
                    self.groups[m].extend(x for x in members if x != m)

    def stem(self, tok):
        return next((t.text for t in self.an(tok)), tok)

    def known(self, term):
        return any(self.searcher.doc_frequency(f, term) for f in ("title", "kw", "grp"))

    def search(self, q, limit=8):
        q = q.strip()
        bang = q.startswith("!")
        if bang:
            q = q[1:].strip()
            if not q:
                return [d["id"] for d in self.searcher.search(Term("kind", "cmd"), limit=limit)]
        terms = {}
        def put(t, w):
            terms[t] = max(terms.get(t, 0), w)
        base = self.prep(q, True)
        for t, w in base:
            put(self.stem(t), w)
        if self.expand:
            for t, w in list(terms.items()):
                for m in self.groups.get(t, []):
                    put(m, 0.7 * w)
        raw = tokenize(q)
        fields = ("title", "kw", "grp")
        clauses = [Term(f, t, boost=w) for t, w in terms.items() for f in fields]
        if self.expand:
            for r in raw:
                s = self.stem(r)
                if len(r) >= 4 and not self.known(s):
                    md = 2 if len(r) >= 8 else 1
                    clauses += [FuzzyTerm(f, s, maxdist=md, prefixlength=0, boost=0.5) for f in fields]
            if raw and not q.endswith(" ") and len(raw[-1]) >= 2:
                clauses += [Prefix(f, raw[-1], boost=0.6) for f in fields]
        if not clauses:
            return []
        query = Or(clauses)
        if bang:
            query = And([Term("kind", "cmd"), query])
        return [d["id"] for d in self.searcher.search(query, limit=limit)]


class StockWhoosh:
    name = "B-stock"

    def __init__(self, entries):
        an = StemmingAnalyzer()
        schema = Schema(id=ID(stored=True), kind=ID,
                        title=TEXT(analyzer=an, field_boost=3.0),
                        kw=TEXT(analyzer=an, field_boost=2.0), grp=TEXT(analyzer=an))
        t0 = time.perf_counter()
        ix = RamStorage().create_index(schema)
        w = ix.writer()
        for e in entries:
            w.add_document(id=e[0], kind=e[1], title=e[2] + " " + e[3],
                           kw=e[4] + ";" + e[5], grp=e[6])
        w.commit()
        self.build_s = time.perf_counter() - t0
        self.searcher = ix.searcher(weighting=scoring.BM25F(B=0.75, K1=1.2))
        self.parser = MultifieldParser(["title", "kw", "grp"], ix.schema, group=OrGroup)

    def search(self, q, limit=8):
        q = q.strip().lstrip("!").strip() if q.strip().startswith("!") else q
        try:
            return [d["id"] for d in self.searcher.search(self.parser.parse(q), limit=limit)]
        except Exception:
            return []


# --------------------------------------------------------------------------- metrics
def rank_of(ranked, rel):
    for i, x in enumerate(ranked):
        if x in rel:
            return i + 1
    return 0


def scores(ranked, rel):
    if not rel:  # neg: success = nothing returned
        ok = 0 if ranked else 1
        return dict(r1=ok, r3=ok, r8=ok, mrr=ok, ndcg=ok)
    rk = rank_of(ranked, rel)
    dcg = sum(1 / math.log2(i + 2) for i, x in enumerate(ranked[:8]) if x in rel)
    idcg = sum(1 / math.log2(i + 2) for i in range(min(len(rel), 8)))
    return dict(r1=int(rk == 1), r3=int(0 < rk <= 3), r8=int(0 < rk <= 8),
                mrr=1 / rk if rk else 0, ndcg=dcg / idcg)


def bootstrap(a, b, n=4000, seed=7):
    rng = random.Random(seed)
    d = [x - y for x, y in zip(a, b)]
    mean = sum(d) / len(d)
    boots = sorted(sum(rng.choice(d) for _ in d) / len(d) for _ in range(n))
    return mean, boots[int(0.025 * n)], boots[int(0.975 * n)]


def pct(v, p):
    v = sorted(v)
    return v[min(len(v) - 1, int(p * len(v)))]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default=os.path.join(ROOT, "build_cmd"))
    ap.add_argument("--split", default="test", choices=["dev", "test", "all"])
    ap.add_argument("--repeats", type=int, default=20)
    ap.add_argument("--out")
    args = ap.parse_args()

    _, entries = read_tsv(os.path.join(DATA, "entries.tsv"))
    _, queries = read_tsv(os.path.join(DATA, "queries.tsv"))
    queries = [q for q in queries if args.split == "all" or q[3] == args.split]
    syn, stop, phrases = load_lexicon(LEXICON)
    prep = Prep(stop, phrases)

    exe = os.path.join(args.build_dir, "tests", "ranls-palette-eval")
    out = subprocess.run([exe, os.path.join(DATA, "entries.tsv"), os.path.join(DATA, "queries.tsv"),
                          LEXICON, args.split, str(args.repeats)],
                         capture_output=True, text=True, check=True).stdout.splitlines()
    cpp_rss = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    a_build_us = int(out[0].split("\t")[1])
    a_rows = {(r[0], r[1]): (r[2].split(",") if r[2] else [], int(r[3]))
              for r in (l.split("\t") for l in out[1:])}

    models = {"B": WhooshModel(entries, prep, syn), "B-stock": StockWhoosh(entries)}
    res = {"A": [], "B": [], "B-stock": []}
    lat = {"A": [], "B": [], "B-stock": []}
    rows = []
    for c, qtext, rel_s, split in queries:
        rel = set(x for x in rel_s.split(",") if x)
        ranked_a, us_a = a_rows[(c, qtext)]
        per = {"A": ranked_a}
        lat["A"].append(us_a)
        for name, m in models.items():
            ts, r = [], []
            for _ in range(args.repeats):
                t = time.perf_counter()
                r = m.search(qtext)
                ts.append((time.perf_counter() - t) * 1e6)
            lat[name].append(statistics.median(ts))
            per[name] = r
        for name in res:
            res[name].append((c, scores(per[name], rel)))
        rows.append((c, qtext, rel, per))

    L = []
    L.append(f"# Palette benchmark — split `{args.split}`, {len(queries)} queries, {len(entries)} entries\n")
    L.append("A = ranls C++ `palette_search`; B = Whoosh + shared pre-tokenizer/lexicon; B-stock = stock Whoosh.\n")
    cats = sorted({c for c, *_ in queries})
    for metric, label in (("mrr", "MRR"), ("r1", "recall@1"), ("r3", "recall@3"), ("r8", "recall@8"), ("ndcg", "nDCG@8")):
        L.append(f"\n## {label}\n\n| category | n | A | B | B-stock |\n|---|---|---|---|---|")
        for c in cats + ["ALL"]:
            vals = {n: [s[metric] for cc, s in res[n] if c == "ALL" or cc == c] for n in res}
            L.append(f"| {c} | {len(vals['A'])} | " + " | ".join(f"{sum(vals[n])/len(vals[n]):.3f}" for n in ("A", "B", "B-stock")) + " |")
    ma, lo, hi = bootstrap([s["mrr"] for _, s in res["A"]], [s["mrr"] for _, s in res["B"]])
    L.append(f"\n**Paired bootstrap, MRR A − B:** {ma:+.3f} (95% CI {lo:+.3f} … {hi:+.3f})"
             + (" — CI excludes 0" if lo > 0 or hi < 0 else " — not significant"))
    L.append("\n## Performance\n\n| | A (C++) | B (Whoosh) | B-stock |\n|---|---|---|---|")
    L.append(f"| index build | {a_build_us/1000:.2f} ms | {models['B'].build_s*1000:.2f} ms | {models['B-stock'].build_s*1000:.2f} ms |")
    for p, lab in ((0.5, "p50"), (0.95, "p95")):
        L.append(f"| query latency {lab} | {pct(lat['A'], p):.0f} µs | {pct(lat['B'], p):.0f} µs | {pct(lat['B-stock'], p):.0f} µs |")
    L.append(f"| peak RSS | {cpp_rss/1024:.1f} MB (C++ process) | {resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/1024:.1f} MB (whole Python run) | — |")
    L.append("\n## Disagreements (A vs B top-1)\n")
    n = 0
    for c, qtext, rel, per in rows:
        a1 = per["A"][0] if per["A"] else None
        b1 = per["B"][0] if per["B"] else None
        if a1 != b1:
            n += 1
            tag = lambda x: ("✓" if (x in rel if rel else x is None) else "✗")
            L.append(f"- `{c}` “{qtext}” — A: {a1} {tag(a1)} · B: {b1} {tag(b1)} · expected {sorted(rel) or 'none'}")
    if not n:
        L.append("(none)")
    text = "\n".join(L) + "\n"
    print(text)
    if args.out:
        open(args.out, "w", encoding="utf-8").write(text)


if __name__ == "__main__":
    main()
