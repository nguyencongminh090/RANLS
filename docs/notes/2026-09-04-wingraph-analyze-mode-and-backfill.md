# 2026-09-04 — WinGraph vẫn trống trong luồng "user đánh nhiều nước → analyze 1 lần"

Thảo luận nối tiếp UI-13 (đã FIXED, candidate A + flush-ordering). Người dùng báo:
với luồng *user đánh vài nước → bấm Analyze → user đánh vài nước → Analyze lại*,
WinGraph gần như không hiện (chỉ 1–2 điểm rời, không đủ 2 điểm liền kề để vẽ đoạn).

## Vì sao UI-13 chưa che được ca này

`setAnalysisData` chỉ ghi eval cho:
1. node = `currentPath()` tại thời điểm có search thật, và
2. node con ngay sau nước tốt nhất của PV — *nếu* con đó đã nằm trên đường đã đánh.

Các nước user đánh mà không có search chạy trên đúng vị trí đó → không bao giờ là
`currentPath()` trong lúc search → `evalHistory()` trả NaN → WinGraphView vẽ khoảng
trống (đúng theo UI-01: NaN = "chưa đánh giá", không vẽ thành 50% giả).

Đây chính là mục "evaluate the whole played line" đã defer trong
`docs/fix-log/2026-09-03-wingraph-record-eval-regardless-of-side.md` (đề xuất
`GRAPH-xx`).

## Hai hướng người dùng đề xuất

### 1. Backfill nước trước: `eval[i-1] = 1 - eval[i]`

- Bản chất là "gương" của candidate A (A làm xuôi: con = 1 − cha; đây làm ngược:
  cha = 1 − con).
- Rẻ, không cần search thêm, lấp được tính liên tục để đường vẽ ra.
- Rủi ro: đẳng thức `eval(i-1) = 1 - eval(i)` chỉ đúng khi nước đi từ i-1 sang i là
  nước "trung tính / tốt". Nếu user đánh dở, backfill sẽ làm i-1 trông xấu bằng i
  → **giấu mất blunder**, ngược mục đích của win-graph.
- Chain backfill qua nhiều ply trống → sai số cộng dồn, thành một đoạn phẳng gây
  hiểu nhầm "không có gì xảy ra".
- Thỏa hiệp đề xuất: chỉ backfill **đúng 1 hop** (chỉ lấp i-1 khi i-1 liền ngay
  trước một node được search thật và bản thân chưa có eval), và đánh dấu điểm suy
  ra khác về mặt thị giác (mờ hơn / chấm rỗng) để phân biệt với điểm đo thật.
  Không chain.

### 2. Analyze Mode (ponder liên tục — mô hình Lizzie)

- Đây là cách LizzieYZY / KataGo-GUI thực sự giải: engine ponder vị trí hiện tại
  liên tục; mỗi lần user điều hướng hoặc đánh, engine re-analyze → mọi vị trí user
  ghé qua đều có eval đo thật.
- Chất lượng dữ liệu tốt nhất, không suy diễn.
- Giá: engine chạy liên tục (CPU/nhiệt/pin); cần vòng đời sạch — pause khi
  load/replay game, stop gọn, không đánh nhau với "Engine plays <side>".
- Hợp kiến trúc hiện có: đã có `isAnalyzing()`, `tickAnalysis` throttled,
  `signal_board_changed`. "Analyze Mode" = mỗi `signal_board_changed`, nếu mode bật
  và engine idle → kick analyze trên vị trí mới.
- Lưu ý: phần "on Stop, engine plays best move" mà user mô tả là gộp nhầm 2 khái
  niệm. Nên tách bạch: **Analyze Mode** (ponder, không bao giờ tự đánh) vs
  **Engine plays <side>** (tự đánh). Hai cái có thể cùng bật.
- Biến thể nhẹ mà Lizzie cũng có: nút **"Analyze entire game"** một lần — quét mọi
  node đã đánh với ngân sách cố định (vd 1s/nước) rồi lấp `evalHistory`. Rẻ hơn
  always-on, lấp graph theo yêu cầu.

## Các UI khác xử lý WinGraph thế nào

