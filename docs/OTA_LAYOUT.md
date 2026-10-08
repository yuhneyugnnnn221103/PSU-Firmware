# OTA – bản đồ flash, metadata, hành vi khi mất điện

## Memory map (STM32H725, 1 MB, 8 sector × 128 KB)

| Sector | Địa chỉ | Nội dung |
|---|---|---|
| 0 | 0x08000000 | Bootloader |
| 1–2 | 0x08020000 | Slot A (header 1 KB + app, tối đa 255 KB) |
| 3–4 | 0x08060000 | Slot B |
| 5 | 0x080A0000 | Dự phòng |
| 6, 7 | 0x080C0000 / 0x080E0000 | Cfg bank 0 / bank 1 (luân phiên) |

Hằng số nằm ở `shared_lib/ota_layout.h`; `FLASH ORIGIN/LENGTH` trong
`STM32H725RGVX_FLASH_SLOT{A,B}.ld` phải khớp (ORIGIN = base + 0x400).

## Metadata (`shared_lib/cfg_store.c`)
Log bản ghi 64 byte (magic, seq, type, len, payload, CRC32). Một bank "hợp lệ"
khi có bản ghi `COMMIT`. Bank đang dùng = bank hợp lệ có `COMMIT.seq` lớn nhất.

Compaction khi bank đầy: xoá bank kia → ghi snapshot (boot A/B, limits) →
ghi `COMMIT` cuối cùng. Bank cũ chỉ bị xoá ở lần compaction kế tiếp.

| Mất điện khi... | Kết quả |
|---|---|
| đang xoá bank mới | bank mới không có COMMIT → bỏ qua, bank cũ còn nguyên |
| đang ghi snapshot | như trên |
| sau khi ghi COMMIT | bank mới thắng, bank cũ bị bỏ |
| giữa 2 flash-word của 1 bản ghi | CRC sai → bản ghi bị bỏ, ô không dùng lại |

Kiểm thử: `make -C tests/host test` (flash giả, cắt điện tại từng thao tác).

## Chọn slot khi khởi động (`shared_lib/ota_select.c`)
Một slot chỉ được chọn khi **đồng thời**: ảnh hợp lệ (CRC32 + vector), chưa bị PC rollback
(`boot_count != 0xFFFFFFFF`), và chưa hết lượt (đã confirm, hoặc `boot_count < 3`).
Nhiều ứng viên: `install_seq` lớn hơn thắng, hoà thì `version` lớn hơn.

- Bootloader tăng `boot_count` **trước** khi nhảy với ảnh chưa confirm → ảnh được thử đúng 3 lần;
  lần reboot thứ 4 slot bị loại, **dù ảnh vẫn hợp lệ**.
- Không còn mức "nới lỏng": nếu không còn slot nào đủ điều kiện → vào recovery.
- Slot chỉ được chọn lại sau khi nạp lại (OTA hoặc recovery ghi lại boot record `count=0, confirmed=0`).
- Hệ quả: các lần reset sớm (mất điện, brown-out, watchdog) trước khi app tự confirm (10 s)
  cũng tiêu hao lượt thử.

## Recovery: nạp firmware trong bootloader khi cả hai slot hỏng
Vào khi `choose_slot()` không tìm được slot nào có CRC32 + vector table hợp lệ.
Bootloader nhận ảnh qua UART bằng **cùng giao thức FW_*** với app (`shared_lib/ota_proto.h`),
trên cả 3 cổng (115200 8N1): RS422 #1 = UART5, RS422 #2 = USART1, FT232 = UART4.
LED nhấp nhanh (50 ms). Code: `stm32h725_ota/Boot/boot_recovery.c` (giao thức) và
`boot_recovery_hw.c` (UART mức thanh ghi, vòng chính).

| Lệnh | Hành vi trong recovery |
|---|---|
| `FW_INFO` | trả ACK với byte slot = `'R'` (app trả `'A'`/`'B'`) → host biết đang ở recovery |
| `FW_BEGIN` | kiểm size, **xoá cả 2 slot** (ACK sau khi xoá xong, ~vài giây) |
| `FW_DATA` | chunk 0: lấy Reset_Handler để chọn slot đích (ảnh phải link đúng slot), ghi + đọc lại so sánh |
| `FW_END` | kiểm CRC32 tổng → cấp `install_seq` → ghi header (1 flash-word, **ghi cuối**) → kiểm lại CRC từ flash + vector → ghi boot record |
| `FW_COMMIT` | ACK rồi reset; bootloader khởi động slot mới |

Mất điện ở bất kỳ bước nào: header chưa hợp lệ → vẫn không có ảnh hợp lệ → lại vào recovery.
Khung `FW_DATA` dài cố định 268 byte (chunk cuối phải được host đệm), như app.

Test host: `make -C tests/host test` (`recovery_test`: giao thức + cắt điện tại từng thao tác flash).

### Khi sản phẩm đóng kín (không còn SWD/JTAG)
- Bảo vệ ghi sector 0 (bootloader) bằng option byte **WRP** để OTA/recovery không thể làm hỏng bootloader.
- RDP level 1 vẫn cho phép bootloader tự ghi flash; RDP level 2 là **không đảo ngược** – chỉ bật
  sau khi đã kiểm thử recovery trên nhiều board.
- Thiết bị phải có đường ra cổng UART nói trên (đã đủ cho recovery).

## Ghi flash khi đầu ra đang bật
Xoá sector chặn CPU (kể cả ISR trip). App chỉ ghi cfg khi đầu ra bật nếu việc
ghi không cần xoá (`CfgStore_NeedsErase()`); ngược lại hoãn tới khi nguồn tắt.
