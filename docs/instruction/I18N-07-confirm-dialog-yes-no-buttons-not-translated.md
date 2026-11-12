# Instruction — I18N-07: confirm dialog Yes/No buttons not translated

Todo: [`docs/todo/I18N-07-confirm-dialog-yes-no-buttons-not-translated.md`](../todo/I18N-07-confirm-dialog-yes-no-buttons-not-translated.md)

## Approach

This is a bug fix: invoke `systematic-debugging` and follow `.claude/rules/bugfix.md`. The root cause
is already known (I18N-06 audit): `MainWindow::confirmDiscardGame` (`src/main_window.cpp` ~line 872)
builds a `Gtk::MessageDialog` with `Gtk::ButtonsType::YES_NO`, whose labels come from GTK's own
gettext domain, not from `i18n::tr`. Confirm it by reading the code, then fix at that one site.

1. **Make the dialog constructible from a test.** `confirmDiscardGame` is a private `MainWindow`
   member that heap-allocates and shows the dialog, so a test cannot reach its buttons. Extract the
   construction into a small GTK-side helper in `src/ui/` (e.g.
   `Gtk::MessageDialog *makeDiscardConfirmDialog(Gtk::Window &parent, const Glib::ustring &action)`
   or a function that only adds the buttons to an existing dialog). `confirmDiscardGame` keeps its
   empty-board short-circuit, the response handler and the self-delete; only construction moves.
2. **Failing test first** (`tests/test_i18n07_confirm_buttons.cpp`, in the `ranls-gui-ui-tests`
   target, self-skips without a display like `tests/test_i18n04_ui.cpp`): under `vi`, build the
   dialog with today's `YES_NO` code, look up `get_widget_for_response(YES)` / `(NO)` and assert
   their labels equal `i18n::tr("Yes")` / `i18n::tr("No")` (Vietnamese). It must fail before the
   fix. Also assert the `en` labels are "Yes"/"No" (en unchanged).
3. **Fix:** construct with `Gtk::ButtonsType::NONE` and `add_button(i18n::tr("No"), NO)` then
   `add_button(i18n::tr("Yes"), YES)` — same order GTK's `YES_NO` uses (No left of Yes).
4. **Catalog:** add `Yes\tCó` and `No\tKhông` to `src/resources/lang/vi.tsv` (draft; user reviews
   wording). The coverage/orphan lint must stay green.

## Pitfalls

- **Keep `Yes`/`No` as the English text.** The todo allows "more descriptive verbs", but changing
  them changes the `en` UI and the acceptance criterion says `en` is unchanged. Not this task.
- **Response semantics must not change:** confirmation still fires only on `ResponseType::YES`;
  Escape / window close (`DELETE_EVENT`) / No still do nothing. `YES_NO` sets no default response —
  do not add one (would make Enter confirm a destructive action).
- **Catalog key collision:** `vi.tsv` already has `No.` (→ `STT`, a column header). `No` (no dot) is
  a distinct key; do not reuse or edit `No.`.
- Live GUI checks on this Wayland host: `GDK_BACKEND=x11 xvfb-run -a env -u WAYLAND_DISPLAY …`;
  screenshot with PIL/`import`, not a Wayland tool.
- `i18n::tr` keys must be string literals visible to the lint scanner (`tr("Yes")`, not a variable).

## Verification before marking this task done

1. The new test fails on the pre-fix code (record the failure output in the fix-log detail), then
   passes after.
2. `RUN_TESTS=1 ./build.sh` under `GDK_BACKEND=x11 xvfb-run -a env -u WAYLAND_DISPLAY` — both
   `ranls-gui-tests` and `ranls-gui-ui-tests` fully green (UI tests actually ran, not skipped), and
   the i18n catalog lint passes.
3. Live check: run `ranls-gui` under Xvfb with `language=vi`, place a stone, trigger New Game;
   screenshot shows Vietnamese buttons. Same under `en` shows Yes/No. Save the screenshots to the
   scratchpad and reference them in the fix-log detail.
4. Fix-log row in `docs/fix-log.md` + `docs/fix-log/<date>-i18n07-confirm-yes-no-buttons.md`;
   todo `Status:` → ✅ FIXED with verification notes; `TODO.md` Active line → ✅.

## Boundaries — do not touch

- Dialog message / secondary text, and the `confirmDiscardGame` call sites (`onNewGame`,
  `onLoadGame`, board-size Apply).
- **Other dialogs.** `MainWindow::showErrorDialog` (`src/main_window.cpp` ~line 907) uses
  `ButtonsType::OK` — same stock-label class. **List it in the fix-log as a known follow-up; do not
  fix it here** (todo scope boundary: list, don't expand).
- No GTK locale / `setlocale` / `bindtextdomain` changes — rejected design (one language system).
- No `model/` changes; no change to `i18n` loader code (`src/i18n/`).
