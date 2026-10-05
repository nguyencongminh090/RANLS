---
paths:
  - "TODO.md"
  - "docs/todo/**"
  - "instruction.md"
  - "docs/instruction/**"
  - "docs/fix-log.md"
  - "docs/fix-log/**"
  - "docs/audit.md"
  - "docs/audit/**"
---

# Tracking-file layout: index + detail files (query and append)

`TODO.md`, `instruction.md`, `docs/fix-log.md`, and `docs/audit.md` are lightweight **indexes** —
structural headings plus one line per item linking to a detail file. Read the index first (it's
small); actual content lives one level down, one file per item:

- `docs/todo/<CODE>-<slug>.md` — `CODE` is the item's code as used in `TODO.md` (e.g. `WALL-01`).
- `docs/instruction/<CODE>-<slug>.md` — `CODE` matches the `instruction.md` heading.
- `docs/fix-log/<YYYY-MM-DD>-<slug>.md` — one file per fix-log row, named by date + opening-words slug.
- `docs/audit/<YYYY-MM-DD>-<slug>.md` — one file per **long** audit row (> ~40 lines), same naming.
- `docs/audit/<YYYY-MM>-log.md` — monthly digest: **short** audit entries (≤ ~40 lines) as `## <date> — <title>` sections.

**Query:** grep/scan the index for the item code/keyword, then `Read` only the matched detail
file(s) instead of the whole original file.
**Append:** write one new detail file, then add one new line/row to the matching index. Never re-open
or edit an existing detail file's content when adding unrelated history.

## Index/detail sync: status markers move together, in one edit

`TODO.md` marks a finished item with a leading `✅` on its index line; the matching
`docs/todo/<CODE>-<slug>.md` marks the same fact in its own completion marker. These describe the
same fact and must never be updated one without the other:

- **Finishing a task = one edit that touches both files**, same turn. Neither file is "the real
  one" — an index line without a matching detail-file marker, or vice versa, is a drift bug.
- **Canonical marker format for entries**: `**Status:** ✅ <verb>`, where `<verb>` is one of `DONE`
  (implemented), `FIXED`, `CLOSED` (won't-fix/not-a-bug), or `VERIFIED` (measured/confirmed) — pick
  whichever matches what actually happened, then add a short summary + test/verification notes on
  the same or following lines.
- **Before telling the user a task is/isn't done, read both.** If they disagree, bring the index in
  line with the detail file (it carries the evidence — test output, verification notes).

### Automated enforcement

`scripts/check-tracking-sync.js` is wired as a `Stop` hook in `.claude/settings.local.json`: it blocks
ending a turn if a newly-✅-marked `TODO.md` item's detail file has no completion marker (only checks
drift introduced since the last commit). To audit the whole backlog manually:
```
node scripts/check-tracking-sync.js --full
```

`instruction.md` ↔ `docs/instruction/*.md` has no done/not-done marker (execution guidance, not a
status tracker) — nothing to sync there beyond "read the matching entry before implementing."

## `docs/fix-log.md` and `docs/audit.md`: append-only, every row timestamped

- A new fix/audit entry = one new detail file (`## Prompt` / `## Action` / `## Decision` /
  `## Summary` as relevant) + one new row in the matching index. Never edit, reword, reorder, or
  delete an existing row/file. A wrong past entry gets a new correcting entry, not a rewrite.
- Every index row's timestamp/date matches its detail file's heading, set to the real wall-clock
  time the entry is *written*, not when the underlying event happened if those differ.

## Audit entries: Status, Supersedes, and search

- Every audit entry (own file or digest section) starts with `**Status:** Active` (or
  `Superseded by <ref>`). Old entries are never edited to change status: a correcting entry adds
  `**Supersedes:** <ref>[, <ref>]` (file basename without `.md`, or a substring of a digest heading).
- **Before reasoning about a design/protocol/build decision, search the audit trail:**
  `python3.13 scripts/audit-search.py "<plain-English need>" [-k N] [--scope all] [--all-status]`
  (Whoosh BM25F; needs `python3.13 -m pip install -r scripts/requirements.txt`; hides superseded entries by default; English queries only — no translation; index cached in git-ignored `.cache/`).
- Auto-lookup is wired: a `UserPromptSubmit` hook in `.claude/settings.local.json` runs
  `audit-search.py --hook` and prints the top active hits (BM25F score ≥ `--min-score`, default 12) as
  context. It prints nothing below the threshold, and errors are swallowed (`2>/dev/null || true`), so
  silence is normal — and English-only, so Vietnamese prompts rarely score.

## Templates

Copy from `docs/templates/` when writing a new entry: `todo.md`, `fix-log.md`, `audit.md`. Each carries
**Scope**, **Reasoning** (why + rejected alternatives) and **Knowledge** (cite `docs/knowledge/` notes,
skills, prior audits by path). `Status:` is one line from the closed set
`OPEN | ACTIVE | DONE | FIXED | CLOSED | VERIFIED | SUPERSEDED`; evidence goes in the fix-log.
`scripts/check-task-structure.js` lints **open** todo files (Status OPEN/ACTIVE) for the required
sections; closed legacy files are not re-linted (append-only history).

## Task size: classify first, then pick the artifact

Classify every task **before** writing any tracking artifact. Not every task earns an audit entry —
an audit trail that records everything is noisy and hurts retrieval (`scripts/audit-search.py`).

| Size | Test (all must hold) | Artifact |
|---|---|---|
| **S — small** | ≤ ~2 files · one layer · no design decision · trivially reversible · no protocol/format/toolchain/process change | A short **temporary note**: `docs/notes/<date>-<slug>.md` from `docs/templates/note.md`. No todo, no audit. |
| **M — medium** | Several files or a real choice between approaches, but reversible and inside one layer | `docs/todo/` + (optional) `docs/instruction/`; audit **only if** a non-obvious decision was made (then a short digest section). |
| **L — large** | Crosses layers · changes protocol / on-disk format / toolchain / process · hard to reverse · reverses a prior audit · needs a design | `features/<slug>/` → `docs/todo/` + `docs/instruction/` → **audit entry** (own file if > ~40 lines). |

Rules:
- **When unsure, pick the larger size.** Under-recording a decision costs more than an extra file.
- **A bug fix always gets a fix-log entry regardless of size** (`CLAUDE.md`); size only decides whether
  it *also* needs a todo/audit. A small bug fix = short fix-log (Prompt / Root cause / Fix /
  Verification), nothing more.
- **Notes expire.** At sprint close, each `S` note is promoted (grew into M/L → todo/audit/fix-log) or
  deleted. A note about a decision that turns out to matter is promoted, not left to rot.
- A note's size can be re-classified upward at any time; never downward after code ships behind a
  decision others depend on.
- Notes are searchable: `python3.13 scripts/audit-search.py "<need>" --scope all`.
