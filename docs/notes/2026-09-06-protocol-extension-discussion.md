# 2026-09-05/06 — Open Protocol Extension: từ ý tưởng "open environment" tới PROTO-03

Ghi lại quá trình thảo luận dẫn tới `features/protocol-extension/` + `docs/todo/PROTO-03-*.md` +
`docs/instruction/PROTO-03-*.md`, vì bản thân quá trình quyết định (cái gì bị loại, tại sao) quan
trọng không kém kết quả cuối — nhiều lựa chọn ban đầu đã bị đảo ngược sau khi soi vào code thật.

## Xuất phát điểm: architecture report

Trước khi bàn tính năng mới, đã yêu cầu khảo sát kiến trúc protocol hiện tại (`IEngineProtocol` +
`GomocupProtocol` là implementation duy nhất, ghép qua `sigc::signal`, không có coupling ngược).
Kết luận: interface đã có sẵn và đúng chuẩn, nhưng chưa có cách nào để **thêm lệnh mới mà không sửa
C++**. Đây là khoảng trống dẫn tới ý tưởng "open environment cho protocol".

## Ý tưởng ban đầu và câu hỏi "đây có phải Plugin Architecture không?"

Người dùng đề xuất: engine developer tự định nghĩa lệnh mở rộng qua file `.ptc`, ví dụ
`yxAnalyzeOne(x,y)` phân tích 1 nước cụ thể. Khi được hỏi có biết Plugin Architecture không, đã so
sánh 2 hướng:
- **Declarative plugin** (file cấu hình + DSL giới hạn, không chạy code thật) — chọn hướng này.
- **Code-driven plugin** (`.so`/script engine nhúng) — loại bỏ vì phá ranh giới 4-layer hiện có
  (`ui/` chỉ được code nội bộ động vào) và cần sandbox nặng nề không cần thiết.

Đào sâu thêm về khả năng mở rộng lâu dài (stress-test với VCF path, heatmap, tracking xu hướng qua
nhiều lượt) cho thấy hướng declarative vẫn đủ sức miễn là: sink dispatch là registry (không phải
switch cứng), và có điểm graduate rõ ràng sang code thật nếu sau này cần **tính toán tùy ý** (chứ
không chỉ hiển thị). Quyết định: **không thêm gì vào scope ngay** (từ chối "Q9" lúc đó) — giữ đúng
nguyên tắc "không thiết kế cho nhu cầu giả định".

## Sink whitelist: 3 lần thu hẹp liên tiếp

1. Đề xuất đầu: `highlight_cell(x,y,color)` với `color` là chuỗi tùy ý.
2. Người dùng chỉ ra ảnh chụp màn hình có vòng tròn "48%" màu gradient — đó chính là
   `set_source_from_winrate()` đã có sẵn cho `candidateMoves`/`databaseMarkers`. Sửa `highlight_cell`
   nhận `winrate: float` thay vì `color: string`, tái dùng đúng gradient đó.
3. Người dùng tự đặt câu hỏi "highlight_cell maybe useless" — soi lại thấy nó cần hạ tầng UI mới
   (`BoardViewModel` field + `BoardRenderer` layer), trong khi use case gốc (biết win% của 1 nước)
   `set_status_field` đã trả lời đủ. **Bỏ hẳn `highlight_cell`/`clear_highlights` khỏi v1.**

Sau đó phát hiện tiếp: `set_status_field` cũng **không phải "0 dòng UI mới"** như tưởng — kiểm tra
`EngineStatusView` thấy 6 field (D/N/NPS/T/Eval/Best) đều là member cố định, không có container tra
theo tên. Chốt: chấp nhận 1 addition nhỏ (dynamic row container), nhỏ hơn nhiều so với vẽ lên board
nhưng không phải zero.

## Khối lệnh open/close: `send` không chỉ là 1 dòng

Người dùng đưa ví dụ `yxboard x,y,c x,y,c done` và hỏi có cần phân biệt lệnh 1-arg vs nhiều-arg dạng
khối. Soi code thật (`GomocupProtocol::generateAnalyzeRequest`/`generateMoveRequest`) xác nhận:
`YXBOARD`/`BOARD ... DONE` đúng là khối multi-line thật (`std::vector<std::string>`), và console đã
có tiền lệ tương tự (`PosSession`, `!pos ... done`). Đây là **lỗ hổng thật trong schema Q1 ban đầu**
(chỉ hỗ trợ `send` 1 dòng), không phải scope creep — sửa Q1 để hỗ trợ `send.open/repeat/close`.

## `group` classification: giới hạn "search thật" ra khỏi v1

Câu hỏi tiếp theo — nếu `yxAnalyzeOne` cần đồng bộ board trước (như `!analyze` làm), nó có cần tích
hợp với `EngineState`/Stop không? Soi `EngineController::analyze()` thấy tín hiệu "search xong" duy
nhất là 1 dòng tọa độ trần qua `signal_move` — reply của lệnh custom (`MOVEEVAL...`) không bao giờ
khớp dạng đó. Tích hợp thật sẽ cần `SearchIntent` mới + sửa `pendingStopFlush_`/Stop, đụng thẳng vào
state machine tinh vi (PROTO-04/ANLZ-06). Người dùng đề xuất tái dùng taxonomy 7 nhóm đã có trong
`!help` (`analysis/board/config/database/debug/engine/info`) làm cờ phân loại — chốt: **v1 chỉ hỗ
trợ 6 nhóm tức thì, `group = "analysis"` (search thật) là follow-up riêng.** Hệ quả: `yxAnalyzeOne`
— ví dụ xuyên suốt — phải khai `group` khác `analysis`, chấp nhận không tích hợp Stop/EngineState.

## `$currentPath`: dữ liệu sống, không phải tham số user gõ

Điểm còn treo cuối: `send` cần cách đọc vị trí bàn cờ hiện tại (không phải user tự gõ lại từng nước)
để dựng khối kiểu `YXBOARD`. Chốt: `send.repeat_source = "$currentPath"`, loader tự tính sẵn
`i.x`/`i.y`/`i.color` cho mỗi phần tử (màu xen kẽ theo index, đúng công thức
`generateAnalyzeRequest` đã dùng) — `.ptc` chỉ cần bỏ field nào không cần trong template, không cần
cờ "optional" riêng.

## Kết quả

Toàn bộ Q1–Q9 (schema, DSL, collision, config path, limits, hot-reload, console namespace,
multi-line reply, group+context) được chốt qua 2026-09-05/06, formalize thành:
- `features/protocol-extension/` — `user_story.md`, `diagram/flow.md`, `planning.md`,
  `examples/yxAnalyzeOne.ptc`.
- `docs/todo/PROTO-03-open-protocol-extension.md` + dòng Backlog trong `TODO.md`.
- `docs/instruction/PROTO-03-open-protocol-extension.md` + entry trong `instruction.md`.

Bài học chính đáng nhớ cho lần sau: **hỏi ngược lại code thật trước khi chốt schema** — ít nhất 3
quyết định lớn (color→winrate, highlight bị loại, send thành khối) chỉ đến sau khi đối chiếu với
`board_renderer.cpp`/`gomocup_protocol.cpp`/`engine_status.cpp` thật, không phải suy luận trừu tượng.
