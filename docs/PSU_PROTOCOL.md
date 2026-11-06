# Giao thức PSU – phía firmware (APSU, phần cứng bring-up: 3 INA228 giám sát cụm 1)

Nguồn chuẩn của khung và trường: `psu_messages.md` (ACTL Tool). Tài liệu này ghi **cách firmware hiện thực**
và các **mở rộng / khác biệt** so với file đó. Code: `shared_lib/psu_frame.[ch]`, `shared_lib/ota_proto.h`,
`PSU_stm32h7/App/{comm,psu_cfg,psu_trip,psu_monitor}.[ch]`, `stm32h725_ota/Boot/boot_recovery.c`.

## 1. Quy ước
- Khung: `AB CD | CMD (2 byte) | ADDR | dữ liệu | CRC16 (BE) | E1 E2`. Mọi số nhiều byte big-endian.
- **CRC** CRC16-CCITT-FALSE (poly `0x1021`, init `0xFFFF`), **tính từ byte ADDR đến hết payload** (bỏ qua `AB CD`, `CMD CMD`, CRC và `E1 E2`),
  áp dụng cho mọi khung, ở cả app và bootloader. Quay lại cách cũ (tính từ CMD): đặt `PSU_CRC_INCLUDE_CMD 1` trong `psu_frame.h`
  (một chỗ duy nhất; phải đổi cả phía PC). Lưu ý: lỗi bit ở CMD không bị CRC bắt, chỉ bị chặn bởi bảng CMD hợp lệ + độ dài cố định.
