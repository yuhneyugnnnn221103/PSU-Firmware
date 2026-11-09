# Hướng dẫn phía phần mềm PC: SET/GET_VERSION và bỏ version u32

Đối tượng: người sửa phần mềm PC (ACTL Tool) để dùng **version dạng chuỗi** (tên file `.bin`) thay cho số version `u32` cũ.
Đánh số byte trong tài liệu này **từ 1** (`AB` = byte 1), giống `psu_messages.md`; cột "(từ 0)" ghi thêm cho ai đọc code firmware.
Khung mẫu có CRC do firmware sinh: `docs/PSU_FRAME_EXAMPLES.md`. Code tham chiếu đã test: `tools/pc_reference/`.

> **PC và firmware phải cập nhật cùng lúc.** Firmware mới không còn nhận `BEGIN` 21 byte (xem mục 2). Phải nạp lại **cả bootloader và app**.

## 1. Tóm tắt thay đổi

| Hạng mục | Trước | Nay |
|---|---|---|
| Version | số `u32` khai báo trong `BEGIN` (mặc định thời điểm hiện tại), hiện ở INFO và `81 81` | **chuỗi ASCII 32 byte** = tên file `.bin` bỏ đuôi, gửi bằng `SET_VERSION`, hỏi bằng `GET_VERSION` |
| `BEGIN` `90 90` | 21 byte: Size, CRC32, **Version** | **17 byte**: Size, CRC32 |
| ACK của INFO (`Info`, byte 8–11) | version | `00 00 00 00` |
| `81 81` vùng mở rộng +13..+16 (byte 252–255) | `FW.VERSION` | `00` (dự phòng); **bỏ khỏi bộ phân tích và giao diện** |
| Bản tin mới | - | `9A 9A` SET_VERSION, `99 99` GET_VERSION, `A9 A9` trả lời |

## 2. Đặc tả bản tin

### 2.1 BEGIN `90 90` (17 byte) – đã đổi

| Byte (từ 1) | (từ 0) | Dài | Trường | Ý nghĩa |
|---|---|---|---|---|
| 1–2 | 0–1 | 2 | Header | `AB CD` |
| 3–4 | 2–3 | 2 | CMD | `90 90` |
| 5 | 4 | 1 | Addr | Địa chỉ PSU |
| 6–9 | 5–8 | 4 | Size | Kích thước file `.bin` (byte), 1..0x9FC00 |
| 10–13 | 9–12 | 4 | CRC32 | CRC32 (zlib/IEEE) của toàn bộ file |
| 14–15 | 13–14 | 2 | CRC16 | Từ byte 5 đến byte 13 |
| 16–17 | 15–16 | 2 | Đuôi | `E1 E2` |

Ví dụ (Addr = 1, Size = 0x1000, CRC32 = DEADBEEF): `AB CD 90 90 01 00 00 10 00 DE AD BE EF 2D 82 E1 E2`

### 2.2 SET_VERSION `9A 9A` (41 byte) – mới, PC → PSU

| Byte (từ 1) | (từ 0) | Dài | Trường | Ý nghĩa |
|---|---|---|---|---|
| 1–2 | 0–1 | 2 | Header | `AB CD` |
| 3–4 | 2–3 | 2 | CMD | `9A 9A` |
| 5 | 4 | 1 | Addr | Địa chỉ PSU |
| 6–37 | 5–36 | 32 | Chuỗi version | ASCII `0x20..0x7E`, đệm `00` |
| 38–39 | 37–38 | 2 | CRC16 | Từ byte 5 đến byte 37 |
| 40–41 | 39–40 | 2 | Đuôi | `E1 E2` |

- **Chỉ có tác dụng trong recovery** và **chỉ trong khoảng sau `ERASE` đến trước `END`**. PSU giữ chuỗi trong RAM rồi ghi vào header của ảnh khi nhận `END`; do đó version luôn đi cùng đúng ảnh đó.
- ACK `94 94` (AckCmd = `9A`): `Status 0` = OK; `5` (Busy) = gửi sai thời điểm (chưa `ERASE`, hoặc đã `END`); `12` (NotRecovery) = đang ở app.
- Gửi lại nhiều lần được; lần sau ghi đè lần trước. `ERASE` xoá chuỗi của phiên cũ. Không gửi thì chuỗi rỗng.
- Byte ngoài `0x20..0x7E` (kể cả `FF`) cắt chuỗi tại đó.

