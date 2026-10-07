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

## Khi không còn slot hợp lệ (recovery)
`fatal_no_valid_slot()` nhấp LED nhanh và giữ SWD truy cập được; phục hồi bằng
cách nạp lại qua SWD. Chưa có nhận ảnh qua cáp trong bootloader.

## Ghi flash khi đầu ra đang bật
Xoá sector chặn CPU (kể cả ISR trip). App chỉ ghi cfg khi đầu ra bật nếu việc
ghi không cần xoá (`CfgStore_NeedsErase()`); ngược lại hoãn tới khi nguồn tắt.
