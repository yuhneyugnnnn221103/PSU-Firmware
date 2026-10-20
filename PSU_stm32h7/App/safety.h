/**
 ******************************************************************************
 * @file    safety.h
 * @brief   Lop bao ve cuc bo: phat hien loi tu INA228 / DCM -> cat PWR_EN.
 *
 * NGUYEN TAC:
 *   1. MOI CUM DCM (3 DCM) co mot chan EN va mot may trang thai rieng (4 cum).
 *      Loi o DCM nao cung cat nguon CA CUM do. Mach bring-up chi co EN cua cum 1
 *      (= PWR_EN_CTRL, dieu khien 4 DCM); cum 2..4 chua co phan cung: luon OFF.
 *   2. Duong nhanh (EXTI) cat nguon truoc, phan loai loi sau. Do tre ~2 us
 *      thay vi vai chuc ms neu doi ina228_alert_process().
 *   3. LATCH cung. Chi PC gui ClearTrip roi lenh bat moi bat lai duoc. Khong tu dong
 *      thu lai va KHONG co trang thai khoa: trip nghiem trong chi cat nguon cum do.
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
#include "psu_topology.h"

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

/* Chay on dinh du lau thi dem trip lien tiep ve 0 (chi de bao cao; khong con khoa). */
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
    SAFE_TRIPPED = 4   /* da cat nguon do loi, cho PC xoa (ClearTrip) */
} safe_state_t;

/* Ma loi = so nhay den DO. Gia tri NHO hon = uu tien cao hon (nguyen nhan goc). */
typedef enum {
    FCODE_NONE      = 0,
    FCODE_OVERCUR   = 1,  /* SHNTOL / SHNTUL / POL */
    FCODE_OVERVOLT  = 2,  /* BUSOL                 */
    FCODE_UNDERVOLT = 3,  /* BUSUL                 */
    FCODE_OVERTEMP  = 4,  /* TMPOL                 */
    FCODE_DCM       = 5,  /* chan FT tu DCM        */
    FCODE_BUS       = 6,  /* mat lien lac I2C      */
    FCODE_THRESHOLD = 7   /* STM tu so sanh nguong (psu_trip) vuot MAX/MIN */
} fault_code_t;

/* ==========================================================================
 * API  (c = chi so cum, 0..PSU_CLUSTERS-1)
 * ========================================================================== */

/** @brief Goi MOT LAN, SAU MX_GPIO_Init() va truoc ina228_init_all(). Dat moi chan EN ve TAT. */
void Safety_Init(void);

/** @brief Goi tu HAL_GPIO_EXTI_Callback khi chan ALERT cua INA `ina_idx` kich. Cat nguon cum chua INA
 *         do ngay lap tuc. KHONG cham I2C, KHONG blocking. */
void Safety_FastTrip(uint8_t ina_idx);

/** @brief Goi moi vong main loop, SAU ina228_alert_process(). */
void Safety_Task(uint32_t now_ms);

/** @brief Yeu cau bat/tat cum c. @retval false neu cum khong co phan cung, dang TRIPPED hoac dang nap code. */
bool Safety_RequestPower(uint8_t c, bool on);

/** @brief Clear trip (TOAN PSU, 1 byte ClearTrip cua khung 01 01): xoa trip code + co tich luy cua INA; cum nao
 *         dang TRIPPED thi ve SAFE_OFF va huy yeu cau bat (KHONG tu bat lai). Cum khac KHONG doi bat/tat.
 *         KHONG reset bo dem trip (chi reset sau SAFE_STABLE_MS o SAFE_ON). */
void Safety_ClearFault(void);

/** @brief Lenh dieu khien 01 01. Bit c cua mask = bat/tat cum c (cum chua co phan cung: bo qua). Moi lenh khong
 *         clear ghi de trang thai bat/tat. clear = ClearTrip: cum vua duoc xoa khoi TRIPPED bo qua lenh bat trong
 *         CUNG khung (PC gui lenh bat o khung sau). */
void Safety_Control(uint8_t mask, bool clear);

/** @brief Cat moi chan EN, KHONG phu thuoc trang thai/HAL. Dung trong fault handler. */
void Safety_ForceOff(void);

/** @brief true khi cum c dang SAFE_ON: duoc phep nap nguong duoi cua INA va so sanh nguong. */
bool Safety_IsArmed(uint8_t c);

/** @brief Cum chua INA `ina_idx` (0xFF neu khong thuoc cum nao). */
uint8_t Safety_ClusterOfIna(uint8_t ina_idx);

/** @brief Moi cum deu SAFE_OFF (dieu kien de ghi cau hinh / nap code). */
bool Safety_AllOff(void);

/** @brief Co chan EN nao dang bat (trang thai LOGIC)? */
bool Safety_AnyPowerOn(void);

/* --- Truy van trang thai tung cum --- */
safe_state_t Safety_State(uint8_t c);
fault_code_t Safety_Code(uint8_t c);
bool         Safety_PowerIsOn(uint8_t c);  /* trang thai LOGIC cua chan EN cum c */
uint8_t      Safety_TripCount(uint8_t c);  /* so lan trip lien tiep (chi bao cao) */

#endif /* SAFETY_H_ */
