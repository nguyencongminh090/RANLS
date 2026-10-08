# About dialog: "Links & protocol" heading rendered blank (2026-10-08)

**Status:** Active
**Related:** UI-22

## Prompt

Help > About: the section heading "Links & protocol" is blank (the other headings render) and GTK
logs a warning. Found by the I18N-02 agent during the vi visual pass; hypothesis (bare `&` in Pango
markup) was unverified.

## Root cause

Root cause is `makeSection()` (`src/ui/about_dialog.cpp`) building Pango markup from the raw title,
`heading->set_markup("<b>" + title + "</b>")`, because a bare `&` in the title ("Links & protocol",
vi "Liên kết & giao thức") starts an XML entity that never ends, so the markup parser rejects the whole
string and the label keeps its empty text. Reproduced under Xvfb (`GDK_BACKEND=x11`, no
`WAYLAND_DISPLAY`) in en and vi; the before-screenshots show the heading row missing in both. Verbatim
warning (en):

```
Gtk-WARNING **: Failed to set text '<b>Links & protocol</b>' from markup due to error parsing markup: Error on line 1: Entity did not end with a semicolon; most likely you used an ampersand character without intending to start an entity — escape ampersand as &amp;
```

vi: same message with `'<b>Liên kết & giao thức</b>'`. The other headings ("Developer", "Tech /
build info") only worked because they contain no `&` or `<`.

## Fix

`makeSection()` now wraps `Glib::Markup::escape_text(title)` in the bold markup. English string and
`vi.tsv` key unchanged; no layout change.

Checked other markup users: the only other `set_markup` is `makeValueLabel(..., markup=true)` in the
same file (translated `Repository:` / `Engine protocol:` prefix plus a hard-coded `<a>` anchor); the
current en/vi strings contain no `&` or `<`, so it is not broken today. `src/main_window.cpp` uses
`use_markup=false`. Not changed (not proven defective).

## Verification

Regression test `tests/test_ui11_about_dialog.cpp` case "UI-22: every About section heading keeps its
visible text (en and vi)": asserts each heading's label text is present and that no GLib log message
containing "markup" is emitted while building the dialog. Failed before the fix (heading missing and the
warning above, en and vi), passes after. `GDK_BACKEND=x11 xvfb-run -a env -u WAYLAND_DISPLAY
RUN_TESTS=1 ./build.sh`: ctest 5/5. After-screenshots (en, vi) show the heading rendered bold, no
markup warning. Screenshots used a scratch harness that presents `AboutDialog` directly (not the full
app window). Not verified: the live app via the Help menu.

## Knowledge

`.claude/rules/bugfix.md`; skills `systematic-debugging`, `gtk-ui-design`.
