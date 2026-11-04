# OTA – bản đồ flash, metadata, hành vi khi mất điện

**Kiến trúc: bootloader/recovery + MỘT app** (không còn slot A/B). Việc nạp firmware chỉ do bootloader
làm; app không bao giờ ghi hay xoá vùng app khi đang chạy.

## Memory map (STM32H725, 1 MB, 8 sector × 128 KB)

| Sector | Địa chỉ | Nội dung |
|---|---|---|
| 0 | 0x08000000 | Bootloader + recovery |
| 1–5 | 0x08020000 | App: header 1 KB + code (tối đa 639 KB; code bắt đầu 0x08020400) |
| 6, 7 | 0x080C0000 / 0x080E0000 | Cfg bank 0 / bank 1 (luân phiên) |

Hằng số nằm ở `shared_lib/ota_layout.h`; `FLASH ORIGIN/LENGTH` trong
`PSU_stm32h7/STM32H725RGVX_FLASH_APP.ld` phải khớp (ORIGIN = 0x08020400, LENGTH = 0x9FC00).
File `.bin` build ra **không có header**; recovery tự ghi header (1 KB) khi nhận `END`.

## Metadata (`shared_lib/cfg_store.c`)
Log bản ghi 64 byte (magic, seq, type, len, payload, CRC32). Một bank "hợp lệ"
khi có bản ghi `COMMIT`. Bank đang dùng = bank hợp lệ có `COMMIT.seq` lớn nhất.
Bản ghi `BOOT` giữ `{boot_count, confirmed}` của app (slot duy nhất, tag `'A'`).

Compaction khi bank đầy: xoá bank kia → ghi snapshot (boot, blob cấu hình) →
ghi `COMMIT` cuối cùng. Bank cũ chỉ bị xoá ở lần compaction kế tiếp.

| Mất điện khi... | Kết quả |
|---|---|
| đang xoá bank mới | bank mới không có COMMIT → bỏ qua, bank cũ còn nguyên |
| đang ghi snapshot | như trên |
| sau khi ghi COMMIT | bank mới thắng, bank cũ bị bỏ |
| giữa 2 flash-word của 1 bản ghi | CRC sai → bản ghi bị bỏ, ô không dùng lại |

Kiểm thử: `make -C tests/host test` (flash giả, cắt điện tại từng thao tác).

## Khởi động (`stm32h725_ota/Boot/boot_main.c`)
1. `CfgStore_Init`.
2. `Ota_SelectSlot`: app khởi động được khi **đồng thời** ảnh hợp lệ (CRC32 + vector), chưa bị đánh dấu
   vào recovery (`boot_count != 0xFFFFFFFF`), và chưa hết lượt (đã confirm, hoặc `boot_count < 3`).
3. Không khởi động được → **recovery vĩnh viễn** (không trở về).
4. Khởi động được → vẫn **nghe UART 300 ms** (`BOOT_RECOVERY_WINDOW_MS`): nếu nhận được *một khung hợp lệ gửi
   tới PSU này* (thường là `INFO`) thì ở lại recovery để PC nạp; hết giờ không có khung nào → tiếp tục.
   Cửa sổ này cứu được app "hợp lệ nhưng treo" (không thể nhận `ENTER_REC`).
5. `Ota_CommitTrial`: tăng `boot_count` **trước** khi nhảy với ảnh chưa confirm → ảnh được thử đúng 3 lần;
   lần reboot thứ 4 ảnh bị loại **dù vẫn hợp lệ** (vào recovery). Không ghi được bộ đếm → vẫn khởi động (thiết bị không chết).
6. Nhảy vào app.

- Hệ quả: các lần reset sớm (mất điện, brown-out, watchdog) trước khi app tự confirm (10 s)
  cũng tiêu hao lượt thử.
- Sau khi đã confirm, app không bị đếm lượt nữa; app treo/reset liên tục vẫn vào được recovery nhờ cửa sổ 300 ms.

## Cập nhật firmware
PC → app: `INFO` (`95 95`, trả `'A'` + version + trạng thái trial) → `ENTER_REC` (`98 98`).
App (chỉ khi mọi cụm `OFF`) ghi bản ghi `BOOT` với `boot_count = FORCE_FAIL`, ACK, rồi reset sau ~50 ms.
Bootloader thấy app "không khởi động được" → ở lại recovery. PC hỏi `INFO` đến khi slot trả về `'R'`, rồi nạp:

| Lệnh | Hành vi trong recovery |
|---|---|
| `INFO` | ACK với byte slot = `'R'` → host biết đang ở recovery |
| `ENTER_REC` | ACK OK (đã ở recovery), không reset |
| `ERASE` | xoá 5 sector vùng app; **một ACK duy nhất khi xoá xong** (timeout phía PC ≥ 30 s, chưa đo) |
| `BEGIN` | kiểm size (≤ 639 KB), cần đã `ERASE` (ngược lại `NOT_ERASED`) |
| `DATA` | chunk 0: kiểm Reset_Handler nằm trong vùng app (ngược lại `WRONG_SLOT`), ghi + đọc lại so sánh |
| `END` | kiểm CRC32 tổng → cấp `install_seq` → ghi header (1 flash-word, **ghi cuối**) → kiểm lại CRC từ flash + vector → ghi boot record `(0, chưa confirm)` |
| `COMMIT` | ACK rồi reset; bootloader khởi động app mới |

