#!/usr/bin/env python3.13
"""Information-retrieval search over docs/audit/ using Whoosh (BM25F).

Usage:
  python3.13 scripts/audit-search.py "<natural-language query>" [-k N] [--all-status] [--scope audit|all]
  python3.13 scripts/audit-search.py --hook [--min-score S]   # UserPromptSubmit hook: JSON on stdin

Requires: `pip install whoosh` (Python 3.13; tested with Whoosh 3.51).

Chunking: a plain docs/audit/<date>-<slug>.md is one chunk; a monthly digest
docs/audit/<YYYY-MM>-log.md is split into one chunk per `## ` section.
Status: an entry's `**Status:** Superseded ...` line, or any entry's
`**Supersedes:** <ref>[, <ref>]` line, marks the referenced entry superseded (append-only: old
files are never edited). A ref is a file basename (no .md) or a substring of a digest heading.
Superseded hits are hidden unless --all-status. --scope all also indexes docs/todo, docs/fix-log,
docs/notes.

Query/index normalisation (applied identically to both sides): accents stripped, lower-cased,
tokens split on non-alphanumerics, hyphenated task codes also emitted joined (`anlz-05` ->
`anlz05`, matching test names like `test_anlz05_*`), Porter stemming, stop-words removed.
The query is plain text (no Whoosh query syntax). It is lexical: no translation, no embeddings.

The on-disk index lives in .cache/audit-index/<scope>/ (git-ignored) and is rebuilt automatically
when any source file's mtime/size changes.
"""
import argparse
import glob
import hashlib
import json
import os
import re
import sys
import unicodedata

try:
    from whoosh import index, scoring
    from whoosh.analysis import LowercaseFilter, RegexTokenizer, StemFilter, StopFilter
    from whoosh.fields import ID, STORED, TEXT, Schema
    from whoosh.qparser import MultifieldParser, OrGroup
except ImportError:
    sys.exit("whoosh is required: python3.13 -m pip install whoosh")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCHEMA_VERSION = "1"  # bump when analyzer/schema/chunking changes
STOP = frozenset(
    "a an and are as at be but by for from has have how i in is it its of on or that the this to was "
    "we what when where which who why will with you do does did not no if so than then there these "
    "they their them can could should would may might about into over under".split()
)
SCOPES = {
    "audit": [("docs/audit", "audit")],
    "all": [("docs/audit", "audit"), ("docs/todo", "todo"), ("docs/fix-log", "fix-log"), ("docs/notes", "notes")],
}
CODE_RE = re.compile(r"\b([A-Za-z]{2,6})-(\d{1,3})\b")

ANALYZER = RegexTokenizer(r"[a-z0-9]+") | LowercaseFilter() | StopFilter(stoplist=STOP, minsize=2) | StemFilter()


def prep(text: str) -> str:
    """Accent-fold + lower-case, and append joined forms of task codes (anlz-05 -> anlz05)."""
    t = unicodedata.normalize("NFD", text).replace("đ", "d").replace("Đ", "D")
    t = "".join(c for c in t if not unicodedata.combining(c)).lower()
    joined = {a + b for a, b in CODE_RE.findall(t)}
    return t + (" " + " ".join(sorted(joined)) if joined else "")


def tokens(text: str) -> list[str]:
    return [tok.text for tok in ANALYZER(prep(text))]


SCHEMA = Schema(
    cid=ID(stored=True, unique=True),
    kind=STORED,
    file=STORED,
    title=TEXT(stored=True, analyzer=ANALYZER, field_boost=3.0),
    body=TEXT(analyzer=ANALYZER),
    text=STORED,
)


def chunks_of(rel: str, kind: str) -> list[dict]:
    with open(os.path.join(ROOT, rel), encoding="utf8") as fh:
        text = fh.read()
    base = os.path.basename(rel)[:-3]
    if kind == "audit" and re.fullmatch(r"\d{4}-\d{2}-log", base):
        out = []
        for part in re.split(r"^(?=## )", text, flags=re.M):
            if part.startswith("## "):
                title = part.split("\n", 1)[0][3:].strip()
                out.append(dict(cid=f"{base}#{title}", kind=kind, file=rel, title=title, text=part))
        return out
    m = re.search(r"^#\s+(.+)$", text, re.M)
    return [dict(cid=base, kind=kind, file=rel, title=m.group(1) if m else base, text=text)]


def source_files(scope: str) -> list[tuple[str, str]]:
    files = []
    for d, kind in SCOPES[scope]:
        for f in sorted(glob.glob(os.path.join(ROOT, d, "*.md"))):
            if os.path.basename(f) != "README.md":
                files.append((os.path.relpath(f, ROOT), kind))
    return files


