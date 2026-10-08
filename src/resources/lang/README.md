# UI language catalogs

`<code>.tsv` per language (`vi`; `en` is the identity and has no file). Lines are
`English key<TAB>translation`; `\n`, `\t`, `\\` are escapes; `ctx|text` is a context key;
`#` starts a comment; an empty translation means "untranslated" (English shown).

Keys are the exact literals passed to `i18n::tr("...")` / `trc("ctx", "...")` in `src/`.
`ctest` (`tests/test_i18n01_catalog.cpp`) fails on orphan keys, mismatched placeholders
(`%s`, `%d`, `{n}`) and altered do-not-translate terms.

Do-not-translate (kept verbatim): Freestyle Gomoku, Standard Gomoku, Free Renju, Renju,
Gomoku, PV, Rapfi, Yixin, `.rdb`.

## Coverage and wiring (I18N-02)

`ctest` also fails on a **missing** translation: every `tr("...")` / `trc(...)` literal found in `src/`
needs a non-empty entry in `vi.tsv` (`i18n::lint::checkCoverage`). Strings built from parts are one
template key with printf placeholders (`"Rule: %s"`, `"Value %d, depth %d, bound %s"`), substituted with
`i18n::format(tr("..."), args...)`. Rule names (Freestyle Gomoku / Standard Gomoku / Free Renju), `!`
commands and `!help`, Engine Log text, protocol text, palette entry titles/search data (I18N-04) are not
wrapped. Long-lived widgets re-read their texts on `i18n::setLanguage()` (see `MainWindow::applyLanguage`);
dialogs are built on open.

## Vietnamese terminology (draft for review)

One term per concept; please review and amend in `vi.tsv`, then the lint keeps it consistent.

| English | Vietnamese | Note |
|---|---|---|
| game (one played game / file) | ván, ván cờ | "New game" = Ván mới; "Load" = Mở, "Save" = Lưu |
| move | nước, nước đi | "Move Log" = Nhật ký nước đi |
| board / board size | bàn cờ / kích thước bàn cờ | |
| Black / White | Đen / Trắng | quân Đen in prose |
| rule (menu, chip) | Luật chơi / Luật | rule *names* stay English |
| engine | engine | kept as a loan word; "Engine Log" = Nhật ký engine |
| analysis / analyze / analyze mode | phân tích / Phân tích / Chế độ phân tích | |
| settings | Cài đặt | same word as the palette `@` scope |
| UI tab / theme | Giao diện / Chủ đề | palette title for Theme (I18N-04) still says "Giao diện" |
| hotkeys | Phím tắt | |
| threads / hash / depth / nodes | số luồng / hash / độ sâu / nút | "Nodes" column = Nút |
| variation, variation tree | biến, cây biến | "Multi PV" = Multi PV (nhiều biến); PV stays |
| win rate | tỉ lệ thắng | |
| undo / redo | Hoàn tác / Làm lại | |
| stone | quân | |
| database | cơ sở dữ liệu | |
| palette | Bảng lệnh | scope words: lệnh / thao tác / cài đặt |
| Cancel / Apply / Close / Clear / Browse | Hủy / Áp dụng / Đóng / Xóa / Duyệt… | |
| engine state | TẮT / ĐANG KHỞI ĐỘNG / BẬT / ĐANG NGHĨ / ĐANG DỪNG / BỊ LỖI | header chip, upper case like English |
| stat labels | Sâu: Nút: NPS: TG: Đ.giá: Tốt nhất: | short on purpose (fixed-width status row) |
