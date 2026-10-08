# Instruction — I18N-02: wire UI strings

## Approach
Wrap per file (menus, then dialogs, then status/errors), run the lint after each. Keep English literal as the key so `en` output is unchanged. Claude drafts `vi.tsv`; user reviews before merge.

## Pitfalls
- Strings built by concatenation: convert to a single `%s` template key.
- Rule names, Renju, Gomoku, PV, engine names, `.rdb` stay verbatim.
- Do not wrap Engine Log / `!` / protocol text.
- Static widgets built once need a refresh hook on the language-changed signal.

## Verification before done
UI tests unchanged under `en`; Xvfb (`GDK_BACKEND=x11`, WAYLAND_DISPLAY unset) screenshots of each Settings tab and main window under `vi`.

## Boundaries
Text-only change; no selector UI (I18N-03), no palette titles (I18N-04).