def signature(files: list[tuple[str, str]]) -> str:
    h = hashlib.sha1(SCHEMA_VERSION.encode())
    for rel, _ in files:
        st = os.stat(os.path.join(ROOT, rel))
        h.update(f"{rel}|{st.st_mtime_ns}|{st.st_size}".encode())
    return h.hexdigest()


def open_index(scope: str):
    files = source_files(scope)
    sig = signature(files)
    d = os.path.join(ROOT, ".cache", "audit-index", scope)
    sigfile = os.path.join(d, "signature.txt")
    if index.exists_in(d) and os.path.exists(sigfile) and open(sigfile).read() == sig:
        return index.open_dir(d)
    os.makedirs(d, exist_ok=True)
    ix = index.create_in(d, SCHEMA)
    w = ix.writer()
    for rel, kind in files:
        for c in chunks_of(rel, kind):
            w.add_document(cid=c["cid"], kind=c["kind"], file=c["file"], title=c["title"],
                           body=prep(c["title"] + "\n" + c["text"]), text=c["text"])
    w.commit()
    with open(sigfile, "w") as fh:
        fh.write(sig)
    return ix


def superseded_ids(docs: list[dict]) -> set[str]:
    refs, hit = [], set()
    for c in docs:
        m = re.search(r"^\*\*Supersedes:\*\*\s*(.+)$", c["text"], re.M)
        if m:
            refs += [r.strip().removesuffix(".md").replace("`", "") for r in m.group(1).split(",")]
        if re.search(r"^\*\*Status:\*\*\s*Superseded\b", c["text"], re.M | re.I):
            hit.add(c["cid"])
    for c in docs:
        for r in refs:
            if r and (c["cid"] == r or c["cid"].startswith(r + "#") or r in c["title"]):
                hit.add(c["cid"])
    return hit


def snippet(text: str, qtoks: set[str]) -> str:
    best, best_n = "", -1
    for line in text.split("\n"):
        if not line.strip() or line.startswith("#"):
            continue
        n = sum(1 for t in tokens(line) if t in qtoks)
        if n > best_n:
            best, best_n = line.strip(), n
    return best if len(best) <= 160 else best[:157] + "..."


def search(query: str, k: int = 5, all_status: bool = False, scope: str = "audit") -> list[dict]:
    ix = open_index(scope)
    qtoks = set(tokens(query))
    if not qtoks:
        return []
    with ix.searcher(weighting=scoring.BM25F()) as s:
        docs = [dict(f) for f in s.documents()]
        sup = superseded_ids(docs)
        parser = MultifieldParser(["title", "body"], ix.schema, group=OrGroup)
        res = s.search(parser.parse(" ".join(sorted(qtoks))), limit=max(k * 4, 20))
        hits = []
        for h in res:
            if h["cid"] in sup and not all_status:
                continue
            hits.append(dict(score=h.score, kind=h["kind"], file=h["file"], title=h["title"],
                             superseded=h["cid"] in sup, snippet=snippet(h["text"], qtoks)))
            if len(hits) == k:
                break
        return hits


def fmt(hits: list[dict]) -> str:
    return "\n".join(
        f"{i}. [{h['score']:.2f}] {h['kind']}: {h['title']}{'  (SUPERSEDED)' if h['superseded'] else ''}\n"
        f"   {h['file']}\n   > {h['snippet']}"
        for i, h in enumerate(hits, 1)
    )


def main() -> None:
    ap = argparse.ArgumentParser(description="BM25F search over docs/audit (Whoosh)")
    ap.add_argument("query", nargs="*")
    ap.add_argument("-k", type=int, default=5)
    ap.add_argument("--all-status", action="store_true")
    ap.add_argument("--scope", choices=SCOPES, default="audit")
    ap.add_argument("--hook", action="store_true", help="UserPromptSubmit hook: read JSON on stdin")
    ap.add_argument("--min-score", type=float, default=12.0, help="hook mode: minimum BM25F score to print")
    a = ap.parse_args()
    if a.hook:
        raw = sys.stdin.read()
        try:
            prompt = json.loads(raw).get("prompt", "")
        except (ValueError, AttributeError):
            prompt = raw
        hits = [h for h in search(prompt, k=3) if h["score"] >= a.min_score]
        if hits:
            print("Relevant audit entries (BM25F, active only):\n" + fmt(hits))
        return
    if not a.query:
        ap.error("query required")
    hits = search(" ".join(a.query), a.k, a.all_status, a.scope)
    print(fmt(hits) if hits else "No matching audit entries.")


if __name__ == "__main__":
    main()
