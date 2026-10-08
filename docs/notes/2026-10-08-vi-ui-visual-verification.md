# 2026-10-08 — Vietnamese UI visual verification (I18N-06)

**Size:** S
**Expires:** sprint close
**Related:** I18N-06, I18N-02/03/04, PAL-04; filed from this pass: I18N-07, UX-09

Run: v0.11.1 + I18N-05 (`main` 750ffb7), `build_cmd/ranls-gui` under `xvfb-run` (1280x800, no window manager),
`GDK_BACKEND=x11`, `xdotool` input, PIL screenshots, real `pbrain-rapfi` for engine-on states, `mock_engine_quit` for the crash banner.
Screenshots stayed in the session scratchpad (not committed). Settings file restored afterwards.

**OK under `vi`** (looked at, nothing wrong): main window (header bar, rule badge, Phân tích/Dừng, stats strip, tab titles);
palette empty list, diacritic-free query ("cai dat" → Cài đặt first), `!` view; Settings dialog all five tabs incl. Language row
(System/English/Tiếng Việt dropdown); hamburger menu + Help submenu; About (headings, links, build info, no clipping);
engine-on (PV rows, "ĐANG NGHĨ", Sâu/Nút/NPS/TG/Đ.giá stats, engine log, table tab); crash banner ("BỊ LỖI", "Bỏ qua").
**OK under `en`:** after a live switch vi→en via Settings → Apply, toolbar/tabs/stats/rule badge all refreshed; palette `!` view
(English title + Vietnamese dimmed line) and actions/settings view.

**Findings (filed, not fixed here):**
1. Confirm dialogs ("Bắt đầu ván mới…", "Mở một ván cờ…") show English **No / Yes** buttons — `Gtk::ButtonsType::YES_NO` stock labels
   come from GTK's own gettext, not our catalog → **I18N-07**.
2. **Apply is disabled while the engine path is invalid**, on every tab. Changing Language (or theme) is impossible until the engine
   path is fixed, and the only explanation is on the Engine tab → **UX-09** (not Vietnamese-specific).

**Observations, no action:** palette row "Rule: Standard Gomoku" keeps its English title in `vi` (deliberate: rule names stay English,
vi.tsv maps it to itself) while the toolbar badge says "Luật:". `Gtk-WARNING … measure GtkBox … for width of 608, but it needs at least 609`
appears with the crash banner in **en** too (pre-existing layout rounding, not language-related).

**Not reachable / not captured:** "palette open during a live language switch" cannot happen — language changes only via the modal
Settings dialog and activating a palette setting row closes the palette and opens Settings; tooltips (no hover/WM under Xvfb); native
file-chooser (no portal under Xvfb); Esc-to-close on modal dialogs (no WM focus). These stay human-owed on a real desktop.
