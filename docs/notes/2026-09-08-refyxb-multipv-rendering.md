# Yixin-Board gốc render Multi-PV như thế nào — và khác gì với YixinBoard hiện tại

**Ngày:** 2026-09-08
**Tham chiếu:** `/run/media/ngmint/…/RefYXB/Yixin-Board/main.c` (bản gốc accreator, GTK3, C, 1 file).
**Bối cảnh:** PROTO-05 đã xong nhưng kết quả chưa đúng kỳ vọng của người dùng. Đọc lại cách bản gốc làm.

---

## 1. Bản gốc: Multi-PV = nhãn winrate/depth vẽ TRỰC TIẾP LÊN Ô CỜ, không có panel danh sách

Bản gốc **không có PVView / danh sách PV** nào cả. Toàn bộ "Multi-PV" là các nhãn heat-map
(`winrate%` hoặc `+M/-M`) vẽ lên **ô đầu tiên của mỗi PV line** trên bàn cờ, cập nhật theo từng
depth.

### Lệnh khởi động (main.c:7106–7107)

```c
send_command("info show_detail 3\n");   // bật CẢ REALTIME lẫn INFO detail
send_command("yxshowinfo\n");           // GUIMode: BRIEF→NORMAL, tắt lỗi unknown-command
```

→ PROTO-05 đã chọn đúng bộ lệnh này (Mechanism C).

### Nguồn dữ liệu: luồng `INFO` detail (main.c:6477–6600), KHÔNG phải luồng `MESSAGE (n)|…`

Bản gốc parse các dòng `INFO PV n` / `INFO NUMPV` / `INFO DEPTH` / `INFO BESTLINE` / `INFO EVAL` /
`INFO WINRATE` / `INFO PV DONE`. State per-PV:

| Dòng | Tác dụng |
| --- | --- |
| `INFO PV <n>` | `curpvidx = n` |
| `INFO NUMPV <k>` | `curnumpv = k` |
| `INFO DEPTH <d>` | `curdepth = d` |
| `INFO EVAL +M<n>` / `-M<n>` | `curmatestep` |
| `INFO WINRATE <w>` | `curwinrate = round(w*100)` (cap 99) |
| `INFO BESTLINE <y,x> …` | chỉ lấy **nước đầu tiên** → `boardpvY/boardpvX` |
| `INFO PV DONE` | **commit**: xem dưới |

### `INFO PV DONE` — điểm commit (main.c:6544–6566)

```c
// 1. đóng dấu nhãn cho ô đầu của PV vừa xong
if (showanalysiswinrate && boardpvX >= 0 && boardpvY >= 0) {
    boarddepth[boardpvY][boardpvX] = curdepth;
    boardtag[boardpvY][boardpvX]   = tag;   // "62%", "+M7", … mã hoá thành ký tự
    refresh_board_at(boardpvX, boardpvY);
}
// 2. khi PV CUỐI của vòng depth này xong → dọn nhãn cũ
if (showanalysiswinrate && curpvidx + 1 == curnumpv) {
    for (mọi ô)
        if (boarddepth[y][x] < curdepth) { boardtag[y][x] = 0; refresh_board_at(x, y); }
}
```

Nghĩa là:
- Mỗi PV → **một nhãn trên ô đi đầu của nó**, kèm `boarddepth` để biết nhãn thuộc depth nào.
- Nhãn **giữ nguyên qua các depth**; chỉ bị xoá khi vòng depth mới hoàn tất (`curpvidx+1==curnumpv`)
  và ô đó không còn là ứng viên ở depth mới (`boarddepth < curdepth`).
- Không có hiện tượng "sập rồi mọc lại" — nhãn chỉ đổi giá trị tại chỗ hoặc bị dọn một lần cuối vòng.

### Luồng `REALTIME` (main.c:6408–6460) — feed "đang nghĩ", cũng vẽ lên bàn cờ

| Dòng | Vẽ |
| --- | --- |
| `REALTIME POS y,x` | `boardpos[y][x]=2` — ô engine đang xét (highlight) |
| `REALTIME DONE y,x` | `boardpos[y][x]=1` — đã xét xong |
| `REALTIME LOST y,x` | `boardlose[y][x]=1` — nước thua (dấu riêng) |
| `REALTIME BEST y,x` | `boardbestX/Y` — nước tốt nhất hiện tại (highlight nổi bật) |
| `REALTIME REFRESH` | xoá toàn bộ `boardpos` (bắt đầu depth mới) |
| `REALTIME VAL <cp>` | `bestval` |