- **Địa chỉ PSU** cố định trong code: `PSU_ADDR` (mặc định 1, đặt bằng `-DPSU_ADDR=n`). Khung sai địa chỉ bị bỏ, không phản hồi.
- **Không tự phát**: firmware chỉ trả lời. Trả lời đi ra **đúng cổng đã nhận lệnh** (RS422 #1, RS422 #2, FT232 – dùng cổng nào cũng được).
- Điều khiển (`01 01`) và ghi cấu hình (`04 04`) **không ACK**. ACK chỉ có trong nạp code.

| CMD | Hướng | Dài | Việc |
|---|---|---|---|
| `01 01` | PC→PSU | 13 | Điều khiển: `[5]` Mask, `[6]` ClearTrip (0x01 = xoá) |
| `02 02` | PC→PSU | 13 | Hỏi giám sát: `[5..8]` dự phòng → trả `81 81` |
| `81 81` | PSU→PC | 310 | Giám sát |
| `03 03` | PC→PSU | 13 | Hỏi cấu hình → trả `82 82` |
| `82 82` | PSU→PC | 915 | Trả lời cấu hình (cùng bố cục `04 04`) |
| `04 04` | PC→PSU | 915 | Ghi cấu hình (không ACK) |
| `95 95` | PC→PSU | 9 | INFO (app và recovery đều trả lời) |
| `98 98` | PC→PSU | 9 | **ENTER_REC**: app ghi cờ rồi reset vào recovery (chỉ khi mọi cụm OFF); recovery trả ACK OK |
| `96 96`, `90 90`..`93 93` | PC→PSU | 9 / 21 / 269 / 9 / 9 | ERASE / BEGIN / DATA / END / COMMIT: **chỉ ở recovery** (app trả ACK `NOT_RECOVERY`) |
| `9A 9A` | PC→PSU | 41 | **SET_VERSION**: chuỗi ASCII 32 byte, chỉ ở recovery, giữa ERASE và END (app trả `NOT_RECOVERY`) |
| `99 99` | PC→PSU | 9 | **GET_VERSION** (app và recovery) → trả `A9 A9` |
| `A9 A9` | PSU→PC | 41 | Trả lời GET_VERSION: `[5..36]` chuỗi ASCII, đệm `00`, rỗng = chưa đặt |
| `94 94` | PSU→PC | 17 | ACK |

## 2. Kiến trúc: 4 cụm DCM, mỗi cụm một máy trạng thái
- 1 PSU = 1 STM32 quản lý **4 cụm DCM** (+ cụm Supply). Mỗi cụm có 3 DCM dùng chung **một chân EN**, nên firmware có **4 máy trạng thái safety độc lập**
  (`BOOT → OFF → ARMING → ON → TRIPPED`; **không còn LOCKOUT**: trip chỉ cắt nguồn cụm đó và giữ đến khi nhận ClearTrip).
- Mạch bring-up: **một** chân `PWR_EN` = EN của **cụm 1** (đóng 4 DCM). INA1..3 giám sát DCM1..3 của cụm 1. **INA4 bỏ qua hoàn toàn**
  (không đọc, không cấu hình, không bảo vệ, ALERT4 bị bỏ qua); 4 chân FT của DCM đều thuộc cụm 1.
  Cụm 2..4 chưa có phần cứng: luôn `OFF`, bỏ qua lệnh bật. Thêm phần cứng sau này = điền bảng `k_hw[]` trong `safety.c`.

## 3. Giám sát `81 81`
- **Cụm 1** nhận dữ liệu của INA1..3 theo DCM: `I_OUT_ADCn` ← thanh ghi CURRENT thô của INAn (3 byte, dữ liệu ở bit 23..4, có dấu);
  `TEMP_ADCn` ← thanh ghi DIETEMP thô 16 bit của INAn, đặt ở 2 byte thấp (byte cao = `00`), có dấu.
  Phía giao diện tự dịch phải/xử lý dấu. **VBUS không hiển thị.** DCM mất dữ liệu gửi `00`.
- Cụm 2..4, cụm Supply, RTC: `00`. 10 byte **Trip**: xem mục 6.
- **Vùng dự phòng 68 byte** (byte 239..306 theo cách đếm của tài liệu; 238..305 theo cách đếm từ 0) là **mở rộng của firmware**:

| Offset | Nội dung |
|---|---|
| +0..+2 | cụm 1: `safe_state` (0 BOOT, 1 OFF, 2 ARMING, 3 ON, 4 TRIPPED), trạng thái PWR_EN (1 bật / 0 tắt), số lần trip liên tiếp |
| +3..+5 / +6..+8 / +9..+11 | cụm 2 / 3 / 4 (cùng 3 byte) |
| +12 | slot firmware đang chạy (`A` = app) |
| +13..+16 | số version cũ do `BEGIN` khai báo (u32 BE); PC gửi 0 thì luôn 0 (version nay là chuỗi, xem `99 99`) |
| +17 | lệnh điều khiển/cấu hình gần nhất: `01` điều khiển, `04` ghi cấu hình, `00` chưa có lệnh nào |
| +18 | kết quả lệnh đó: `0` OK, `1` ghi cấu hình bị từ chối (còn cụm chưa OFF), `2` lệnh bật bị từ chối (cụm đang TRIPPED, hoặc vừa ClearTrip trong cùng khung), `3` ghi Flash thất bại |
| +19..+20 | bộ đếm số lệnh đã ghi nhận ở +17 (u16 BE, tràn quay vòng) |
| +21..+38 | 9 bộ đếm chẩn đoán truyền thông, mỗi cái u16 BE tràn quay vòng, theo thứ tự: `rx_frames` (khung hợp lệ), `rx_badcrc`, `rx_badaddr`, `rx_uart_err`, `rx_overrun`, `tx_sent`, `tx_skipped`, `tx_error`, `tx_stuck` (cộng gộp cả 3 cổng) |
| +39..+42 | thời gian chạy kể từ lần reset gần nhất (giây, u32 BE) |
| +43 | phiên bản giao thức (hiện `1`) |
| +44..+67 | 0 (dự phòng) |

Điều khiển (`01 01`) và ghi cấu hình (`04 04`) không có ACK: PC kiểm tra bằng cách hỏi giám sát lần nữa và xem +17/+18/+19 (bộ đếm tăng thêm 1 và kết quả là `0`).
Chỗ chưa có dữ liệu thật (cụm 2..4, Supply, RTC, VBUS…) giữ `00`.

## 4. Điều khiển `01 01`
Bit *c* của Mask điều khiển EN của cụm *c+1* (bit0 = cụm 1 = chân `PWR_EN`); bit của cụm chưa có phần cứng bỏ qua.
Mỗi lệnh ghi đè trạng thái bật/tắt. `ClearTrip = 0x01` áp dụng cho **toàn PSU** (một byte duy nhất):
- xoá mọi trip code và cờ tích luỹ của INA; mọi cụm đang TRIPPED về OFF và huỷ yêu cầu bật;
- cụm vừa thoát TRIPPED **bỏ qua lệnh bật trong cùng khung** (PC gửi lệnh bật ở khung sau); cụm khác bật/tắt theo Mask, ClearTrip không tắt nguồn cụm đang ON.
Lệnh bật bị từ chối (cụm đang TRIPPED hoặc đang nạp code) → bỏ qua im lặng (không ACK).

## 5. Cấu hình `04 04` / `82 82`
- Phần dữ liệu 904 byte (sau Mask 2 byte) được **lưu nguyên vào Flash** (cfg_store, nhiều bản ghi, chốt bằng CRC32): cụm DCM, INA, ngưỡng Supply, dự phòng.
  `82 82` trả đúng nội dung đã ghi (để PC đọc lại so sánh). Chưa lưu → trả cấu hình mặc định (INA mặc định, còn lại 0).
- Mask `FF FF` coi như ghi đè toàn bộ. **Chỉ nhận khi mọi cụm OFF** (đổi `SHUNT_CAL` khi nguồn bật làm sai thang đo); ngoài ra bỏ qua im lặng.
- **INA1..3**: 9 thanh ghi/INA nạp thẳng vào IC (khi ghi và khi khởi động). `SHUNT_CAL = 0` ⇒ dùng mặc định của INA đó (`ina228_cfg.h`); bit RST/RSTACC của CONFIG bị xoá.
  Khối INA4 chỉ lưu và trả lại.
- **Ngưỡng dưới của INA (`SUVL`, `BUVL`) chỉ được nạp khi cụm chứa INA đó ở SAFE_ON**; lúc OFF/ARMING IC giữ giá trị "tắt".
  Lý do (C3): điện áp bus thấp hơn `BUVL` khi nguồn tắt/đang lên là bình thường, nhưng chân ALERT (ALATCH=1) bị kéo xuống liên tục và
  `Safety_FastTrip` (cắt ngay ở ARMING) có thể cắt nhầm. Ngưỡng *trên* (`SOVL`, `BOVL`, `TEMP_LIMIT`) vẫn nạp luôn → bảo vệ quá dòng ở ARMING giữ nguyên tốc độ.
  **Chưa kiểm chứng trên board.**

## 6. Trip code và so sánh ngưỡng (STM tự so)
- Ngưỡng lấy từ **36 byte đầu của khối mỗi cụm** trong cấu hình: `I_OUT_ADC1..3`, `TEMP_ADC1..3` (MAX rồi MIN, 3 byte). So với giá trị thô cùng đơn vị bản tin giám sát.
  Dòng so sánh **có dấu 24 bit**; nhiệt độ lấy **16 bit thấp, có dấu**. **Giá trị 0 = tắt ngưỡng đó** (cả MAX lẫn MIN). Ngưỡng Supply chỉ lưu, không dùng.
- Chỉ so khi cụm **SAFE_ON**; cần `PSU_TRIP_CONFIRM_N` (= 2) mẫu mới liên tiếp (~60 ms/mẫu) vượt ngưỡng mới latch. Hiện chỉ cụm 1 có bảng bit.
- Trip code **latch cho đến khi nhận ClearTrip** (không tự xoá khi giá trị trở lại bình thường; chưa lưu qua reset).
- **Bảng bit dưới đây là TẠM THỜI** – chờ bảng chính thức (sửa ở `psu_trip.h`):

| Byte | Ý nghĩa (cụm 1) |
|---|---|
| TRIP1 | bit n (0..2) = `I_OUT_ADC(n+1)` vượt **MAX**; bit 3+n = `TEMP_ADC(n+1)` vượt **MAX** |
| TRIP2 | như TRIP1 nhưng dưới **MIN** |
| TRIP3 | bit n = INA n báo SHNTOL/POL (quá dòng/công suất); bit 3+n = SHNTUL |
| TRIP4 | bit n = BUSOL (quá áp bus); bit 3+n = BUSUL (sụt áp bus) |
| TRIP5 | bit n (0..2) = TMPOL (quá nhiệt INA n); bit 3+n (n = 0..3) = chân FT của DCM n+1 |
| TRIP6 | bit0 mất liên lạc I²C; bit1 cắt do ALERT nhưng không xác nhận được nguyên nhân; bit3 cấu hình INA không hợp lệ |
| TRIP7..10 | 0 |

## 7. Nạp code (chỉ trong recovery)
App **không** nạp firmware. Trình tự từ PC: `INFO` (`95 95`; app trả slot `A` + version) → `ENTER_REC` (`98 98`; app ACK rồi reset ~50 ms sau)
→ hỏi `INFO` lặp lại đến khi slot trả `R` (recovery) → `ERASE` (xoá vùng app 5 sector; **một ACK duy nhất khi xoá xong**, `Info = 1`;
timeout phía PC ≥ 30 s, chưa đo) → `BEGIN` (size ≤ 639 KB, CRC32, version; ACK ngay; chỉ sau ERASE, nếu không `NOT_ERASED`) →
`DATA` (256 B/gói, gói cuối đệm `FF`, khung luôn 269 B) → `END` → `COMMIT` (ACK rồi reset, bootloader chạy app mới).
Nếu bootloader chưa có app hợp lệ (board mới / app hỏng) thì nó đã ở recovery sẵn: bỏ qua `ENTER_REC`.
Nếu app treo, cấp nguồn lại và gửi `INFO` trong 300 ms đầu: bootloader ở lại recovery.
- ACK `94 94` (17 byte): `[5]` AckCmd (1 byte, ví dụ `96` cho ERASE), `[6]` status, `[7..10]` info, `[11]` slot (`A` = app; **`R` = recovery**),
  `[12]` trạng thái trial chỉ có ở trả lời INFO của app (bit7 confirmed, bit6..0 boot_count).
- Status: 0 OK, 1 Refused (có cụm chưa OFF), 2 BadSize, 3 BadSeq, 4 CrcFail, 5 Busy, 6 NoCommitPending, 7 EraseFail, 8 WrongSlot (Reset_Handler ngoài vùng app),
  **9 ProgFail** (lỗi ghi flash, info = mã HAL/địa chỉ), 10 (dự phòng), **11 NotErased** (BEGIN khi chưa ERASE), **12 NotRecovery** (app nhận lệnh nạp).
- **Version:** chuỗi do PC đặt (tên file bin bỏ đuôi, tối đa 32 ký tự in được `0x20..0x7E`, ví dụ `PSU_v1.2.3_abc123`). Gửi `SET_VERSION`
  sau `ERASE` và trước `END` (thường ngay sau `BEGIN`); recovery ghi vào header của ảnh khi `END` nên chuỗi luôn đi cùng đúng ảnh đó.
  `ERASE` xoá chuỗi của phiên trước; byte ngoài `0x20..0x7E` cắt chuỗi tại đó; không gửi thì chuỗi rỗng. Hỏi bằng `99 99` (app và recovery;
  recovery trả chuỗi của ảnh đang có nếu header hợp lệ). Bin nạp bằng SWD không có header nên không có version.
- Stop-and-wait: PC phải chờ ACK rồi mới gửi khung kế. Muốn về bản cũ: nạp lại file `.bin` cũ (không còn ROLLBACK, không còn slot B).

Ví dụ khung có CRC (do firmware sinh): `docs/PSU_FRAME_EXAMPLES.md`.

## 8. Còn mở
1. Bảng trip code chính thức (mục 6). 2. (Đã xong) CRC tính từ ADDR, `81 81` dài 310 byte. 3. Cụm 2..4, Supply, RTC: chưa có nguồn dữ liệu / phần cứng.
4. C3 (ngưỡng dưới INA khi OFF/ARMING): cần đo trên board. 5. Cổng nào là đường Gateway/dự phòng (hiện cả 3 cổng nhận mọi lệnh).
