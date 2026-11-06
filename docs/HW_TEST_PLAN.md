# Kế hoạch test trên MCU thật (board bring-up)

Phần cứng hiện tại: 3 INA228 ↔ DCM1..3 của cụm 1, một chân `PWR_EN`. Trường chưa có dữ liệu thật (cụm 2..4, Supply, RTC, VBUS…) giữ `00`.
Khung mẫu có CRC: `docs/PSU_FRAME_EXAMPLES.md`. Đặc tả: `docs/PSU_PROTOCOL.md`, `docs/OTA_LAYOUT.md`.

## 0. Chuẩn bị
1. Build 2 project trong CubeIDE: `stm32h725_ota` (bootloader) và `PSU_stm32h7` (app, cấu hình `Debug`, ra `app.bin`).
2. Nạp bootloader vào `0x08000000` bằng ST-Link. Board mới → LED nhấp nhanh (recovery).
3. PC: UART 115200 8N1, địa chỉ PSU = 1, CRC16 tính từ ADDR. Có thể dùng bất kỳ cổng nào trong 3 cổng.
4. **Chưa gắn tải vào đầu ra** khi test điều khiển (mục 4).

## 1. Nạp code (recovery)
| Bước | Gửi | Mong đợi |
|---|---|---|
| 1 | `INFO` | ACK, slot `R` |
| 2 | `GET_VERSION` | `A9 A9`, chuỗi rỗng |
| 3 | `ERASE` | một ACK status 0, info 1 (chờ đến 30 s) |
| 4 | `BEGIN` (size, CRC32 của `app.bin`) | ACK 0 |
| 5 | `SET_VERSION` (tên file bỏ đuôi) | ACK 0 |
| 6 | `DATA` 0..N-1 | mỗi gói ACK 0, info = seq |
| 7 | `END` | ACK 0, info 1 |
| 8 | `COMMIT` | ACK 0 rồi PSU reset |
| 9 | chờ ~1 s, `INFO` | slot `A`, byte trạng thái `[12]` bit7 = 0, boot_count tăng |
| 10 | `GET_VERSION` | đúng chuỗi đã gửi |
| 11 | sau ~15 s, `INFO` | bit7 = 1 (đã confirm). **Cần ≥ 1 INA đọc được**, nếu không app không tự confirm và bị loại sau 3 lần reset |

Lỗi cố ý nên thử: `BEGIN` trước `ERASE` (status 11), `DATA` seq nhảy (status 3, info = seq cuối nhận đúng), `END` khi file sai CRC (status 4, info = CRC tính được), `DATA` chunk 0 của bin sai vùng link (status 8).

## 2. Vào recovery từ app
1. Khi app đang chạy: `INFO` (slot `A`) → `ENTER_REC` (ACK 0, reset ~50 ms sau) → `INFO` lặp lại đến khi slot `R`.
2. Khi cụm 1 đang `ON`: `ENTER_REC` phải bị từ chối (status 1, Refused).
3. App nhận `ERASE`/`BEGIN`/`DATA`/`END`/`COMMIT`/`SET_VERSION`: ACK status 12.
4. Cứu hộ: cấp nguồn lại trong khi PC gửi `INFO` mỗi 30–50 ms → bootloader ở lại recovery (LED nhấp nhanh).

## 3. Giám sát và cấu hình (app)
1. `02 02` → khung 310 byte, CRC đúng. Kiểm tra byte 238.. (xem mục 3 của PSU_PROTOCOL.md):
   `rx_frames` (+21) và `tx_sent` (+26) tăng theo từng lần hỏi; `rx_badcrc`, `rx_badaddr`, `tx_skipped`, `tx_error`, `tx_stuck` = 0; uptime (+39) tăng.
2. Nhiệt độ/dòng của DCM1..3 đổi theo thực tế (INA nào chưa gắn → 00 và LED xanh 3 nháy).
3. Ghi cấu hình `04 04` (915 byte) khi mọi cụm `OFF` rồi `03 03` đọc lại: khớp từng byte. Kiểm tra +17 = `04`, +18 = `0`, +19 tăng 1.
4. Ghi cấu hình khi cụm 1 đang `ON`: bị từ chối (+18 = `1`), cấu hình cũ giữ nguyên.
5. Reset PSU rồi `03 03`: cấu hình còn nguyên (lưu Flash).

## 4. Điều khiển (không tải)
1. `01 01` Mask `01` → trạng thái cụm 1 (byte 238) `1 → 2 → 3` (OFF → ARMING → ON), `PWR_EN` (239) = 1; +17 = `01`, +18 = `0`.
2. Mask `00` → về OFF, `PWR_EN` = 0.
3. Tạo trip (ngưỡng dòng/nhiệt độ nhỏ trong cấu hình, hoặc cờ INA) → TRIPPED (4), trip code TRIP1..6 (byte 228..233) có bit; bật lại khi TRIPPED: +18 = `2`.
4. `ClearTrip` → về OFF, trip code về 0; lệnh bật cùng khung bị bỏ qua (+18 = `2`).
5. Mask bit 1..3 (cụm 2..4, chưa có phần cứng): bỏ qua, không có `PWR_EN` mới.

## 5. Truyền thông
- Thử lần lượt cả 3 cổng; phản hồi đi ra đúng cổng nhận. Sai địa chỉ → không phản hồi, `rx_badaddr` tăng. Sai CRC → `rx_badcrc` tăng.
- FT232: firmware mặc định không kiểm tra `RTS#` (`COMM_FT232_CHECK_RTS = 0`).
