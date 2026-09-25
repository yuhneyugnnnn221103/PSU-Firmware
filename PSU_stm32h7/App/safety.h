/**
 ******************************************************************************
 * @file    safety.h
 * @brief   Lop bao ve cuc bo: phat hien loi tu INA228 / DCM -> cat PWR_EN.
 *
 * NGUYEN TAC:
 *   1. Chan PWR_EN_CTRL la MOT chan dung chung cho ca 4 kenh (theo schematic).
 *      => Loi o BAT KY kenh nao cung cat nguon CA HE THONG. Khong the lam khac.
 *   2. Duong nhanh (EXTI) cat nguon truoc, phan loai loi sau. Do tre ~2 us
 *      thay vi vai chuc ms neu doi ina228_alert_process().
 *   3. LATCH cung. Chi PC gui CLR_FAULT roi PWR_CTRL(on) moi bat lai duoc.
 *      Khong tu dong thu lai.
 *   4. Khi nguon TAT thi V_bus = 0 -> BUSUL luon dung. Bit nay bi mask trong
 *      moi trang thai khac SAFE_ON, neu khong se khong bao gio thoat duoc latch.
 *
 * CANH BAO PHAN CUNG:
 *   Macro SAFE_PWR_ACTIVE_HIGH phai khop voi tang cong suat thuc te.
 *   Neu dat sai, mach se BAT NGUON ngay luc boot truoc khi Safety_Init() chay.
 *   DO BANG DONG HO TRUOC KHI NAP FIRMWARE.
 ******************************************************************************
 */

#ifndef SAFETY_H_
#define SAFETY_H_

#include <stdint.h>
#include <stdbool.h>

/* ==========================================================================
 * CAU HINH
 * ========================================================================== */

/* 1 = GPIO muc CAO bat nguon. 0 = GPIO muc THAP bat nguon. */
#ifndef SAFE_PWR_ACTIVE_HIGH
#define SAFE_PWR_ACTIVE_HIGH   1
#endif

/* Bo qua BUSUL trong cua so nay sau khi bat nguon. Datasheet DCM4623 cho
 * thoi gian tu EN len muc cao toi khi ngo ra on dinh o dien ap danh dinh
 * dao dong 16-300 ms (KHONG dung chan FT de xac nhan: FT nha rat som,
 * ~200 us sau EN, khong lien quan gi toi thoi diem dien ap thuc su on
 * dinh - xem safety.c). Dat cao hon bien tren 300 ms mot khoang du de
 * khong nam sat ranh gioi datasheet (dung sai nhiet do, linh kien, tai).
 * Chinh lai neu do thuc te tren board cho thay can khac. */
#define SAFE_ARM_MS            400u

/* Datasheet DCM4623: tOFF-MONOTONIC = 100 ms - "minimum time a module
 * needs to be in the disabled state before it is GUARANTEED to exhibit
 * monotonic soft-start and have predictable startup timing". Neu EN bi
 * ha roi keo len lai nhanh hon moc nay, chinh cam ket tSS <= 300 ms
 * (dung de tinh SAFE_ARM_MS o tren) khong con dam bao nua. tOFF-MIN
 * (2 ms) la san thap hon, duoi do module co the tu choi khoi dong han;
 * 100 ms la san AN TOAN de giu dung gia dinh ve timing. */
#define SAFE_OFF_MIN_MS        100u

/* Sau khi cat nguon, cho V_bus xa roi moi tin cac co doc duoc tu INA228. */
#define SAFE_SETTLE_MS         100u

/* Cho INA228 hoan tat chu ky do dau tien truoc khi roi SAFE_BOOT. */
#define SAFE_BOOT_MS           500u

/* So lan trip lien tiep truoc khi vao LOCKOUT vinh vien. */
#define SAFE_TRIP_LOCKOUT_N    3u

/* Chay on dinh du lau thi coi nhu su co truoc do da duoc khac phuc. */
#define SAFE_STABLE_MS         10000u

/* Mat lien lac I2C voi INA228 co cat nguon khong?
 * 0 = chi canh bao (LED do nhay), khong cat. 1 = cat nguon.
 * De 0: mot glitch I2C khong nen lam sap tai. */
#define SAFE_TRIP_ON_BUS_LOSS  0

/* ==========================================================================
 * KIEU DU LIEU
 * ========================================================================== */

typedef enum {
    SAFE_BOOT    = 0,  /* chua arm, bo qua moi ALERT                  */
    SAFE_OFF     = 1,  /* nguon tat theo lenh PC (khong phai loi)     */
    SAFE_ARMING  = 2,  /* vua bat nguon, dang cho qua inrush          */
    SAFE_ON      = 3,  /* chay binh thuong, giam sat day du           */
    SAFE_TRIPPED = 4,  /* da cat nguon do loi, cho PC xoa             */
    SAFE_LOCKOUT = 5   /* het so lan thu lai -> khoa vinh vien        */
} safe_state_t;

/* Ma loi = so nhay den DO. Gia tri NHO hon = uu tien cao hon (nguyen nhan goc). */
typedef enum {
    FCODE_NONE      = 0,
    FCODE_OVERCUR   = 1,  /* SHNTOL / SHNTUL / POL */
    FCODE_OVERVOLT  = 2,  /* BUSOL                 */
    FCODE_UNDERVOLT = 3,  /* BUSUL                 */
    FCODE_OVERTEMP  = 4,  /* TMPOL                 */
    FCODE_DCM       = 5,  /* chan FT_SECn tu DCM   */
    FCODE_BUS       = 6   /* mat lien lac I2C      */
} fault_code_t;

/* ==========================================================================
 * API
 * ========================================================================== */

/** @brief Goi MOT LAN trong main(), NGAY SAU MX_GPIO_Init() va truoc
 *         ina228_init_all(). Dat PWR_EN ve muc TAT. */
void Safety_Init(void);

/** @brief Goi tu HAL_GPIO_EXTI_Callback khi chan ALERT cua INA228 kich.
 *         Cat nguon ngay lap tuc. KHONG cham I2C, KHONG blocking. */
void Safety_FastTrip(void);

/** @brief Goi moi vong main loop, SAU ina228_alert_process(). */
void Safety_Task(uint32_t now_ms);

/** @brief Xu ly lenh 0x88 PWR_CTRL tu PC.
 *  @retval false neu dang TRIPPED/LOCKOUT va PC yeu cau BAT -> tu choi. */
bool Safety_RequestPower(bool on);

/** @brief Xu ly lenh 0x8B CLR_FAULT tu PC. Xoa latch, ve SAFE_OFF.
 *         KHONG tu dong bat lai nguon: PC phai gui them PWR_CTRL(on). */
void Safety_ClearFault(void);

/* --- Truy van trang thai (dung cho khung STATUS 0x03 va LED) --- */
safe_state_t Safety_State(void);
fault_code_t Safety_Code(void);
uint8_t      Safety_TripMask(void);   /* bit i = kenh i gop phan gay trip */
bool         Safety_PowerIsOn(void);  /* trang thai LOGIC cua PWR_EN      */
uint8_t      Safety_TripCount(void);  /* so lan trip lien tiep (khoa LOCKOUT) */

#endif /* SAFETY_H_ */
