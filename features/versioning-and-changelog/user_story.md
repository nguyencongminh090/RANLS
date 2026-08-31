# Versioning + changelog — user story

Promoted from [docs/notes/2026-08-30-versioning-and-changelog.md](../../docs/notes/2026-08-30-versioning-and-changelog.md).
See also [planning.md](planning.md) and [diagram/flow.md](diagram/flow.md).

## Actors

- **User** — runs YixinBoard, wants to know which version they have and what changed.
- **Maintainer** (solo dev) — cuts releases, writes the changelog, tags the repo.
- **Contributor** (future) — reads the changelog to see what shipped, references it in an issue.

## User stories

1. *As a user*, I can see the exact version of YixinBoard I'm running (Help → About, and
   `yixinboard --version` / `rapfi-gui --version` on the CLI), so I can report bugs against a
   specific build.
2. *As a user*, I can open `CHANGELOG.md` (in the repo / release page) and read, newest first,
   what changed in each released version in plain language.
3. *As a maintainer*, at a release boundary I can produce the next changelog entry by aggregating
   the `docs/fix-log.md` rows and sprint-archive items since the last tag, without hand-tracking
   them separately during the sprint.
4. *As a maintainer*, the version string has exactly one source of truth; the binary, the About
   dialog, the CLI flag, and the git tag can never disagree.
5. *As a maintainer*, cutting a release is a short checklist (bump, finalize changelog section,
   commit, tag, push tag), not an ad-hoc ritual.

## Rules

- **Scheme: SemVer, `0.x` series.** `0.MINOR.PATCH` while pre-1.0: MINOR = new user-visible
  feature, PATCH = fix/polish bundle. `1.0.0` is declared only when the app is deemed
  feature-complete — out of scope here.
- **`CHANGELOG.md` at the repo root**, "Keep a Changelog" format (`## [0.2.0] - 2026-09-15`,
  grouped `Added / Changed / Fixed / Removed`), newest version first, an `## [Unreleased]` section
  at the top that accumulates between releases.
- **Changelog entries are user-facing**, not internal. They summarize impact ("Board size and rule
  now persist between launches"), not `CODE`s or file names — though an entry *may* cite the `CODE`
  in parentheses for traceability.
- **Release cadence = sprint close.** When a sprint is archived, its shipped work becomes the next
  `CHANGELOG.md` version and a `v0.x.y` tag. A mid-sprint hotfix may cut a PATCH release out of
  band.
- **Backfill:** reconstruct entries for the work already shipped (Sprints 1–6) from
  `docs/sprint/archive/` + `docs/fix-log.md`, ending at the current state as the first tagged
  release. Exact starting version number decided in [planning.md](planning.md) Q1.
- The `docs/fix-log.md` / `docs/sprint/` convention is unchanged — `CHANGELOG.md` is a new
  *derived* artifact, not a replacement.

## Hard constraints

- Single version source: CMake `project(... VERSION x.y.z)` → generated header consumed by the
  About dialog and the CLI flag. No second literal anywhere. (Today CMake says `1.0.0`, the About
  dialog hardcodes `"2.0"` — both must be replaced, not kept.)
- No new build dependency. Changelog generation, if scripted, is a Node script under `scripts/`
  like the existing tracking-file scripts — not a new toolchain.
- `--version` must work without initializing GTK / opening a window.
- Doc-only portions (`CHANGELOG.md`, release checklist) may land on `main` directly; the CMake +
  C++ wiring goes through the normal branch/PR flow.
