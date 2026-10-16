# PSU-Firmware – luồng hoạt động hiện tại (bản chụp trước khi cải cách)

Tài liệu mô tả đúng những gì code đang làm (nhánh `claude/stoic-maxwell-314gkk`, sau commit "OTA nhóm D").
Chưa có gì trong tài liệu này được kiểm chứng trên phần cứng; những chỗ "chưa đo" được ghi rõ.

## 1. Ba chương trình, một flash

```
 Flash 1 MB (STM32H725, 8 sector × 128 KB)
 ┌────────┬───────────────┬───────────────┬────────┬───────────────┐
 │ sec 0  │ sec 1–2       │ sec 3–4       │ sec 5  │ sec 6 │ sec 7 │
 │ BOOT   │ SLOT A (256K) │ SLOT B (256K) │ trống  │ CFG 0 │ CFG 1 │
 │ 128 KB │ hdr 1KB + app │ hdr 1KB + app │        │ (2 bank luân  │
 │        │               │               │        │  phiên)       │
 └────────┴───────────────┴───────────────┴────────┴───────────────┘
   stm32h725_ota/    PSU_stm32h7/ (build SlotA / SlotB, 2 linker script)
   shared_lib/ (dùng chung: flash, cfg_store, ota_*, crc, flash_guard)
```

- `stm32h725_ota/` – bootloader (HSI 64 MHz, không PLL, không HAL UART).
- `PSU_stm32h7/` – ứng dụng (4 kênh INA228 qua I²C, 3 cổng UART, điều khiển PWR_EN, LED, OTA).
- `shared_lib/` – compile vào cả hai dự án (thư mục liên kết trong CubeIDE).
- `tests/host/` – test chạy trên PC với flash giả và phần cứng giả (5 bộ).

## 2. Bootloader

```
 RESET → MPU → HAL_Init → FlashGuard_Init → IWDG_Start(8 s) → clock HSI 64 MHz → GPIO(LED)
   → Boot_Run:
        CfgStore_Init            quét cfg bank 0/1, chọn bank có COMMIT mới nhất
        Ota_SelectSlot           mỗi slot: ảnh hợp lệ (CRC32 + vector) && !rollback && (đã confirm || count<3)
                                 nhiều ứng viên: install_seq lớn hơn, hoà thì version
          ├─ không có slot ─────► Boot_RecoveryRun()  (không trở về)
          └─ có slot ───────────► Ota_CommitTrial: chưa confirm → ghi boot_count+1
                                  (ghi lỗi & slot kia đã confirm → dùng slot kia)
                                  jump_to_slot: DeInit, tắt SysTick/NVIC/MPU, VTOR, MSP, bx entry
```

Trạng thái trial của một slot (bản ghi BOOT trong cfg): `không có → TRIAL(count 0..2) → CONFIRMED`
hoặc `→ EXHAUSTED (count≥3, không bao giờ chọn lại)`, hoặc `REJECTED` (count=0xFFFFFFFF, do ROLLBACK).
Nạp lại slot (OTA/recovery) ghi `count=0, confirmed=0` → về TRIAL.

