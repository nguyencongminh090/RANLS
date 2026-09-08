# Rapfi engine: how the search-progress output actually works (for PROTO-06)

**Ngày:** 2026-09-08
**Nguồn:** `Rapfi_V1/rapfi/Rapfi/search/searchoutput.{h,cpp}`, `search/ab/search.cpp`,
`core/iohelper.{h,cpp}`, `config.cpp`, `docs/protocol.md`.
**Mục đích:** xác định chính xác Rapfi phát gì / khi nào, để PROTO-06 (port REALTIME lên bàn cờ)
biết cái gì thực sự có sẵn.

---

## 1. Hai luồng, hai công tắc độc lập

| Luồng | Bật bởi | Bit |
| --- | --- | --- |
| `INFO …` (detail, per-PV per-depth) | `INFO SHOW_DETAIL` ∈ {2,3} | `INFO_DETAIL` (`0b10`) |
| `MESSAGE REALTIME …` | `INFO SHOW_DETAIL` ∈ {1,3} | `INFO_REALTIME` (`0b01`) |
| `MESSAGE (n)|…`, `MESSAGE Depth …`, `MESSAGE Bestline …` | `messageMode` (BRIEF/NORMAL/UCILIKE) — **độc lập với SHOW_DETAIL** | — |

PROTO-05 gửi `info show_detail 3` + `yxshowinfo` → cả 3 đều bật (BRIEF→NORMAL do `yxshowinfo`).

`INFO …` in ra **không** có tiền tố `MESSAGE` (`sync_cout() << "INFO "`).
`REALTIME …` **có** (`MESSAGEL` = `"MESSAGE " << …`) → GUI nhận `MESSAGE REALTIME POS x,y`.
→ khớp với routing hiện tại: `parseLine` tách `MESSAGE ` → `parseMessage` → nhánh `REALTIME `.

---

## 2. Luồng REALTIME — điều kiện phát (searchoutput.cpp)

```
showRealtime()       = (infoMode & INFO_REALTIME) && rootDepth >= 8
showRealtimeInLoop() = showRealtime() && tc.elapsed() >= 200ms && balanceMode != BALANCE_TWO
```

| Dòng | Hàm | Điều kiện thêm | **Có phát với Rapfi mặc định?** |
| --- | --- | --- | --- |
| `REALTIME POS x,y` | `printEnteringMove` | `showRealtimeInLoop && !aspirationWindow && pvIdx==0 && !ponder` | **KHÔNG** — `aspiration_window = true` mặc định |
| `REALTIME DONE x,y` | `printLeavingMove` | `showRealtimeInLoop && !aspirationWindow && pvIdx==0 && !ponder` | **KHÔNG** — như trên |
| `REALTIME LOST x,y` | `printMoveResult` | `showRealtimeInLoop && pvIdx==0 && moveValue <= MATED` | **CÓ** (depth≥8, >200ms) |
| `REALTIME BEST x,y` | `printMoveResult` | `showRealtimeInLoop && pvIdx==0 && isNewBest` | **CÓ** |
| `MESSAGE REALTIME REFRESH` | `printPvCompletes` | `showRealtime()` (không có mốc 200ms) | **CÓ** — 1 lần **mỗi PV mỗi depth** |
| `REALTIME BEST` (lần 2) | `printPvCompletes` | ngay sau REFRESH, = `rootMoves[0].pv[0]` | **CÓ** |
| `REALTIME VAL <cp>` | — | — | **KHÔNG BAO GIỜ** — Rapfi không có; chỉ engine Yixin phát |

### Hệ quả quan trọng cho PROTO-06

1. **`POS` / `DONE` (ô sáng lên khi engine đang duyệt) — KHÔNG có với Rapfi cấu hình mặc định**,
   vì aspiration window bật. Muốn có phải đặt `aspiration_window = false` trong `config.toml` của
   Rapfi (làm engine yếu đi chút + đổi hành vi search). Engine **Yixin** thì phát đầy đủ POS/DONE.
2. Với Rapfi mặc định, feed REALTIME rút gọn còn: **`LOST` + `BEST` + `REFRESH`**.
   - `LOST` = nước gốc thua → đánh dấu riêng (giữ tới hết search / đổi vị trí).
   - `BEST` = nước tốt nhất gốc hiện tại (1 ô) → highlight.
   - `REFRESH` = mỗi PV mỗi depth xong; ở Rapfi nó chỉ để xoá ô `POS` (mà Rapfi không phát) →
     **gần như no-op**, nhưng vẫn nên xử lý cho engine Yixin.
3. **Không có feed nào ở 8 depth đầu / 200ms đầu.** Overlay chỉ "thức dậy" khi search đủ sâu.
4. **MCTS** (`SEARCH_TYPE mcts`) dùng `printRootMoves` — **không có REALTIME nào**. Overlay realtime
   chỉ tồn tại với search alpha-beta (mặc định).
5. Feed REALTIME **chỉ PV#0** (`pvIdx==0`). Multi-PV không có POS/BEST/LOST riêng cho line 2..N.

---

## 3. Luồng INFO — cái thực sự nuôi nhãn winrate Multi-PV

`printPvCompletes` (alpha-beta) / `printRootMoves` (MCTS + YXNBEST) phát, **mỗi PV mỗi depth**,
khối này rồi `INFO PV DONE`:

```
INFO PV <pvIdx>          (0-based)
INFO NUMPV <n>
INFO DEPTH <rootDepth>   (printPvCompletes; printRootMoves KHÔNG có dòng này)
INFO SELDEPTH <sd>
INFO NODES <nodesForThisMove>
INFO TOTALNODES <n> / INFO TOTALTIME <ms> / INFO SPEED <nps>
INFO EVAL <value>        ("123" | "+M7" | "-M3" | "+M*")
INFO WINRATE <0..1>
INFO BESTLINE <x,y x,y …>   (toàn bộ PV, raw coords)
INFO PV DONE
```

- Không có "round start marker". `INFO PV 0` xuất hiện lại = vòng depth mới (giống logic
  `curpvidx` của Yixin-Board và `onPVDone` của ta).
- `printRootMoves` (YXNBEST) **thiếu `INFO DEPTH`** trong khối — depth phải lấy từ nơi khác
  (dòng `MESSAGE (n)` mang `SD`, hoặc `INFO`/`MESSAGE` khác). Đây đúng là chỗ parser cần cẩn thận.
- Winrate ở `printRootMoves` là `curMove.winRate` (đã tính sẵn), ở `printPvCompletes` là
  `valueToWinRate(value)`.

→ Nhãn winrate per-ô của Yixin-Board (`INFO BESTLINE` nước đầu + `INFO WINRATE` + `INFO DEPTH` →
`boardtag`/`boarddepth`, commit ở `INFO PV DONE`) chạy được với Rapfi. Đây là luồng ổn định nhất
cho "Multi-PV overlay".

---

## 4. Toạ độ

`REALTIME`/`INFO BESTLINE`/bestmove **đều** đi qua `CoordText{pos, size, Config::GeneralCfg.ioCoordMode}`
→ `outputCoordConvert`. Mặc định `ioCoordMode = NONE` → in `pos.x(),pos.y()` (cột,hàng, gốc trên-trái).
`flipY_X` → `size-1-y , x`. Chỉ đặt được qua **file `config.toml`** (`coord_conversion_mode`), không
có lệnh `INFO` runtime.

GUI hiện tại: `coordToEngine` gửi `y,x`; `parseEngineCoord` đọc token đầu là hàng. → GUI **giả định
Rapfi chạy với `coord_conversion_mode = "flipY_X"`** (hoặc engine Yixin native, vốn dùng `y,x`).
Đây là giả định sẵn có của toàn bộ giao tiếp, không phải việc của PROTO-06.

**Kết luận cho PROTO-06:** toạ độ REALTIME đi **cùng đường** với bestmove và `MESSAGE (n)` (đã parse
đúng). → **dùng lại `parseEngineCoord`**, không tự xử lý trục. Pitfall "coordinate axis order" trong
instruction PROTO-06 → giải quyết bằng "reuse `parseEngineCoord`".

---

## 5. Reset / vòng đời (Rapfi phía phát)

- Không có sự kiện "search bắt đầu" nào rõ ràng trên wire ngoài `MESSAGE OptiTime…` (chỉ NORMAL,
  chỉ khi có time limit). GUI tự biết vì nó gửi `YXNBEST`.
- Kết thúc search: `printSearchEnds` → `MESSAGE Speed … | Depth … | Eval … | Node … | Time …`
  (+ `Bestline …` ở BRIEF). Sau đó là **dòng toạ độ bestmove** (YXNBEST vẫn phát 1 — ANLZ-06 loại bỏ).
- Không có "SEARCH DONE"/"STOP OK" cho phân tích. GUI dựa vào dòng toạ độ cuối = tín hiệu xong.
- `INFO PV DONE` cuối cùng cũng phát từ `printBestmoveWithoutSearch` khi nước ra từ opening book
  (không search) — khối có `INFO DEPTH`, `NODES 0`, `TOTALTIME 0`.

---

## 6. Tác động lên thiết kế PROTO-06 (tóm tắt)

1. **Đừng hứa hẹn hiệu ứng "ô sáng lên khi duyệt" (POS/DONE) cho Rapfi** — không có ở cấu hình mặc
   định. Hoặc (a) chấp nhận overlay realtime = LOST + BEST + REFRESH cho Rapfi, POS/DONE chỉ có với
   Yixin / Rapfi `aspiration_window=false`; hoặc (b) PROTO-06 kèm gửi/khuyến nghị
   `aspiration_window` qua `config.toml` hoặc một lệnh (kiểm tra Rapfi có lệnh INFO cho cái này
   không — hiện **không**, chỉ file config).
2. Nhãn winrate Multi-PV per-ô (mục tiêu "Yixin-Board like") → nuôi từ luồng **`INFO PV DONE`**,
   không phải `MESSAGE (n)`. Cần `boarddepth` per-ô + dọn nhãn cũ khi `pvIdx+1 == NUMPV`.
3. Toạ độ: reuse `parseEngineCoord`.
4. Overlay là **live-only** (đúng như Yixin-Board): search xong → không vẽ nữa. Với one-shot
   analyze bàn cờ trở về trơn sau khi hội tụ.
5. Feed chỉ có sau depth ≥ 8 — overlay xuất hiện trễ vài trăm ms, đừng coi là bug.
6. `REALTIME REFRESH` phát **mỗi PV mỗi depth** (không phải 1 lần/depth) — chỉ clear `pos`, rẻ.
