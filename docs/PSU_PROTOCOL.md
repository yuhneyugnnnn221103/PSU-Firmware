# Giao thức PSU – phía firmware (APSU, phần cứng bring-up 4 INA228)

Nguồn chuẩn của khung và trường: `psu_messages.md` (ACTL Tool). Tài liệu này ghi **cách firmware hiện thực**
và các **mở rộng / khác biệt** so với file đó. Code: `shared_lib/psu_frame.[ch]`, `shared_lib/ota_proto.h`,
`PSU_stm32h7/App/{comm,psu_cfg,psu_trip,psu_monitor}.[ch]`, `stm32h725_ota/Boot/boot_recovery.c`.

## 1. Quy ước
- Khung: `AB CD | CMD (2 byte) | ADDR | dữ liệu | CRC16 (BE) | E1 E2`. Mọi số nhiều byte big-endian.
- **CRC** CRC16-CCITT-FALSE. Hiện **tính từ byte CMD** (khớp ACTL Tool). Muốn đổi sang tính từ ADDR: đặt
  `PSU_CRC_INCLUDE_CMD 0` trong `psu_frame.h` (một chỗ duy nhất, dùng chung app + bootloader; phải đổi cả phía PC).
  Lưu ý khi bỏ CMD khỏi CRC: lỗi bit ở CMD không bị CRC bắt, chỉ bị chặn bởi bảng CMD hợp lệ + độ dài cố định.
