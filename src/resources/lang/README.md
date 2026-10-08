# UI language catalogs

`<code>.tsv` per language (`vi`; `en` is the identity and has no file). Lines are
`English key<TAB>translation`; `\n`, `\t`, `\\` are escapes; `ctx|text` is a context key;
`#` starts a comment; an empty translation means "untranslated" (English shown).

Keys are the exact literals passed to `i18n::tr("...")` / `trc("ctx", "...")` in `src/`.
`ctest` (`tests/test_i18n01_catalog.cpp`) fails on orphan keys, mismatched placeholders
(`%s`, `%d`, `{n}`) and altered do-not-translate terms.

Do-not-translate (kept verbatim): Freestyle Gomoku, Standard Gomoku, Free Renju, Renju,
Gomoku, PV, Rapfi, Yixin, `.rdb`.