### 2.3 GET_VERSION `99 99` (9 byte) – mới, PC → PSU, chạy ở app và recovery

`AB CD 99 99 Addr CRC16 E1 E2`: CRC16 ở byte 6–7 (tính trên đúng 1 byte, byte 5). Mẫu (Addr = 1): `AB CD 99 99 01 F1 D1 E1 E2`

### 2.4 Trả lời `A9 A9` (41 byte) – mới, PSU → PC

Cùng bố cục `SET_VERSION` (CMD = `A9 A9`; byte 6–37 là chuỗi). Chuỗi **rỗng (toàn `00`)** nghĩa là: chưa đặt, đang giữa phiên nạp (chưa `END`), không có ảnh hợp lệ, hoặc ảnh nạp bằng SWD (không có header).
- Ở **app**: chuỗi của ảnh đang chạy. Ở **recovery**: chuỗi của ảnh đang có trong Flash (nếu header hợp lệ).
- PSU trả lời ra đúng cổng đã nhận lệnh.

## 3. Quy tắc tạo chuỗi version từ tên file

1. Lấy tên file không kèm thư mục, **bỏ phần mở rộng** (`PSU_v1.2.3_abc1234.bin` → `PSU_v1.2.3_abc1234`).
2. Chỉ ký tự in được `0x20..0x7E`. Ký tự khác (dấu tiếng Việt…) → thay bằng `_` và **cảnh báo người dùng**.
3. Dài tối đa **32** ký tự. Dài hơn → **từ chối và yêu cầu đổi tên file**. Không tự cắt vì dễ mất phần mã commit ở cuối.
4. Gợi ý quy ước tên file: `PSU_v<major>.<minor>.<patch>_<7 ký tự git>.bin`.

## 4. Tích hợp vào luồng nạp code

```
INFO                                   (như cũ: biết slot A hay R)
[ENTER_REC → chờ → INFO lặp đến khi R] (như cũ, bỏ qua nếu đã ở R)
ERASE            (chờ đến 40 s)        (như cũ)
BEGIN  (17 byte, KHÔNG còn Version)    <-- đổi
SET_VERSION      (chờ ACK 200 ms)      <-- mới: ngay sau ACK của BEGIN
DATA × N                               (như cũ)
END
COMMIT
(chờ ~1 s)  INFO                       (slot 'A')
GET_VERSION      (chờ 200 ms)          <-- mới: so với chuỗi vừa gửi, báo kết quả
```

- `SET_VERSION`: chờ ACK 200 ms, thử lại tối đa 3 lần. Không có ACK hoặc status khác `0` thì **dừng** và báo lỗi (hoặc cho người dùng chọn nạp không có version).
- Sau `COMMIT` và khi PSU đã ở app (`INFO` trả slot `A`), gửi `GET_VERSION`. Chuỗi trả về phải **bằng** chuỗi đã gửi; khác thì cảnh báo ("version đọc lại không khớp").
- `GET_VERSION` có thể gửi bất kỳ lúc nào (app hoặc recovery); không cần hỏi định kỳ. Nên hỏi khi kết nối, sau mỗi lần nạp, và khi người dùng bấm "Đọc version".

## 5. Giao diện gợi ý

- Thêm cột/nhãn **Firmware** cạnh mỗi PSU: chuỗi version; kèm `A` (app) hoặc `R` (recovery) và trạng thái xác nhận từ `INFO` (byte 13: bit7 = đã confirm).
- Chuỗi rỗng: hiện "(chưa đặt)". Slot `R`: hiện "Recovery".
- Nhật ký nạp ghi: tên file, version gửi, version đọc lại, kết quả.

## 6. Việc cần **xoá** trong code PC (version u32 cũ)

- Trường Version trong bộ dựng `BEGIN` (khung còn 17 byte) và ô nhập/giá trị mặc định "thời điểm hiện tại" trên giao diện nạp.
- Phân tích `Info` của ACK INFO thành version (nay luôn `0`).
- Ánh xạ `FW.VERSION` (byte 252–255 của `81 81`) và hiển thị của nó; các byte đó giữ `00`.
- Cập nhật `psu_messages.md`: xem bảng mục 7.