- **Địa chỉ PSU** cố định trong code: `PSU_ADDR` (mặc định 1, đặt bằng `-DPSU_ADDR=n`). Khung sai địa chỉ bị bỏ, không phản hồi.
- **Không tự phát**: firmware chỉ trả lời. Trả lời đi ra **đúng cổng đã nhận lệnh** (RS422 #1, RS422 #2, FT232 – dùng cổng nào cũng được).
- Điều khiển (`01 01`) và ghi cấu hình (`04 04`) **không ACK**. ACK chỉ có trong nạp code.

| CMD | Hướng | Dài | Việc |
|---|---|---|---|
| `01 01` | PC→PSU | 13 | Điều khiển: `[5]` Mask, `[6]` ClearTrip (0x01 = xoá) |
| `02 02` | PC→PSU | 13 | Hỏi giám sát: `[5..8]` dự phòng → trả `81 81` |
| `81 81` | PSU→PC | 266 | Giám sát |
| `03 03` | PC→PSU | 13 | Hỏi cấu hình → trả `82 82` |
| `82 82` | PSU→PC | 915 | Trả lời cấu hình (cùng bố cục `04 04`) |
| `04 04` | PC→PSU | 915 | Ghi cấu hình (không ACK) |
| `90 90`..`93 93`, `95 95` | PC→PSU | 21 / 269 / 9 / 9 / 9 | BEGIN / DATA / END / COMMIT / INFO |
| `96 96` | PC→PSU | 9 | **ERASE** (mới) |
| `97 97` | PC→PSU | 9 | **ROLLBACK** (mới) |
| `94 94` | PSU→PC | 17 | ACK |

## 2. Giám sát `81 81`
- Cụm **Supply** dùng 4 INA228: INA1 → `I_DCM/V_DCM`, INA2 → `I_5V/V_5V`, INA3 → `I_3V3_1/V_3V3_1`, INA4 → `I_3V3_2/V_3V3_2`.
  Giá trị là **3 byte thanh ghi CURRENT / VBUS nguyên bản** (dữ liệu ở bit 23..4): phía giao diện tự dịch phải 4 bit và xử lý dấu
  (dòng có dấu, áp không dấu). Kênh INA không "tươi" (mất dữ liệu) gửi `00 00 00`.
- 4 cụm DCM, RTC: `00` (chưa có cảm biến / RTC). 10 byte **Trip**: xem mục 5.
- **Vùng dự phòng 24 byte** (byte 239..262 theo cách đếm của tài liệu) là **mở rộng của firmware**, chỉ giữ các mục:

| Offset | Nội dung |
|---|---|
| +0 | `safe_state` (0 BOOT, 1 OFF, 2 ARMING, 3 ON, 4 TRIPPED, 5 LOCKOUT) |
| +1 | trạng thái PWR_EN (1 = đang bật, 0 = tắt; trạng thái logic của chân) |
| +2 | số lần trip liên tiếp |
| +3 | slot firmware đang chạy (`A` / `B`) |
| +4..+7 | version firmware đang chạy (u32 BE) |
| +8..+23 | 0 |

## 3. Điều khiển `01 01`
Phần cứng hiện tại: **một chân PWR_EN = cụm 1** (đóng 4 DCM). Chỉ **bit0** của Mask điều khiển PWR_EN; bit 1..3 bỏ qua.
Mỗi lệnh ghi đè trạng thái bật/tắt. `ClearTrip = 0x01`:
- đang TRIPPED → xoá trip code + các cờ, về OFF; **lệnh bật trong cùng khung bị bỏ qua** (PC gửi lệnh bật ở khung sau);
- trạng thái khác → xoá trip code, bật/tắt theo Mask;
- **LOCKOUT** (≥ 3 lần trip liên tiếp) → bỏ qua mọi lệnh, trip code giữ nguyên; chỉ reset phần cứng mới thoát.
Lệnh bật bị từ chối (đang TRIPPED/LOCKOUT hoặc đang nạp code) → bỏ qua im lặng (không ACK).

## 4. Cấu hình `04 04` / `82 82`
- Phần dữ liệu 904 byte (sau Mask 2 byte) được **lưu nguyên vào Flash** (cfg_store, nhiều bản ghi, chốt bằng CRC32): cụm DCM, INA, ngưỡng Supply, dự phòng.
  `82 82` trả đúng nội dung đã ghi (để PC đọc lại so sánh). Chưa lưu → trả cấu hình mặc định (INA mặc định, còn lại 0).
- Mask `FF FF` coi như ghi đè toàn bộ. **Chỉ nhận khi SAFE_OFF** (đổi `SHUNT_CAL` khi nguồn bật làm sai thang đo); ngoài ra bỏ qua im lặng.
- **INA**: 9 thanh ghi/INA nạp thẳng vào IC (khi ghi và khi khởi động). `SHUNT_CAL = 0` ⇒ dùng mặc định của INA đó (`ina228_cfg.h`); bit RST/RSTACC của CONFIG bị xoá.
- **Ngưỡng dưới của INA (`SUVL`, `BUVL`) chỉ được nạp khi SAFE_ON**; lúc OFF/ARMING IC giữ giá trị "tắt".
  Lý do (C3): điện áp bus thấp hơn `BUVL` khi nguồn tắt/đang lên là bình thường, nhưng chân ALERT (ALATCH=1) bị kéo xuống liên tục và
  `Safety_FastTrip` (cắt ngay ở ARMING) có thể cắt nhầm. Ngưỡng *trên* (`SOVL`, `BOVL`, `TEMP_LIMIT`) vẫn nạp luôn → bảo vệ quá dòng ở ARMING giữ nguyên tốc độ.
  **Chưa kiểm chứng trên board.**

## 5. Trip code và so sánh ngưỡng (STM tự so)
- Ngưỡng Supply (`8 × (MAX, MIN) × 3 byte`) lấy từ cấu hình đã lưu; so với **giá trị thô cùng đơn vị bản tin giám sát**.
  Dòng so sánh **có dấu 24 bit**, áp so sánh không dấu. **Giá trị 0 = tắt ngưỡng đó** (cả MAX lẫn MIN).
- Chỉ so khi **SAFE_ON**; cần `PSU_TRIP_CONFIRM_N` (= 2) mẫu mới liên tiếp (~60 ms/mẫu) vượt ngưỡng mới latch.
- Trip code **latch cho đến khi nhận ClearTrip** (không tự xoá khi giá trị trở lại bình thường; chưa lưu qua reset).
- **Bảng bit dưới đây là TẠM THỜI** – chờ bảng chính thức (sửa ở `psu_trip.h`):

| Byte | Ý nghĩa |
|---|---|
| TRIP1 | bit k = giá trị Supply k vượt **MAX** (k: 0 I_DCM, 1 V_DCM, 2 I_5V, 3 V_5V, 4 I_3V3_1, 5 V_3V3_1, 6 I_3V3_2, 7 V_3V3_2) |
| TRIP2 | bit k = giá trị Supply k dưới **MIN** |
| TRIP3 | bit c = INA c báo SHNTOL/POL (quá dòng/công suất); bit 4+c = SHNTUL |
| TRIP4 | bit c = BUSOL (quá áp bus); bit 4+c = BUSUL (sụt áp bus) |
| TRIP5 | bit c = TMPOL (quá nhiệt INA); bit 4+c = chân FT của DCM c |
| TRIP6 | bit0 mất liên lạc I²C; bit1 cắt do ALERT nhưng không xác nhận nguyên nhân; bit2 LOCKOUT; bit3 cấu hình INA không hợp lệ |
| TRIP7..10 | 0 |

## 6. Nạp code
Trình tự: `INFO` (kiểm kết nối, slot, version) → `ERASE` (xoá slot đích; **một ACK duy nhất khi xoá xong**, `Info = 1`;
timeout phía PC nên ≥ 15 s) → `BEGIN` (size, CRC32, version; ACK ngay; chỉ sau ERASE, nếu không `NOT_ERASED`) →
`DATA` (256 B/gói, gói cuối đệm `FF`, khung luôn 269 B) → `END` → `COMMIT`.
- ACK `94 94` (17 byte): `[5]` AckCmd (1 byte, ví dụ `96` cho ERASE), `[6]` status, `[7..10]` info, `[11]` slot (`A`/`B`; **`R` ở recovery**),
  `[12]` trạng thái trial chỉ có ở trả lời INFO (bit7 confirmed, bit6..0 boot_count).
- Status: 0 OK, 1 Refused (không ở SAFE_OFF), 2 BadSize, 3 BadSeq, 4 CrcFail, 5 Busy, 6 NoCommitPending, 7 EraseFail, 8 WrongSlot,
  **9 ProgFail** (lỗi ghi flash, info = mã HAL/địa chỉ), **10 NoTarget** (ROLLBACK: slot kia không khởi động được), **11 NotErased** (BEGIN khi chưa ERASE).
- `ROLLBACK (97 97)`: đánh dấu slot đang chạy hỏng và reset, có ACK; chỉ khi SAFE_OFF và slot kia khởi động được.
- Bootloader (recovery, khi cả hai slot hỏng) nói cùng giao thức: `INFO`, `ERASE` (xoá cả hai slot), `BEGIN`, `DATA`, `END`, `COMMIT`; slot trả lời `R`.
  Stop-and-wait: PC phải chờ ACK rồi mới gửi khung kế.

## 7. Còn mở
1. Bảng trip code chính thức (mục 5). 2. Đổi CRC sang "từ ADDR" (một macro). 3. RTC, 4 cụm DCM: chưa có nguồn dữ liệu.
4. C3 (ngưỡng dưới INA khi OFF/ARMING): cần đo trên board. 5. Cổng nào là đường Gateway/dự phòng (hiện cả 3 cổng nhận mọi lệnh).
