# UI Language (English / Vietnamese / more later) — user stories

Source: user request 2026-10-08 (after Sprint 23): "Add a **Language** setting for the UI:
Vietnamese / English / support more languages later." Not started — design stage only.

Scope: the **application UI text** (menus, dialogs, tooltips, labels, status text, palette titles).
Out of scope unless decided otherwise in `planning.md`: engine protocol text, the Engine Log, `!` command
names/output, game file formats, rule names (kept in the original — "Freestyle Gomoku", "Standard Gomoku",
"Free Renju" — user decision 2026-10-08).

## Actors

- **User** — wants the interface in their own language; may mix (Vietnamese UI, English engine log).
- **Translator/contributor** — adds a language without touching C++ (drop-in catalog file).
- **Settings dialog / palette** — where the language is chosen / which already search both languages.

## Stories

- **L1** As a user, I can choose the UI language (English, Tiếng Việt) in Settings; the choice persists.
- **L2** As a new user, the first launch follows my system language when it is supported, else English.
- **L3** As a user, switching language takes effect without corrupting state (live, or clearly on next start).
- **L4** As a Vietnamese user, menus, dialogs, tooltips, the Settings tabs and palette titles are Vietnamese;
  names that are proper terms (rule names, "Renju", engine names) stay in the original.
- **L5** As a contributor, I add a language by adding one catalog file; missing strings fall back to English
  rather than showing blanks or keys.
- **L6** As a maintainer, a test fails when a UI string has no entry in the reference catalog, or when a
  catalog has placeholders (`%s`, `{n}`) that differ from the English source.
- **L7** As a palette user, search keeps matching **both** languages whatever the UI language is
  (PAL-01 lexicon unchanged); only the *displayed* title follows the UI language.