Trong `refresh_board_at` (main.c:705–712), khi `isthinking` và `showanalysis`:
`boardtag>0` (nhãn winrate) > `boardlose` > `boardbest` > `boardpos` — thứ tự ưu tiên overlay.

`bestline` (PV đầy đủ) **chỉ dùng để in log**, không vẽ chuỗi ghost-stone lên bàn cờ. Kể cả bản gốc
cũng không vẽ nguyên đường PV khi đang phân tích — chỉ đánh dấu nước đi đầu của mỗi PV.

---

## 2. YixinBoard hiện tại làm gì

- **Nguồn:** `parseMessage` luồng `MESSAGE (n)|…` + `Bestline` + `Depth …`; `parseInfo`/`onPVDone`
  luồng `INFO`. Sau PROTO-05 đã bật `SHOW_DETAIL 3` + `YXSHOWINFO` nên **cả hai luồng đều tới**.
- **PV list:** có `PVView` (panel danh sách) — bản gốc không có.
- **Nhãn trên bàn cờ:** `BoardViewModel::candidateMoves` = 1 marker cho `pv.moves[0]` của mỗi PV,
  label = winrate% / ±M, màu heat theo `pv.score` → `BoardRenderer::drawCandidateMoves`
  (`board_renderer.cpp:388`). **Về hình thức tương đương `boardtag` của bản gốc.**
- **Cập nhật:** `candidateMoves.clear()` rồi dựng lại toàn bộ từ `pvLines()` mỗi tick RT-01 75 ms.

### Khác biệt chính (nhiều khả năng là "chưa đúng kỳ vọng")

| Khía cạnh | Bản gốc | YixinBoard hiện tại |
| --- | --- | --- |
| Feed `REALTIME POS/DONE/LOST/REFRESH` | **Vẽ** ô đang xét / nước thua / reset mỗi depth | `parseMessage` nhánh REALTIME **chỉ xử lý BEST / PV / VAL** — POS/DONE/LOST/REFRESH bị **bỏ qua hoàn toàn**, không có hiệu ứng "đang nghĩ" trên bàn cờ |
| Theo dõi depth per-ô + dọn nhãn cũ | `boarddepth[y][x]` + dọn khi hết vòng | Không có; dựa vào việc dựng lại `candidateMoves` từ `pvLines()` mỗi tick. Nếu engine chỉ báo top-N thì chỉ N ô có nhãn — kết quả gần giống, nhưng cơ chế khác và không giữ nhãn ổn định giữa các depth |
| Nước "thua" (`REALTIME LOST`) | Có dấu riêng | Không hiển thị |
| Ô engine đang duyệt (`REALTIME POS`) | Highlight động | Không hiển thị |
| PV list panel | Không có | Có (`PVView`) |
| Vẽ nguyên đường PV lên bàn khi phân tích | Không (chỉ nước đầu) | Không (chỉ `pvPreview` khi hover) — giống bản gốc |

### Hệ quả

Sau PROTO-05, nhãn winrate Multi-PV **đã** cập nhật theo depth (luồng `(n)` + `INFO PV DONE` giờ
đã tới). Nhưng phần **"cảm giác đang suy nghĩ"** của bản gốc — ô sáng lên khi engine duyệt
(`REALTIME POS`), nước thua bị gạch (`REALTIME LOST`), nước tốt nhất nhấp nháy (`REALTIME BEST`
đã có một phần), reset mỗi depth (`REALTIME REFRESH`) — **chưa được port**. `parseMessage` nhận
các dòng đó (vì `SHOW_DETAIL 3` giờ bật REALTIME) nhưng vứt đi, nên chúng chỉ làm nhiễu Engine Log.

---

## 3. Đề xuất (chờ chốt với người dùng trước khi lập TODO)

1. **Port feed `REALTIME` lên bàn cờ** — mảng trạng thái per-ô cho POS/DONE/LOST + xử lý REFRESH,
   thêm layer render trong `BoardRenderer` (giống `boardpos`/`boardlose` bản gốc). Đây gần như chắc
   chắn là thứ người dùng đang thiếu.
2. **(Tuỳ chọn) Theo dõi depth per-ô** cho nhãn candidate để nhãn ổn định giữa các depth thay vì
   clear-and-rebuild mỗi tick.
3. Xác nhận với người dùng: có muốn giữ `PVView` panel (bản gốc không có) hay chuyển hẳn sang mô
   hình "chỉ vẽ lên bàn cờ".

Xác nhận: `docs/notes/2026-09-07-pv-multipv-display-root-cause.md` (chẩn đoán PROTO-05) vẫn đúng;
note này bổ sung phần *render* mà PROTO-05 cố tình để ngoài phạm vi ("Do not change PVView /
BoardRenderer drawing logic").
