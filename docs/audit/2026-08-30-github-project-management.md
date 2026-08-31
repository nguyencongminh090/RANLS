# 2026-08-30 — GitHub project management: development model + repo strategy

## Prompt

User review found `CLAUDE.md` documents a local tracking discipline but says nothing about how
GitHub is used. Two questions: (1) which development model is this project applying? (2) which
GitHub management strategy best fits? Constraints set with the user: local tracking files remain
the single source of truth; solo developer, low ceremony; scope = branch/PR workflow + Issues/
labels/milestones + a Projects board; CI and Releases out of scope for now.

## Question 1 — development model in use

**Answer: personal Scrumban with a V-Model documentation/traceability spine.**

Evidence:
- `CLAUDE.md` names it "Agile Scrum + tracking-file discipline" and defines a four-stage stage-gated
  flow (brainstorm → design → backlog/sprint → fix log).
- Scrum artifacts are real and maintained: `docs/sprint/current.md` (goal, dates, committed CODEs,
  points), `docs/sprint/burndown.md`, `docs/sprint/archive/sprint-N.md`; git history shows Sprints
  1–6 with explicit "Close Sprint N, open Sprint N+1" commits and per-sprint "lessons" (retro).
- But flow is continuous / single-piece, not textbook Scrum: several sprints are open-ended
  ("Dates: 2026-08-30 to — (open — no fixed end date set yet)"); items are pulled straight into the
  Active section mid-sprint (UI-04/05, UX-06, UI-06, STATE-04 all "committed straight into Sprint
  6"); effective WIP is ~1 (`/implement-task` dispatches one bounded subagent at a time). That is
  Kanban-style flow wearing Scrum vocabulary — i.e. Scrumban.
- Traceability resembles the V-Model: each item is traced `docs/notes/` → `features/<slug>/` →
  `docs/todo/<CODE>` + `docs/instruction/<CODE>` → code → `docs/fix-log/` or `docs/audit/`, with
  canonical status verbs (DONE/FIXED/CLOSED/VERIFIED), a mandatory regression test per fix, and an
  automated index/detail sync check (`check-tracking-sync.js`, wired as a `Stop` hook).
- Team: solo (`git` user `nguyencongminh090` / `nguyenminh`, one human contributor), AI-agent-
  assisted (`worktree-agent-*` branches, `AGENTS.md` model tiers).

Divergence from the written process: `CLAUDE.md` frames sprints as time-boxed, but practice treats
them as scope-boxed continuous batches. This is working fine for a solo dev and is not flagged as a
problem — just noted so the label ("Scrumban") matches reality.

## Question 2 — GitHub strategy chosen

Trunk-based development with short-lived per-`CODE` branches and squash-merge PRs. Local `docs/`
tree stays canonical; GitHub is review + history + a board only. Full rules and commands are in the
new `github` skill; the headline decisions:

- Branch `<code>/<slug>` off `main`, one per `CODE`; squash-merge so `main` is linear and
  one-commit-per-task (matches existing history style `Fix UI-02: …`); delete branch after merge.
- `main` requires a PR but **no required reviewer** — a review gate would just block the solo dev.
  Doc-only tracking edits keep going straight to `main` (already allowed by `CLAUDE.md`).
- PR title `<CODE>: <summary>`; PR body links `docs/todo/<CODE>-*.md` (a local detail file is not a
  GitHub Issue, so no "Closes #" unless an Issue genuinely exists).
- Issues stay thin: intake for externally reported bugs (triaged into a `CODE`) or items wanted on
  the board. Not a mirror of `docs/todo/`.
- Labels mirror `TODO.md` prefixes (`area:*`) + `sprint:<N>`; milestone = `Sprint <N>`, closed at
  archive; Projects board (Backlog/Active/Done) mirrors `TODO.md` + `docs/sprint/current.md`,
  hand-synced only at sprint planning and sprint close.
- No automated GitHub↔local sync — deliberate; the local-side checkers are enough and the board is
  a convenience view.

Branch model: **single trunk, no permanent `dev`/`develop`**. With WIP ≈ 1 and no Releases, a second
long-lived branch is just `main` delayed. A big/risky feature (multi-`CODE`, cross-layer, or one you
want to stabilise while fixes keep shipping) uses a `feat/<slug>` integration branch — the code
counterpart of `features/<slug>/`: sub-task branches PR into it, it rebases on `main`, one PR merges
it back (non-squash, keep per-`CODE` commits). Demo/stable builds are tagged (or a short
`release/vX.Y` branch) off `main`, never off the feature branch. Escalate to a permanent `develop`
only when a second regular contributor joins with concurrent features.

Deferred (file as backlog when wanted): GitHub Actions CI (build + `doctest` + `rapfi-gui-ui-tests`
on PRs, then a required check) and tagged Releases with a distributable Linux build.

## Action

- Added a "## GitHub project management" section to `CLAUDE.md` (after "Audit", before "Agent
  management") — states the model and the strategy headlines, points to the skill for commands.
- Added `.claude/skills/github/SKILL.md` — branch naming, PR lifecycle for one `CODE`, commit
  format, squash-merge/protection, label + milestone setup commands, board mapping, reconciliation.
- This audit row.

No code changed; doc-only, committed on `main` per `CLAUDE.md`.
