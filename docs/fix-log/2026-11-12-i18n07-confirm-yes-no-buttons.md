# Confirm dialog Yes/No buttons not translated (2026-11-12)

**Status:** Active
**Related:** I18N-07

## Prompt

I18N-06 visual pass: with `language=vi` the "start a new game" / "open a game" confirmations show Vietnamese text but English No / Yes buttons.

## Root cause

`MainWindow::confirmDiscardGame` built a `Gtk::MessageDialog` with `ButtonsType::YES_NO`. GTK labels those buttons from its own gettext domain (stock "_Yes"/"_No"), which the app's TSV catalog (`i18n::tr`) does not control.

## Fix

- Construction moved to `makeDiscardConfirmDialog` in `src/ui/confirm_dialog.{h,cpp}` (testable); `MainWindow::confirmDiscardGame` keeps the empty-board short-circuit, response handler and self-delete.
- `ButtonsType::NONE` + `add_button(i18n::tr("No"), NO)` then `add_button(i18n::tr("Yes"), YES)` (same order as YES_NO; no default response; response handling untouched).
- `src/resources/lang/vi.tsv`: `Yes` -> `Có`, `No` -> `Không` (draft; key `No.` -> `STT` untouched).
- Left alone: dialog message text, call sites, other dialogs, GTK locale.

## Verification

Regression test: `tests/test_i18n07_confirm_buttons.cpp` (ranls-gui-ui-tests; self-skips without display).

Pre-fix (YES_NO, commit 8606e3a, GTK mnemonic underscore not yet stripped by the test):
```
test_i18n07_confirm_buttons.cpp:61: ERROR: CHECK( labelOf(*d, Gtk::ResponseType::YES) == i18n::tr("Yes") ) is NOT correct!
  values: CHECK( _Yes == Có )
test_i18n07_confirm_buttons.cpp:62: ERROR: CHECK( labelOf(*d, Gtk::ResponseType::NO) == i18n::tr("No") ) is NOT correct!
  values: CHECK( _No == Không )
test_i18n07_confirm_buttons.cpp:68: ... CHECK( _Yes == Yes )   (en: only the mnemonic underscore differed)
test_i18n07_confirm_buttons.cpp:69: ... CHECK( _No == No )
```
(The same run also had the i18n catalog lint fail with orphan keys `Yes`/`No`, since nothing yet used those literals. The test was then changed to ignore the mnemonic underscore, so the en case is a real "unchanged" guard.)

Post-fix: `GDK_BACKEND=x11 xvfb-run -a env -u WAYLAND_DISPLAY RUN_TESTS=1 ./build.sh` -> ctest 5/5; ranls-gui-tests 310/310 cases (5111 assertions), ranls-gui-ui-tests 139/139 cases (952 assertions), 0 skipped; catalog lint green.

Live check (Xvfb, x11, settings file `language=...` next to the binary, one stone placed, Ctrl+N):
- vi: /tmp/claude-1000/-run-media-ngmint-Data-Programming-Programming-C---ModifyCode-Rapfi-modif-June4-YixinBoard/54bd63ae-cdf5-4e5c-9f47-10a8cb3c44dd/scratchpad/shot-vi.png -> buttons "Không" / "Có", message Vietnamese.
- en: /tmp/claude-1000/-run-media-ngmint-Data-Programming-Programming-C---ModifyCode-Rapfi-modif-June4-YixinBoard/54bd63ae-cdf5-4e5c-9f47-10a8cb3c44dd/scratchpad/shot-en.png -> buttons "No" / "Yes".
(The test-created `rapfi-gui.settings` was removed afterwards; none existed before.)

## Known follow-up (not fixed)

`MainWindow::showErrorDialog` (`src/main_window.cpp`) uses `ButtonsType::OK`, the same stock-label class; it still shows GTK's own "OK" under `vi`.