Mất điện ở bất kỳ bước nào: header chưa hợp lệ → không có ảnh hợp lệ → lại vào recovery và nạp lại từ `ERASE`.
**Không còn rollback tự động trên thiết bị**: muốn về bản cũ thì PC nạp lại file `.bin` cũ.
Khung `DATA` dài cố định 269 byte (chunk cuối phải được host đệm `FF`).
Code: `boot_recovery.c` (giao thức, chạy được trên host) và `boot_recovery_hw.c` (UART mức thanh ghi, vòng chính).
Cổng: RS422 #1 = UART5, RS422 #2 = USART1, FT232 = UART4, 115200 8N1, LED nhấp nhanh 50 ms khi ở lại recovery.

App nhận `ERASE/BEGIN/DATA/END/COMMIT` thì trả ACK status `12 NOT_RECOVERY` (phải `ENTER_REC` trước).

Test host: `make -C tests/host test` (`recovery_test`: giao thức + cắt điện tại từng thao tác flash).

### Khi sản phẩm đóng kín (không còn SWD/JTAG)
- **Chưa làm**: bảo vệ ghi sector 0 (bootloader) bằng option byte **WRP**. Vì bootloader là đường cứu *duy nhất*,
  cần làm trước khi đóng kín.
- RDP level 1 vẫn cho phép bootloader tự ghi flash; RDP level 2 là **không đảo ngược** – chỉ bật
  sau khi đã kiểm thử recovery trên nhiều board.
- Thiết bị phải có đường ra cổng UART nói trên (đã đủ cho recovery).

## Ghi flash khi đầu ra đang bật
Xoá sector chặn CPU (kể cả ISR trip). Việc nạp firmware giờ chỉ xảy ra trong recovery (không có đầu ra nào bật).
Trong app, ghi cấu hình (`04 04`) và `ENTER_REC` chỉ nhận khi mọi cụm `OFF` (vì có thể kích hoạt xoá sector khi compaction).

## Máy trạng thái an toàn
- Mỗi cụm DCM (3 DCM, 1 chân EN) có một máy trạng thái riêng `BOOT → OFF → ARMING → ON → TRIPPED`. **Không còn LOCKOUT**:
  trip chỉ cắt nguồn cụm đó và giữ đến khi nhận ClearTrip (xem `docs/PSU_PROTOCOL.md`). Bộ đếm trip chỉ để báo cáo.
- Mọi fault không phục hồi (HardFault / MemManage / UsageFault / BusFault / NMI / `Error_Handler` / `Default_Handler`)
  gọi `Safety_ForceOff()` (cắt mọi chân EN) trước khi chờ IWDG.
- **`Default_Handler` được sửa tay trong `PSU_stm32h7/Core/Startup/startup_stm32h725rgvx.s`**
  (thêm `bl Safety_ForceOff`). CubeMX sẽ ghi đè file này khi generate lại → phải sửa lại sau khi generate.
- Test: `make -C tests/host safety_test`.

## Trial và tự xác nhận
- **Tự xác nhận (`App/self_test.c`)**: ảnh chỉ được confirm khi, *liên tục* trong `SELFTEST_WINDOW_MS`
  (10 s): ≥ `SELFTEST_MIN_CH` kênh INA228 "tươi" và cấu hình đúng, và máy trạng thái safety đã rời
  `SAFE_BOOT`. Điều kiện sai → đếm lại từ đầu. Trip bảo vệ **không** là tiêu chí thất bại.
  Tuỳ chọn: `SELFTEST_MIN_CH` (mặc định 1), `SELFTEST_NEED_PC_LINK` (mặc định 0).
  **Bench test không có INA: app không bao giờ confirm → sau 3 lần reset bị loại và vào recovery.**
- **Lượt thử**: mọi lần boot ảnh chưa confirm đều tốn 1 lượt (kể cả mất điện/brown-out/watchdog),
  tối đa `OTA_MAX_BOOT_ATTEMPTS` = 3. Muốn nới lỏng: tăng hằng số này.
- **`INFO`** trả thêm byte trạng thái trial (offset 12 trong khung 17 byte): bit7 = đã confirm, bit6..0 = `boot_count` (127 = đã yêu cầu vào recovery).
- Lỗi ghi flash trong recovery trả `PROG_FAIL` (9), `info` = mã lỗi HAL; `CRC_FAIL` dành cho sai CRC.

> Chi tiết khung/ACK: `docs/PSU_PROTOCOL.md`.
