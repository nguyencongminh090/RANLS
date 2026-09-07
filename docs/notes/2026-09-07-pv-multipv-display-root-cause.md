# Báo cáo kiến trúc — Hiển thị PV / Multi-PV / Update value / Board draw PV

**Ngày:** 2026-09-07
**Phạm vi:** luồng cập nhật phân tích engine → `PVView`, danh sách Multi-PV, ô giá trị (`EngineStatusView`), marker PV trên bàn cờ.
**Câu hỏi định hướng:** Engine cập nhật hiển thị như thế nào khi sang *depth* mới trong chế độ multi-PV (manual và auto)?

---

## 1. Kết luận chính (main problem)

**Với engine Rapfi ở cấu hình mặc định, GUI KHÔNG hề nhận được bất kỳ cập nhật nào theo từng depth.**
Toàn bộ PV panel, danh sách Multi-PV, ô giá trị và marker PV trên bàn cờ chỉ được cập nhật **đúng một lần, ở thời điểm search kết thúc**, và chỉ với **một dòng PV duy nhất** (PV #1). Các ứng viên Multi-PV #2..#N **không bao giờ tới GUI**.

Đây không phải lỗi ở tầng UI hay tầng throttle — chúng hoạt động đúng. Vấn đề nằm ở **cặp lệnh cấu hình mà GUI gửi cho engine**: GUI tự tay tắt hết các luồng output tăng tiến (incremental) mà chính parser của nó được viết ra để xử lý.

---

## 2. Pipeline hiện tại

```
engine stdout
  → EngineProcess::signal_line_received
  → EngineController::onEngineLine
  → GomocupProtocol::parseLine
        ├─ "MESSAGE ..." → parseMessage()   ── xử lý REALTIME / "(n)|..." / "Depth ..." / "Speed ..." / Bestline / UCILIKE
        └─ "INFO ..."    → parseInfo()       ── xử lý INFO DEPTH/EVAL/PV n / "INFO PV DONE"
  → (mỗi khi có dữ liệu mới) signal_analysis.emit(currentPVs_, currentStatus_)
  → EngineController: nếu gameState_.isAnalyzing() → GameState::setAnalysisData(pvs, status)
        · cập nhật eval/depth/nodes cho node hiện tại của cây
        · KHÔNG emit ngay — chỉ đặt cờ analysisDirty_ / treeDirty_   (RT-01)
  → MainWindow: timer 75 ms → GameState::tickAnalysis()
        → nếu analysisDirty_ → signal_engine_analysis.emit()
  → signal_engine_analysis được nối ở 2 nơi:
        · AnalysisPanel : EngineStatusView::update() + PVView::update() + WinGraph::setData()
        · MainWindow    : BoardViewModel::update()  (dựng lại candidateMoves từ pvLines) + BoardView::queueRedraw()
  → khi search xong: EngineController gọi GameState::flush() (emit ngay, không chờ tick)
```

Bàn cờ vẽ PV từ hai nguồn tách biệt:
- `candidateMoves` — nước đi đầu của **mỗi** PV line (marker heat-map), dựng lại trong `BoardViewModel::update()` từ `state_.pvLines()`.
- `pvPreview` — "ghost stones", **chỉ** do tương tác hover trong `PVView` đặt vào, không liên quan tới luồng engine.

Kết luận phần này: **mọi thứ hạ nguồn `signal_analysis` đều đúng**. Nếu `currentPVs_` có N dòng theo từng depth thì UI sẽ hiển thị đúng N dòng cập nhật ~13 Hz. Vấn đề là `signal_analysis` gần như không bao giờ được phát trong lúc search.

---

## 3. Điều gì thực sự xảy ra khi sang depth mới

### 3.1 Lệnh GUI gửi cho engine

- `generateConfig()` (`gomocup_protocol.cpp:260`) — **hardcode** `INFO SHOW_DETAIL 0`.
- `generateAnalyzeRequest()` (`:283`) — `YXBOARD … DONE` + `YXNBEST <multiPV>`.
- **Không** gửi `YXSHOWINFO`. **Không** gửi `INFO SHOW_DETAIL` > 0. **Không** đụng tới `messageMode`.

### 3.2 Hệ quả phía Rapfi (`search/searchoutput.cpp`)

| Luồng output | Điều kiện bật | Trạng thái với GUI này |
| --- | --- | --- |
| `REALTIME …` (feed live) | `SHOW_DETAIL` ∈ {1,3} | **TẮT** (SHOW_DETAIL 0) |
| `INFO PV n / … / INFO PV DONE` (mỗi PV mỗi depth) | `SHOW_DETAIL` ∈ {2,3} | **TẮT** (SHOW_DETAIL 0) |
| `Depth N-M \| Eval … \| … <pv>` (mỗi depth) — `printDepthCompletes` | `messageMode == NORMAL` | **TẮT** (mặc định BRIEF) |
| `(1) <value> … \| <pv>` (mỗi ứng viên Multi-PV) — `printMoveResult` / `printRootMoves` | `messageMode ∈ {NORMAL, UCILIKE}` | **TẮT** (mặc định BRIEF) |
| `Speed … \| Depth … \| Eval … \| Node … \| Time …` + `Bestline <pv>` — `printSearchEnds` | `messageMode ∈ {NORMAL, BRIEF}` | **BẬT — nhưng chỉ 1 lần, khi search KẾT THÚC** |

`YXSHOWINFO` đáng lẽ tự nâng `BRIEF → NORMAL` (protocol.md §1.1), nhưng GUI không gửi lệnh này.

### 3.3 Manual analyze (bấm nút Analyze một lần)

1. `analyze()` → gửi `YXBOARD…DONE` + `YXNBEST n`, đặt `analyzing_ = true`.
2. Engine search **im lặng** qua tất cả các depth — không phát dòng nào.
3. Search xong → phát đúng 2 dòng:
   - `MESSAGE Speed … | Depth d-sd | Eval v | Node n | Time t` → `parseMessage` nhánh `"Speed "` → cập nhật `currentStatus_` (depth/eval/nodes/time) → `signal_analysis`.
   - `MESSAGE Bestline <pv>` → `parseMessage` nhánh `"Bestline "` → `commitPV(0, …)` → `signal_analysis`.
4. `GameState::flush()` (từ EngineController khi có coordinate cuối) → `signal_engine_analysis` → UI vẽ **1 lần**: PVView 1 dòng, ô giá trị = giá trị cuối, bàn cờ 1 marker.

→ **Người dùng không bao giờ thấy depth tăng dần, không bao giờ thấy Multi-PV #2..#N.**

### 3.4 Auto (Analyze Mode ∞)

Giống hệt 3.3, lặp lại cho mỗi vị trí (`scheduleAnalyzeModeRestart` → `stopAnalysis()` → `analyze()`).
Thêm cổng `ANLZ-07`: mỗi search chỉ cho ra **một** snapshot kết quả; hai search liên tiếp cùng `bestMove + evalText` → `analysisConverged()` = true → Analyze Mode **ngừng chạy lại**. Về mặt chức năng đúng như thiết kế, nhưng người dùng chỉ thấy một lần "nhảy" giá trị rồi đứng yên — không có tiến trình depth.

---

## 4. Nguyên nhân gốc rễ (root cause)

> **`GomocupProtocol` được viết để tiêu thụ một luồng phân tích tăng tiến (`REALTIME`, `INFO PV DONE`, `Depth …`, `(n)|…`, UCILIKE), nhưng `generateConfig()` / `generateAnalyzeRequest()` lại gửi cho engine đúng bộ lệnh khiến engine KHÔNG phát bất kỳ luồng nào trong số đó.**

Cụ thể, hai quyết định cấu hình:

1. `gomocup_protocol.cpp:260` — `INFO SHOW_DETAIL 0` (hardcode) → tắt `REALTIME` + `INFO detail`.
2. Không gửi `YXSHOWINFO` và không set `messageMode` → Rapfi giữ nguyên `BRIEF` → tắt mọi dòng per-depth và per-Multi-PV, chỉ còn tóm tắt cuối cùng + `Bestline` đơn.

Toàn bộ cỗ máy hạ nguồn — bộ throttle RT-01 75 ms, logic "cắt vòng mới" STATE-03 trong `commitPV`, `onPVDone`, `parseRealtimePV`, parser UCILIKE, việc `PVView` tái dựng widget khi số dòng đổi (RT-03) — trên thực tế là **code chết** với engine ở cấu hình mặc định. Chúng chỉ chạy nếu engine được cấu hình để stream, điều mà GUI hiện không làm.

### Ảnh hưởng lên từng mục trong phạm vi

| Mục | Biểu hiện | Đánh giá |
| --- | --- | --- |
| **PV display** | Chỉ 1 dòng, chỉ xuất hiện lúc search xong | Do root cause |
| **Multi-PV** | #2..#N không bao giờ hiển thị (BRIEF không phát dòng `(n)`) | Do root cause |
| **Update value** (D:/N:/NPS:/T:/Eval:) | Chỉ nhảy tới giá trị cuối, không tăng dần; NPS của dòng cuối bị bỏ (nhánh `"Speed "` không parse token `Speed <speed>` đứng đầu) | Do root cause + 1 lỗi phụ nhỏ ở `parseMessage` |
| **Board draw PV** | `candidateMoves` chỉ có 1 marker, cập nhật 1 lần; `pvPreview` không dính tới engine (chỉ hover) | Do root cause |

---

## 5. Hướng khắc phục (tóm tắt — cần tạo TODO trước khi code)

Chọn một (hoặc kết hợp) ở `generateConfig()` / `generateAnalyzeRequest()`:

- **Tối thiểu:** gửi `YXSHOWINFO` khi khởi động engine → tự nâng `BRIEF→NORMAL`, có ngay các dòng `Depth …` và `(n)|…` mà `parseMessage` đã biết xử lý.
- **Đầy đủ:** gửi `INFO SHOW_DETAIL 3` (hoặc để người dùng cấu hình; hiện `command_dispatcher.cpp:680` đã cho set thủ công nhưng `:260` luôn ghi đè `0` trước) → bật cả `REALTIME` (highlight "đang nghĩ") và `INFO PV DONE` (Multi-PV per-depth đầy đủ, kèm `NUMPV`).
- Bỏ hardcode `INFO SHOW_DETAIL 0` ở `:260`, đưa thành tham số cấu hình có default > 0.
- Lỗi phụ: nhánh `"Speed "` trong `parseMessage` nên parse cả token tốc độ đầu dòng để cập nhật NPS ở dòng tổng kết.

Kèm regression test: giả lập transcript engine ở chế độ NORMAL + SHOW_DETAIL 3 và assert `PVView` / `candidateMoves` nhận đủ N dòng tăng tiến theo depth.