### Recovery (cả hai slot không dùng được)
```
 R_IDLE ──BEGIN(size hợp lệ)→ xoá CẢ HAI slot → R_RECEIVING ──DATA(seq đúng)→ ghi+đọc lại (lặp)
 R_RECEIVING ──END→ CRC32 tổng → cấp install_seq → ghi header (cuối) → ImageValid+VectorOk → ghi boot record → R_VERIFIED
 R_VERIFIED ──COMMIT→ ACK → reset → bootloader chọn slot mới
 lỗi bất kỳ → R_IDLE (vẫn chưa có ảnh hợp lệ → lần reset sau lại vào recovery)
```
UART mức thanh ghi, polling, 115200 8N1: UART5 (RS422 #1), USART1 (RS422 #2), UART4 (FT232).
Mỗi byte vào `Boot_RecoveryFeed` (tách khung AB CD CMD … CRC16 E1 E2); ACK gửi blocking, có điều khiển DE.
Giao thức stop-and-wait: host phải chờ ACK mới gửi khung kế (không có bộ đệm khi CPU bận ghi flash).

## 3. Ứng dụng

### 3.1 Khởi tạo (`main.c`, `app_main.c`)
```
 MPU → ICache → HAL_Init → clock → App_EarlyInit(FlashGuard_Init, Wdg_Init = IWDG 8 s)
 → MX_GPIO (PWR_EN = thấp) → DMA → UART4/5/USART1 → I2C1 → USB (CDC, chưa nối vào Comm)
 → App_Init: Led_Init → Safety_Init (SAFE_BOOT, PWR_EN tắt) → FwUpdate_Init → CfgStore_Init
            → ina228_bus_init → ina228_init_all → apply_saved_limits (ghi ngưỡng lưu vào INA)
            → Comm_Init (bật DMA RX vòng cho 3 cổng) → xác định self_confirmed → SelfTest_Reset
```

### 3.2 Vòng lặp chính (một siêu vòng lặp, không RTOS) – thứ tự thực tế
```
 App_Loop (chạy liên tục):
   ina228_bus_tick           timeout giao dịch I²C (10 ms)
   mỗi 10 ms   dcm_poll (đọc 4 chân FT của DCM, bỏ phiếu 2/3) ; Comm_TxWatchdog
   mỗi 60 ms   Comm_OnTick → ina228_bus_start (quét 4 kênh bằng I²C DMA, bỏ qua nếu đang OTA)
   Comm_RxPoll               tách khung từ buffer DMA vòng, thực thi lệnh
   Comm_Poll                 nếu quét xong (DONE): thu kết quả → tlm_build → gửi telemetry 3 cổng
   nếu bus IDLE: ina228_alert_process (đọc DIAG_ALRT), Comm_CfgTask (SET/GET_LIMIT),
                 mỗi 250 ms reinit_stale_dev
   Safety_Task               máy trạng thái bảo vệ + cập nhật LED
   FwUpdate_Task             xoá sector (mỗi lần gọi 1 sector), timeout phiên, reset sau COMMIT/ROLLBACK
   Comm_StatusTask           khung STATUS 0x03 (mỗi 1 s hoặc khi trạng thái đổi)
   Comm_FwPoll               báo "xoá xong"/lỗi xoá cho PC
   Led_Task ; self_confirmed_task (self-test → ghi confirm)
   Wdg_CheckIn(LOOP), Wdg_CheckIn(I2C nếu bus không BUSY) ; Wdg_Task (đủ cả hai mới feed IWDG)
```

### 3.3 Ngắt (ưu tiên số nhỏ = cao hơn)
| Nguồn | Ưu tiên | Việc làm |
|---|---|---|
| BusFault (FlashGuard) | 0 | bỏ lệnh load lỗi ECC; lỗi khác → `Fault_Hook` + chờ IWDG |
| I²C1 EV/ER, DMA1 S0/S1 (I²C) | 3 | tiến máy trạng thái quét INA (đọc thanh ghi kế tiếp) |
| EXTI ALERT1..4 (PA4..PA7) | 4 | `Safety_FastTrip` (cắt PWR_EN nếu ON/ARMING) + `ina228_alert_isr` |
| UART4/5/USART1, DMA TX | 5 | hoàn tất TX → hạ DE, nhả cổng |
| DMA RX UART | 6 | (RX vòng, không xử lý trong ngắt) |
| SysTick | 15 | `HAL_IncTick` |

### 3.4 Các khối chức năng

**INA228 (BSP/ina228_driver.c)** – 4 con tại 0x40/0x41/0x44/0x45, R_shunt 1 mΩ, ADC 16 mẫu (~50 ms/chu kỳ).
Quét: mỗi kênh đọc ADC_CONFIG, VBUS, CURRENT, DIETEMP bằng `HAL_I2C_Mem_Read_DMA`, chuỗi tiến trong ngắt
(`BUSY → DONE → IDLE`). Ghi/đọc ngưỡng và `DIAG_ALRT` bằng I²C blocking (timeout 5 ms × 3 lần), chỉ khi bus IDLE.
`DIAG_ALRT` có ALATCH=1: chân ALERT giữ thấp tới khi đọc thanh ghi.

**Safety (App/safety.c)** – chân PWR_EN dùng chung cho 4 kênh.
```
 SAFE_BOOT(500 ms) → SAFE_OFF ──PC bật & ≥100 ms──► SAFE_ARMING(400 ms) ──► SAFE_ON
 SAFE_ON/ARMING ──(lỗi INA, FT của DCM, hoặc FastTrip từ ISR)──► SAFE_TRIPPED
 SAFE_TRIPPED ──CLR_FAULT──► SAFE_OFF            SAFE_TRIPPED với trip_count≥3 ──► SAFE_LOCKOUT (chỉ reset mới thoát)
 trip_count về 0 sau 10 s chạy ổn ở SAFE_ON (không bị CLR_FAULT reset)
```
Mã lỗi: OVERCUR, OVERVOLT, UNDERVOLT, OVERTEMP, DCM (FT), BUS. Trip từ ISR chưa biết mã; sau 100 ms
phân loại từ diag (cả ở TRIPPED lẫn LOCKOUT). Mất liên lạc I²C chỉ cảnh báo (`SAFE_TRIP_ON_BUS_LOSS = 0`).
Mọi fault không phục hồi (HardFault, MemManage, UsageFault, BusFault, NMI, `Error_Handler`, `Default_Handler`)
gọi `Safety_ForceOff()` rồi chờ IWDG.

**Comm (App/comm.c)** – 3 cổng 115200 8N1: RS422 #1 (UART5), RS422 #2 (USART1), FT232 (UART4).
- TX telemetry 56 byte (`AB CD 01 | status | 4×12 byte kênh | CRC16 | E1 E2`) mỗi ~60 ms, ping-pong buffer, DMA TX.
- Khung phụ 16 byte (STATUS 0x03, LIMIT_ACK 0x8A, FW_ACK 0x94) dùng chung `aux_acquire` (chỉ cấp khi không cổng nào đang bận).
- RX: DMA vòng 512 byte/cổng; `rx_feed` tách khung theo độ dài cố định từng lệnh.
- Lệnh PC→thiết bị: `0x88` PWR_CTRL, `0x89` SET_LIMIT, `0x8C` GET_LIMIT, `0x8B` CLR_FAULT,
  `0x90..0x93` FW_BEGIN/DATA/END/COMMIT, `0x95` FW_INFO, `0x96` FW_ROLLBACK. Địa chỉ trong khung không được lọc.

**Cấu hình (shared_lib/cfg_store.c)** – log bản ghi 64 byte (LIMITS, BOOT, INSTALL, COMMIT), 2 bank luân phiên
(compaction: xoá bank kia, ghi snapshot, ghi COMMIT cuối). Ngưỡng PC đặt (`SET_LIMIT`) lưu ở đây và được
nạp lại vào INA lúc khởi động. Ghi cần xoá sector bị hoãn khi PWR_EN đang bật.

**OTA phía app (App/fw_update.c)** – chỉ chạy khi `SAFE_OFF`.
```
 IDLE ──BEGIN──► ERASING (xoá slot kia, mỗi lần FwUpdate_Task 1 sector, ACK khi xong)
      ──► RECEIVING (DATA tuần tự, mỗi chunk 256 B; timeout 5 s) ──END──► VERIFIED (timeout 15 s)
 VERIFIED ──COMMIT──► reset sau 50 ms        ROLLBACK: đánh dấu slot đang chạy REJECTED rồi reset (có ACK)
```
Khi OTA bận: dừng quét INA, telemetry, STATUS, SET/GET_LIMIT, và từ chối lệnh bật nguồn; `Safety_Task` vẫn chạy.
Ảnh mới: ghi header ở cuối (1 flash-word), kiểm lại CRC từ flash, ghi boot record `count=0, confirmed=0`.

**Tự xác nhận (App/self_test.c)** – confirm khi liên tục 10 s: ≥1 kênh INA tươi + cấu hình đúng, safety đã rời BOOT.

**LED / Watchdog** – 3 LED (xanh lá: sống/giao tiếp, xanh dương: PWR_EN bật, đỏ: mã lỗi theo số nháy), bitmap 32 khe × 100 ms.
IWDG 8 s, chỉ feed khi cả `WDG_ID_LOOP` và `WDG_ID_I2C` đã check-in.

## 4. Các luồng dữ liệu chính

```
 Đo lường:  INA228 ─I²C DMA─► dev.raw/meas ─► tlm_build ─► 3 cổng UART (telemetry 60 ms)
 Bảo vệ A:  ALERT pin ─EXTI(prio 4)─► Safety_FastTrip: PWR_EN tắt ngay ─► Safety_Task phân loại ─► TRIPPED
 Bảo vệ B:  DIAG_ALRT (đọc định kỳ/ALERT) ─► diag_sticky ─► Safety_Task (mask theo trạng thái) ─► TRIPPED
 Bảo vệ C:  FT của DCM ─dcm_poll 10 ms, bỏ phiếu 2/3─► Safety_Task ─► TRIPPED (DCM)
 Lệnh PC:   UART RX DMA ─► rx_feed ─► rx_exec ─► Safety / Cfg / FwUpdate ─► ACK (khung phụ 16 B)
 Cấu hình:  SET_LIMIT ─► ghi INA (I²C) ─► CfgStore (flash, hoãn nếu nguồn ON và cần xoá) ─► LIMIT_ACK
 OTA:       FW_BEGIN..COMMIT ─► slot kia ─► reset ─► Bootloader chọn slot ─► app ─► self-test ─► confirm
```

## 5. Điểm ghép nối và nợ kỹ thuật (đầu vào cho cải cách)

Đây là quan sát từ code, chưa phải kết luận thiết kế.

1. **Siêu vòng lặp duy nhất.** Thứ tự gọi quyết định hành vi; một lệnh chặn (ghi/xoá flash, I²C blocking) làm trễ mọi thứ,
   kể cả `Safety_Task`. Xoá sector chặn cả ngắt (single-bank, chạy từ cùng flash).
2. **Phụ thuộc vòng giữa các module:** `safety` ↔ `comm` (DcmFault, PcLinkAlive) ↔ `fw_update`; `led` nhận trạng thái từ `safety`;
   `safety` truy cập thẳng mảng thiết bị INA. Giao thức đặt trong `comm.h`, một phần lặp lại ở `shared_lib/ota_proto.h`.
3. **`dcm_poll` nằm trong `comm.c`** nhưng là đầu vào của safety; phát hiện lỗi FT là thăm dò 10 ms + bỏ phiếu 2/3
   (độ trễ phát hiện cỡ 10–30 ms, chưa đo), khác với đường ALERT bằng ngắt.
4. **Ưu tiên ngắt:** I²C/DMA (3) cao hơn EXTI ALERT (4) → `Safety_FastTrip` có thể bị trễ bởi một ngắt I²C đang chạy.
5. **C3 chưa xử lý:** `Safety_FastTrip` không áp mặt nạ BUSUL ở `SAFE_ARMING`; có thể trip nhầm khi `BUVL > 0` (cần đo).
6. **Giao thức không xác thực, không lọc địa chỉ, khung FW_DATA dài cố định 268 byte;** chưa có chữ ký ảnh, chưa chống hạ cấp.
7. **USB CDC được khởi tạo nhưng chưa nối vào `Comm`** (callback nhận chỉ re-arm).
8. **Bootloader recovery dùng polling, stop-and-wait;** không có timeout phiên, không có đường vào bootloader từ app.
9. **Trạng thái nằm trong biến `static` của từng module;** chỉ `cfg_store`, chọn slot, recovery, safety, self-test có test host;
   `comm.c`, `fw_update.c`, driver INA228, `led.c` chưa có test.
10. **Hai bản sao startup/hal_conf** (app và bootloader); chỉnh tay `Default_Handler` bị CubeMX ghi đè khi generate lại.
11. **Mã UI (Qt) đã bị bỏ khỏi repo** nên không còn nơi tham chiếu phía PC cho giao thức.
12. **Chưa build bằng toolchain ARM, chưa chạy trên board** cho các thay đổi gần đây (layout 2 sector, cfg 2 bank, recovery UART, chọn slot, self-test).

## 6. Bản đồ kiểm thử (`make -C tests/host test`)
| Bộ test | Phủ |
|---|---|
| `cfg_store_test` | cfg 2 bank, compaction, cắt điện tại từng thao tác flash |
| `recovery_test` | giao thức recovery, ghi slot, cắt điện từng thao tác |
| `select_test` | chọn slot, hết lượt, rollback, `Ota_CommitTrial` |
| `safety_test` | máy trạng thái safety (safety.c thật + phần cứng giả) |
| `self_test_test` | điều kiện confirm |