- **Lizzie / LizzieYZY**: ponder liên tục lấp mọi node đã ghé; có "blunder bar";
  cho ẩn đường winrate; hiển thị mean/stdev/last-move phía trên; có Batch Analyze /
  Hawk Eye để quét cả ván. Luôn vẽ từ một phía cố định (Black) + toggle. Khi tắt
  ponder thì để trống chỗ chưa phân tích (giống quyết định NaN-gap UI-01 ở đây).
- **dhbloo/gomoku-calculator**: là công cụ tính winrate cho *một* thế cờ, không
  phải đồ thị liên tục — dạng evaluator theo vị trí, không áp dụng trực tiếp cho
  bài toán "lấp cả đường".

## Khuyến nghị

- **Ngắn hạn** (`GRAPH-xx` nhỏ): hướng 1 nhưng giới hạn 1 hop + đánh dấu là điểm
  suy ra. Rủi ro thấp, unit-test được, nối tiếp đúng pattern UI-13.
- **Giải pháp đúng** (`feat/analyze-mode`): hướng 2 như một feature thật — cần
  `features/analyze-mode/` (user story, sơ đồ state vòng đời, tương tác với
  Engine-plays / load / undo) trước khi vào TODO. Đây là chuẩn ngành.
- Cân nhắc thêm nút **"Analyze entire game"** làm mức trung gian.

## Khảo sát best-practice (2026-09-04, web/GitHub)

**Không GUI phân tích nào backfill/nội suy winrate bằng công thức.**

- **Lizzie `WinrateGraph.java`**: node có `playouts == 0` bị **bỏ qua hoàn toàn**;
  chỉ nối đường giữa các node đã phân tích thật, gặp gap thì ngắt bút
  (`lastNodeOk = false`). Không đoán.
- **Sabaki**: đường winrate giữ nguyên, node chưa phân tích không có điểm; nhưng
  **"winrate values are recorded when generating moves even if analysis mode is
  turned off"** → lưu win% vào node + SGF ngay cả khi không analyze (= candidate C
  của UI-13). Có "score lead" graph, ẩn được đường winrate.
- **En Croissant (cờ vua)**: 2 cơ chế tách bạch — *live engine analysis* (ponder
  khi điều hướng) và *Game Report* (quét cả ván). Best practice: **phân tích từ
  cuối ván ngược về đầu** để engine tái dùng hash/TT.
- **KaTrain / KataGo**: nút *"analyse all"* — quét mọi vị trí từ hiện tại tới cuối,
  ngân sách giây/nước cố định. KataGo analysis engine có `analyzeTurns` để xin
  win% cho *mọi* turn trong 1 query (Rapfi/Gomocup-protocol **không** có cái này →
  RANLS phải search tuần tự từng vị trí).
- **Lichess**: eval→win% là **sigmoid hiệu chỉnh từ dữ liệu thật**
  `Win% = 50 + 50*(2/(1+exp(-0.00368208*cp)) - 1)`; accuracy/blunder tính từ
  **Δwin% giữa 2 nước liên tiếp**. → Đây chính là lý do **không được backfill**:
  đặt `eval[i-1] = 1 − eval[i]` ép Δ = 0, giết luôn tín hiệu phát hiện sai lầm.
- **dhbloo/gomoku-calculator**: có "real-time winrate" + "multi-move analysis
  button" (~ quét nhiều nước) nhưng README không mô tả cơ chế; là web wrapper
  quanh Rapfi.

### Lizzie xử lý nước KHÔNG nằm trong candidate của AI thế nào

Xem `rules/Board.java::place()`:

```java
updateWinrate();                       // chốt win% THẬT cho node cha (từ best candidate của root đang ponder)
double nextWinrate = -100;
if (history.getData().winrate >= 0)
    nextWinrate = 100 - history.getData().winrate;   // ước lượng cho node con
BoardData newState = new BoardData(..., nextWinrate, 0 /*playouts*/, nextScoreMean);
```

