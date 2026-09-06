# 2026-09-06 — `test_anlz05_no_automove_action` fail sẵn có trên build host (không phải regression)

Ghi lại trong lúc verify PORT-01 (PR #24). `ranls-gui-ui-tests` chạy **28/29** — 1 case fail. Đã
xác nhận đây là **failure có sẵn từ trước**, không do PORT-01, để lần sau gặp lại không phải điều
tra lại từ đầu. Ứng viên cho một GitHub Issue nhỏ hoặc gộp vào PORT-03.

## Case fail

`tests/test_anlz05_no_automove_action.cpp` — TEST CASE
*"ANLZ-05: Analyze Mode blocks auto-move and analyses the engine's-turn position"*, dừng ở
[line 135](../../tests/test_anlz05_no_automove_action.cpp#L135):

```
REQUIRE( pumpUntil([&] { return sent(wire, "BEGIN"); }) ) is NOT correct!
  values: REQUIRE( false )
```

Test boot một `MainWindow` thật, lấy `/bin/cat` làm engine giả, rồi soi các dòng protocol thực tế
đi ra trên wire. Hai scenario:

- **Scenario A** (assertion chính của ANLZ-05) — Analyze Mode ON, tới lượt engine: chỉ có
  `YXBOARD` / `YXNBEST` đi ra, không bao giờ có `BEGIN` / `BOARD`. → **PASS.**
- **Scenario B** (guard "không phá path auto-move thường") — Analyze Mode OFF, engine cầm Black,
  bàn trống: kỳ vọng path auto-move gửi `BEGIN` trong vòng lặp `pumpUntil` 3 s. → **timeout →
  `pumpUntil` trả `false` → `REQUIRE` abort case.**

## Vì sao không phải PORT-01 regression

- PORT-01 chỉ đụng `settings_dialog.{cpp,h}`, `rdb_container.cpp`, `settings_storage.cpp` — không
  nằm trên path của test này (`MainWindow` auto-move orchestration + `GomocupProtocol`).
- Rebuild sạch `main` tại `c90b0fd` (trước PORT-01): fail y hệt — 25/26 qua 3 lần chạy full, và
  3/3 khi chạy cô lập (`-tc="*ANLZ-05*"`).
- Fix-log PROTO-03 (Sprint 13) đã ghi nhận đúng case này là pre-existing environmental failure.

## Root cause (môi trường, không phải logic)

Đúng như "lessons carried in" của Sprint 13/14: build host **không có engine binary thật, không có
display server**. `/bin/cat` echo lại stdin nhưng không nói được protocol Gomocup, và Scenario B
cần một round-trip đầy đủ qua engine-subprocess I/O + một lượt GTK main-loop mà harness headless
không chạy tới nơi một cách xác định. Scenario A pass vì chỉ cần các dòng `analyze()` đi ra.

## Hướng xử lý

Không sửa trong phạm vi PORT-01. Đúng phần **PORT-03** đã mô tả: "a CMake-built `mock_engine`
target to replace the hardcoded `/bin/cat` / `/bin/true` stand-ins across 7 test suites". Một
`mock_engine` biết trả lời tối thiểu protocol sẽ khiến Scenario B xác định được.

Lựa chọn khi promote:
1. **Gộp vào PORT-03** (mock_engine vốn đã là scope của nó) — ưu tiên, không phát sinh CODE mới.
2. Nếu muốn tách nhỏ: một Issue `TEST-xx` "flaky UI test cases phụ thuộc `/bin/cat` như engine
   giả" — nhưng nhiều khả năng vẫn bị PORT-03 nuốt.

Trước mắt: khi chạy `ctest` trên build host này, **28/29 (hoặc 25/26 trên `main`) là trạng thái
xanh** — chỉ cần đối chiếu đúng 1 case `test_anlz05_no_automove_action` fail, không nhiều hơn.

Liên quan: `features/protocol-extension/` không, nhưng [[2026-09-04-wingraph-analyze-mode-and-backfill]]
(nơi Analyze Mode ra đời) và `.claude/skills/systematic-debugging/condition-based-waiting.md`
(kỹ thuật đúng để thay các `pumpUntil` timeout-based này).
