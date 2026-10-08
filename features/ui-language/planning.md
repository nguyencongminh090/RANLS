# UI Language — planning

See [user_story.md](user_story.md). **Status: RESOLVED 2026-10-08 (see "Resolution" below) — filed as I18N-01..04.** Size **L** (cross-layer, adds a mechanism/toolchain choice, touches every UI file) → resolve
these with the user, then `docs/todo/I18N-*` + `docs/instruction/`.

## Facts (checked 2026-10-08)

- No i18n today: no gettext/libintl/`bindtextdomain` anywhere in `CMakeLists.txt` or `src/`; UI strings are
  literals in `src/ui/*.cpp`, `src/main_window.cpp`, and the `Gio::Menu` model in `buildMenuBar()`.
- Targets include Linux, MSYS2 and MSVC (PORT-01/03), where `msgfmt`/libintl add friction.
- Bundled resources already go through a GResource (`src/resources/ranls.gresource.xml`: style.css, icons,
  `palette_lexicon.tsv`), and Settings persist in a flat `key=value` file (`SettingsStorage`).
- Palette already carries Vietnamese titles (`settings_registry.h`, `palette_catalog.cpp`) — a second,
  unrelated copy of some strings; this feature should let those become catalog entries.

## Options for the mechanism

| | A. gettext (`.po`/`.mo`, `_()`) | B. Own TSV catalogs in GResource (`tr("English text")`) |
|---|---|---|
| Tooling | `xgettext`/`msgfmt` at build; libintl on Windows | none beyond the existing GResource |
| Contributors | standard `.po` editors (Poedit) | plain text TSV; simple, less standard |
| Plurals/context | built in | minimal (add `ctx\|text` keys, no plural rules) |
| Fit here | heavy for ~200 strings, Windows friction | matches how `palette_lexicon.tsv` is shipped |
| Fallback | English msgid | English key |

Proposed default: **B**, with the English source text as the key, a `src/resources/lang/<code>.tsv`
catalog per language, and `i18n::tr()` in a GTK-free header (unit-testable). Revisit if plurals or `.po`
tooling become necessary.

## Open questions

| # | Question | Proposed default |
|---|---|---|
| Q1 | Mechanism: gettext vs own TSV catalogs (above). | B (own TSV in GResource) |
| Q2 | Apply live, or on next start? Live needs every widget to re-read its text (menu model, labels, tooltips, dialogs rebuilt on open). | Dialogs/menus rebuilt on open + header/menus refreshed; if too invasive, "restart to apply" with a note |
| Q3 | First-launch default: system locale (`LANG`/`g_get_language_names`) if supported, else English? | Yes |
| Q4 | Scope: which strings? UI chrome only, or also `!help` text, engine-status text, error messages, About? | UI chrome + messages shown in dialogs/status; keep Engine Log, `!` commands/help, protocol text in English |
| Q5 | Proper terms kept as-is: rule names (decided), "Renju", "Gomoku", "PV", engine names, `.rdb`? | Keep; list them in a "do-not-translate" section of the reference catalog |
| Q6 | Where is the choice stored/shown: new `Language` row in Settings → UI tab (`set.language`, dropdown "System / English / Tiếng Việt") + `language=` key in the settings file? | Yes |
| Q7 | Palette: keep search bilingual always, display title per UI language (L7)? Collapse the duplicated vi strings into the catalog? | Yes / yes, as a follow-up task |
| Q8 | Missing translation policy + lint: fall back to English, and a test fails on keys absent from the reference catalog and on placeholder mismatches? | Yes |
| Q9 | Who translates the first Vietnamese catalog? | I draft, you review (as with the palette dataset) |
| Q10 | Fonts/text width: Vietnamese diacritics + longer strings in fixed-width spots (toolbar, header chip)? Needs a visual pass. | Check headless render + live screenshot per tab |
| Q11 | Split: `I18N-01` mechanism + catalog loader + lint (pure), `I18N-02` wire UI strings (menus/dialogs), `I18N-03` language setting + persistence, `I18N-04` palette titles from catalog? | Yes, in that order |

## Resolution (2026-10-08)

User delegated the answers ("trả lời các câu hỏi mở … rồi chốt thiết kế"); every proposed default above was
adopted, with these specifics:

| # | Decision |
|---|---|
| Q1 | **B** — own TSV catalogs in the GResource (`src/resources/lang/<code>.tsv`), English source text as key; GTK-free `i18n::tr()` in `src/model`-independent header (`src/i18n/`, no gtk). Revisit gettext only if plurals/`.po` tooling are needed. |
| Q2 | Live apply, best effort: `Settings` emits a language-changed signal; menus rebuilt, header/labels refreshed, dialogs rebuilt on open. Anything not refreshable live is listed in I18N-02's acceptance as "restart to apply" with a note beside the setting. |
| Q3 | First launch: first supported entry of `g_get_language_names()`, else English. |
| Q4 | UI chrome + dialog/status/error messages + About. Stay English: Engine Log, `!` commands/`!help`, protocol text. |
| Q5 | Do-not-translate list (rule names, Renju, Gomoku, PV, engine names, `.rdb`) lives in `docs/knowledge`-style note `src/resources/lang/README.md`; lint checks the vi catalog keeps them verbatim. |
| Q6 | `set.language` row in Settings → UI tab (System / English / Tiếng Việt), `language=` key (`system`/`en`/`vi`). |
| Q7 | Palette search stays bilingual; displayed title follows UI language; collapsing duplicate vi strings into the catalog is **I18N-04**, kept in Backlog (not in Sprint 24). |
| Q8 | Missing key → English; ctest fails on keys absent from the reference catalog, orphan keys, and placeholder (`%s`, `%d`, `{n}`) mismatches. |
| Q9 | Claude drafts `vi.tsv`, user reviews. |
| Q10 | Visual pass per Settings tab + toolbar/header under Xvfb (x11 backend) is an acceptance item of I18N-02. |
| Q11 | Split adopted: I18N-01 → I18N-02 → I18N-03 (→ I18N-04 later). I18N-03 (setting + persistence + system-locale default) is pulled with I18N-02 so the feature is usable at sprint end. |
