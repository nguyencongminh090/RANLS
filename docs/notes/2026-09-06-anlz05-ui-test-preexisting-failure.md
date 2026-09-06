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

---

## Cập nhật 2026-09-06 (sau khi PORT-03 merge — PR #26, squash `38ce332`)

**PORT-03 KHÔNG sửa case này.** `mock_engine` của PORT-03 thay `/bin/cat` nhưng vẫn cố tình
*không* phát dòng toạ độ nào (comment ngay trong test: *"none parse as a move"*). Sau PORT-03
`ranls-gui-ui-tests` chạy **29/30** — vẫn đúng 1 case `test_anlz05_no_automove_action` fail (con số
đổi từ 28/29 → 29/30 chỉ vì PORT-03 thêm 1 regression case UI-10, không liên quan). Baseline
`670d0dc` (trước PORT-03) fail y hệt, dòng 135, 5/5 lần chạy cô lập.

### Root cause thật (chính xác hơn phần "Root cause" ở trên — phần đó chưa đúng)

Không phải *"GTK main-loop headless không chạy tới nơi một cách xác định"*. `pumpUntil` bơm
main-loop bình thường suốt 3 s; lệnh `BEGIN` bị **cố ý giữ trong hàng đợi**, không phải "chưa kịp
chạy". Nút thắt là cơ chế **`pendingStopFlush_` của PROTO-04** trong `EngineController`:

1. Scenario A: `analyze()` gửi `YXNBEST`, `searchIntent_ = Analysis`, `state_ = Analyzing`.
2. Scenario B mở đầu bằng `stopAnalysis()` (`engine_controller.cpp:424`):
   - `willEmitTrailingCoord = (state_==Analyzing && searchIntent_==Analysis)` → **true**
   - `pendingStopFlush_ = true` → từ đây **mọi `sendOrDefer()` xếp hàng vào `pendingActions_`,
     không gửi ra wire**
   - `setState(Idle)` → `state_` báo "Idle" ngay (nên `REQUIRE(engineState()==Idle)` pass)
3. `maybeStartAutoMove()` idle callback qua hết mọi guard (analyzeMode off, running, Idle, đúng
   lượt) → gọi `requestEngineMove()` → `sendOrDefer([...]"BEGIN")` → **bị append vào
   `pendingActions_`, `engine_.sendLine` không được gọi**.
4. `pendingStopFlush_` chỉ được xoá ở `signal_move` handler (`engine_controller.cpp:136-141`) khi
   **một dòng toạ độ từ engine** tới — đó là "trailing coordinate" mà một search `YXNBEST` bị
   `STOP` giữa chừng vẫn phát ra theo `docs/protocol.md`.
5. `mock_engine` / `/bin/cat` **không bao giờ phát dòng toạ độ** → `signal_move` không fire →
   `pendingStopFlush_` kẹt `true` vĩnh viễn → `BEGIN` nằm mãi trong hàng đợi → timeout.

Với **engine thật**: dòng best-move trailing tới → xoá `pendingStopFlush_` → xả `pendingActions_`
→ `BEGIN` gửi → test pass. Scenario A pass vì nó không nối lệnh thứ hai sau một analysis bị abort,
nên không chạm cơ chế flush.

Tức: đây là **"engine giả không tôn trọng nửa hợp đồng protocol mà PROTO-04 dựa vào"**, không phải
race hay non-determinism.

### Hướng xử lý (cập nhật)

Phương án "gộp vào PORT-03" ở trên **đã không thành** — PORT-03 đóng rồi mà case vẫn đỏ. Cần một
`CODE` mới, ví dụ `TEST-01` "UI test `test_anlz05_no_automove_action` Scenario B phụ thuộc trailing
coordinate mà `mock_engine` không phát":

1. Cho `mock_engine` một chế độ (env var / sentinel line) phát một dòng toạ độ giả sau khi nhận
   `STOP` tiếp theo một `YXNBEST` — mô phỏng đúng `docs/protocol.md`. Scenario B sẽ xác định được.
2. Hoặc: thay `pumpUntil("BEGIN")` bằng condition-based wait trên trạng thái đã settle thật
   (`pendingStopFlush_ == false`, hoặc một API "queue đã drain") —
   `systematic-debugging/condition-based-waiting.md`.

Không nới timeout.

---

## Cập nhật 2026-09-06 (đã sửa — inline, không CODE)

**Tiêu đề file ("preexisting-failure") và câu "không phải regression" ở trên là SAI.** Đây là
**regression của PROTO-04** (`502bd77`, PR #18). Test xanh ở ANLZ-07 (PR #17, 2026-09-04), đỏ ngay ở
PROTO-04 (2026-09-05). ENG-03 cùng ngày (nhánh từ `main` đã chứa `502bd77`) gắn nhãn "pre-existing"
đầu tiên; 5 entry sau (UI-14, PROTO-03, PORT-01/02/03) mỗi lần "identical on baseline" đều dùng
baseline đã sau `502bd77` — không ai bisect về mốc xanh cuối.

Lưu ý phương án 2 ở trên (**condition-based wait `pendingStopFlush_ == false`**) **tự nó không giải
quyết** — không có gì xoá `pendingStopFlush_` nếu thiếu dòng toạ độ trailing, nên wait cũng chỉ
timeout ở predicate khác. Bắt buộc phải *cấp* toạ độ đó.

**Đã sửa** (`docs/fix-log/2026-09-06-anlz05-uitest-trailing-coordinate-mock-engine.md`): Scenario B
phát `p.eng().signal_line_received.emit("7,7")` sau `stopAnalysis()` — đúng nước default tâm bàn mà
engine thật trả về cho bàn trống, giống `test_anlz06`/`test_proto04`. Không đụng production, không
nới timeout, không đụng `mock_engine`. `ranls-gui-ui-tests` 30/30.

Bài học quy trình: `docs/audit/2026-09-06-anlz05-uitest-misdiagnosis-and-proto04-verification-gap.md`.
Phương án protocol-level "search terminated" signal: đã cân nhắc, **hoãn** — engine thật luôn đóng
vòng lặp cho mọi thế cờ thực tế (kể cả bàn trống), không đáng một protocol path mới.
