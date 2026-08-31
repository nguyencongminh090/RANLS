# Planning — versioning + changelog

See [user_story.md](user_story.md) and [diagram/flow.md](diagram/flow.md).

## Resolved with the user (2026-08-30)

- **Scheme**: SemVer, `0.x` series. ✅
- **Location/format**: root `CHANGELOG.md`, "Keep a Changelog" style. ✅
- **Backfill**: yes — reconstruct Sprints 1–6 into changelog history. ✅
- **Release cadence**: sprint close = release + tag (maintainer's stated lean, adopted). ✅

## Open questions

1. **Starting version number for the backfill.** Options:
   - `0.1.0` = "first tracked state" (pre-Sprint-1), then `0.2.0` … `0.7.0` one per sprint —
     makes the history read naturally but invents 6 releases that were never cut.
   - Single `0.6.0` (or `0.1.0`) "initial public release" entry summarizing everything to date,
     then real per-sprint versions start at the *next* sprint.
   - **Recommendation:** one `## [0.1.0]` covering everything shipped through Sprint 6, tag the
     current `main` commit `v0.1.0`, then Sprint 7's close becomes `0.2.0`. Least fiction.
   - NEEDS USER DECISION.

2. **`configure_file` header name + location.** `build/generated/version.h`? `src/version.h.in` →
   generated into the build tree and added to the include path? Match whatever pattern
   `CMakeLists.txt` already uses for generated files (none today — pick the minimal one).

3. **CLI flag spelling.** Binary is `rapfi-gui` (CMake target); is there an installed name
   `yixinboard`? Support `--version` and `-v`? Does the app already parse argv at all, or is this
   the first flag? (Affects whether REL-02 adds an arg parser or just an early `argv` scan.)

4. **Changelog generation aid (optional, can defer).** A `scripts/changelog-since-tag.js` that
   lists merged `CODE`s + their fix-log summaries since the last `v*` tag, as a drafting aid the
   maintainer edits into prose. Nice-to-have; not blocking. Could be REL-03 later.

5. **GitHub Releases now or later.** The `github` skill already defers tagged Releases. Decision:
   REL-01 produces `CHANGELOG.md` + tags only; mirroring into GitHub Releases (with a built
   artifact) stays deferred until CI exists to produce the artifact.

## Implementation sequencing

Two `CODE`s under a new `REL` prefix (release/versioning):

- **REL-01 — `CHANGELOG.md` + release checklist + backfill.** Doc + process. Create
  `CHANGELOG.md` (backfilled per Q1), add a "Cutting a release" checklist to the `github` skill (or
  a `docs/release.md`), update `CLAUDE.md` "Sprint cadence" so sprint-close includes the release
  step. Tag `v0.1.0`. No code.
- **REL-02 — version string single-source.** Depends on Q1/Q2/Q3. Set CMake `VERSION` to match the
  tag, `configure_file` a `version.h`, wire it into `onAbout()` (replace `set_version("2.0")`) and
  a new `--version` early-exit path. Branch/PR flow, needs a test (CLI `--version` output asserts
  the CMake version).

REL-01 can ship first and independently. REL-02 follows once its open questions are closed.

## Next step

Resolve Q1 (and ideally Q2–Q3) with the user, then this is formalized — it already has
`docs/todo/REL-01`, `docs/todo/REL-02` + `TODO.md` Backlog lines and `docs/instruction/REL-01`,
`docs/instruction/REL-02` entries filed alongside this folder.