## 7. Dòng cần sửa trong `psu_messages.md` (bản bạn gửi)

| Vị trí | Sửa |
|---|---|
| Bảng tổng hợp bản tin (mục 1) | `STM32 BEGIN`: độ dài 21 → **17**, ghi chú "Kích thước, CRC32"; **thêm** 3 dòng: `99 99` (9 B, hỏi version), `9A 9A` (41 B, đặt version), `A9 A9` (41 B, trả lời version) |
| Mục 2.5, dòng `+13..+16` (byte 252–255) | `FW.VERSION` → **dự phòng `00`**; bỏ khỏi bảng trường |
| Mục 5, bảng lệnh | BEGIN: 21 → 17; thêm `SET_VERSION`, `GET_VERSION`, trả lời |
| Mục 5.1 BEGIN | Bố cục 17 byte (mục 2.1); xoá dòng Version byte 14–17 |
| Mục 5.4 ACK, dòng Info | Bỏ "INFO = version" (nay `0`); thêm AckCmd `9A` |
| Mục 5.5 quy trình | Thêm bước SET_VERSION sau BEGIN (bước 4) và GET_VERSION sau COMMIT/INFO (bước 7) |
| Mục 5.5 bước 4 | Bỏ "version (mặc định app lấy thời điểm hiện tại)" |

## 8. Code tham chiếu

- C++17 (không phụ thuộc Qt): `tools/pc_reference/psu_version_proto.hpp`: `crc16CcittFalse`, `buildFrame`, `buildBegin`, `versionFromFileName`, `buildSetVersion`, `buildGetVersion`, `parseVersionReply`.
  Test: `g++ -std=c++17 -Wall -Wextra tools/pc_reference/psu_version_test.cpp -o psu_version_test && ./psu_version_test`.
- Python: `tools/pc_reference/psu_version_demo.py`. `--selftest` đối chiếu vector firmware không cần phần cứng; `--port COM5 --addr 1 get|info|set FILE.bin` thử trên cổng thật (cần `pyserial`).
- Cả hai đã được đối chiếu **từng byte** với khung firmware sinh ra (`docs/PSU_FRAME_EXAMPLES.md`).

## 9. Vector kiểm tra (Addr = 1)

| Khung | Hex |
|---|---|
| BEGIN, Size 0x1000, CRC32 DEADBEEF | `AB CD 90 90 01 00 00 10 00 DE AD BE EF 2D 82 E1 E2` |
| GET_VERSION | `AB CD 99 99 01 F1 D1 E1 E2` |
| SET_VERSION `PSU_v1.2.3_abc123` | xem `docs/PSU_FRAME_EXAMPLES.md`, mục `9A 9A SET_VERSION` |
| Trả lời `A9 A9` `PSU_v1.2.3_abc123` | xem `docs/PSU_FRAME_EXAMPLES.md`, mục `A9 A9` |
| CRC16-CCITT-FALSE("123456789") | `0x29B1` |

## 10. Kiểm thử trên board thật

| # | Thao tác | Mong đợi |
|---|---|---|
| 1 | Recovery, chưa nạp: `GET_VERSION` | `A9 A9`, chuỗi rỗng |
| 2 | `SET_VERSION` khi chưa `ERASE` | ACK status 5 (Busy) |
| 3 | `ERASE` → `BEGIN` (17 byte) → `SET_VERSION` → `DATA`… → `END` → `COMMIT` | Mọi ACK status 0 |
| 4 | Sau reset, app: `INFO` | Slot `A`, Info = `0` |
| 5 | `GET_VERSION` | Đúng chuỗi đã gửi |
| 6 | Nạp lại **không** gửi `SET_VERSION` | `GET_VERSION` trả chuỗi rỗng |
| 7 | App đang chạy: `SET_VERSION` | ACK status 12 (NotRecovery) |
| 8 | Chuỗi chứa byte `FF` giữa chừng | Chuỗi bị cắt tại byte đó |
| 9 | Dùng `BEGIN` 21 byte (cũ) | Không có ACK hợp lệ (xác nhận PC mới phải dùng 17 byte) |
