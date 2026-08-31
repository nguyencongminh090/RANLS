# 2026-08-30 — App versioning + version-history index

## Idea

YixinBoard is a shipped **application**, not a library — it deserves a version number and a
human-readable history of what changed between versions. Right now there is no version string
anywhere and the only "what changed" record is `docs/fix-log.md` (per-fix, not per-release) and the
sprint archives (per-sprint, internal framing).

Wanted:
- A **version index / changelog** — one entry per released version, newest first, saying what a
  user gets in that version.
- The app should **report its own version** (About dialog / `--version` / window title).

## How it fits existing process

- Ties directly to the deferred "Releases" item in the `github` skill (tagged `v<x.y.z>` releases
  with a distributable Linux build) and `docs/audit/2026-08-30-github-project-management.md`.
- The changelog is a *release* artifact — different granularity from `docs/fix-log.md` (every fix,
  internal) and `docs/sprint/archive/` (every sprint, internal). It aggregates those into
  user-facing lines at release boundaries.

## Open questions (resolve before promoting to a feature/backlog item)

1. **Versioning scheme**: SemVer (`MAJOR.MINOR.PATCH`)? Date-based (`2026.08`)? Given it's a
   GUI app with no API, SemVer's MAJOR/MINOR/PATCH contract is loose — maybe `0.x` until a
   "feature-complete" 1.0, then MINOR = feature, PATCH = fix bundle.
2. **Where does the changelog live**: `CHANGELOG.md` at repo root (the conventional place, tools
   and GitHub Releases understand it) vs. `docs/changelog.md` + `docs/changelog/<version>.md`
   (matches this repo's index+detail convention). Leaning root `CHANGELOG.md` since it's
   user-facing and standard, with the "Keep a Changelog" format.
3. **How is an entry produced**: hand-written at release time by skimming the fix-log rows and
   sprint archive since the last tag? Semi-automated (a script that lists merged `CODE`s since the
   last `v*` tag)? Fully manual is fine to start.
4. **What triggers a release / version bump**: every sprint close? Every N fixes? Ad-hoc when
   there's something worth shipping? (Current cadence is continuous, so "sprint close" is the
   natural hook.)
5. **Where the app reads its version from**: a single source (CMake `project(... VERSION)` →
   generated header) so the binary, `--version`, and About dialog can't disagree. Check what the
   build system (`build.sh` / CMake) already exposes.
6. **Retroactive history**: do we backfill versions for the Sprint 1–6 work already shipped, or
   start the changelog at the first tagged release and treat everything before as "0.1.0 —
   initial"? Leaning the latter.
7. **GitHub Releases**: mirror each `CHANGELOG.md` entry into a GitHub Release attached to the
   `v<x.y.z>` tag (with the built artifact), or keep `CHANGELOG.md` as the only record for now?

## Next step — DONE

Promoted 2026-08-30 to `features/versioning-and-changelog/` (scheme = SemVer 0.x, location = root
`CHANGELOG.md`, backfill = yes, cadence = sprint close — all confirmed with the user). Formalized as
`REL-01` (changelog + release checklist + backfill) and `REL-02` (version-string single-source) in
`TODO.md` Backlog, with `docs/instruction/REL-0{1,2}` entries. One open question remains for pickup:
the exact starting version number (`features/versioning-and-changelog/planning.md` Q1).