- Khi user đánh **bất kỳ** nước nào (mạnh/yếu/không có trong candidate — **không
  phân biệt**): node cha được chốt win% thật từ ponder của chính nó; node con nhận
  `100 - winrate(cha)` làm **ước lượng tạm**, nhưng **`playouts = 0`**.
- `WinrateGraph.java` chỉ vẽ node có **`playouts > 0`** → con số ước lượng đó
  **KHÔNG được vẽ lên đồ thị**, chỉ hiện ở text ("estimate") cho tới khi engine
  ponder xong vị trí mới và ghi playouts thật.
- **Không có variation/branch matching**: kể cả khi nước user *đúng là* một
  candidate, Lizzie vẫn **không** copy win% của candidate đó vào node mainline —
  nó luôn re-ponder vị trí mới từ đầu như một root mới (1 visit thì quá nhiễu).
- ⇒ "fullboard hay không fullboard" của danh sách candidate **không liên quan** đến
  việc ghi đồ thị. Candidate chỉ dùng cho: gợi ý nước trên bàn, preview PV, và win%
  tức thời khi hover.

**Điểm mấu chốt:** Lizzie *có* triển khai đúng công thức `100 - cur` của "hướng 1"
— nhưng cố ý để `playouts = 0` nên **không vẽ nó**; chỉ là text tạm, bị thay bằng
phân tích thật. Đây là xác nhận mạnh cho khuyến nghị: đừng đưa số suy diễn lên
đường đồ thị.

### Rút ra

1. Lấp graph = **phân tích thật mọi node**, không đoán. Hai cơ chế chuẩn:
   (a) continuous/ponder khi điều hướng, (b) "analyze entire game" quét tuần tự
   ngân sách/nước.
2. **Persist win% vào node + file save/load** để khỏi phân tích lại (Sabaki/SGF).
3. Ghi win% cả khi engine tự sinh nước lúc analyze tắt (candidate C).
4. Giữ **gap trung thực** cho node chưa phân tích (đúng UI-01 hiện tại).
5. Backfill công thức: cùng lắm 1 hop + đánh dấu "suy ra", không chain, không coi
   là cơ chế chính.
6. Tách **Analyze Mode** (ponder, không tự đánh) khỏi **Engine plays** (tự đánh).

## Trạng thái

**Đã chốt (2026-09-04): dùng "Lizzie way" (hướng 2 — continuous analysis).**
Formalized:
- Design: `features/analyze-mode/` (`user_story.md`, `diagram/flow.md`, `planning.md`)
- Backlog: `ANLZ-01` (continuous Analyze Mode) + `ANLZ-02` ("Analyze entire game")
  + `ANLZ-03` (persist win% vào file save) — xem `TODO.md` Backlog.
- `ANLZ-01` còn gated trên `features/analyze-mode/planning.md` Q1–Q8 trước khi vào sprint.
- Ý tưởng `GRAPH-xx` cũ ("evaluate the whole played line") trong
  `docs/fix-log/2026-09-03-wingraph-record-eval-regardless-of-side.md` bị
  **superseded** bởi ANLZ-01 + ANLZ-04.

### Cập nhật 2026-09-04 (sau khi ANLZ-01 ship)

- ANLZ-01 đã ship (PR #9). Nhưng engine turn-by-turn (Rapfi CPU alpha-beta)
  **không ponder liên tục** như Lizzie (KataGo GPU rẻ) → vẫn còn ply NaN khi engine
  chưa chạy / Analyze Mode tắt lúc đó / lượt engine / search bị cắt.
- **`ANLZ-02` DROPPED** — cả "Analyze entire game" lẫn "Toggle Ponder" đều bỏ:
  giữ engine CPU search mọi vị trí liên tục quá đắt. `ANLZ-02` là code đã nghỉ hưu.
- **`ANLZ-04` (mới)** — WinGraph vẽ **nét đứt mờ nối** qua khoảng NaN thay vì gãy
  đường. Luôn bật, không cap độ dài gap. Đây là cách xử lý "discontinuity" thay cho
  Ponder. Cần entry `docs/audit/` vì tinh chỉnh lại quyết định UI-01.
- **`ANLZ-03`** giữ nguyên (persist win% vào file save).
