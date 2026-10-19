#ifndef PSU_MONITOR_H_
#define PSU_MONITOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "psu_frame.h"

/* Ban tin giam sat 81 81 (266 byte). Phan cung hien tai: 4 INA thay cho 4 cap (dong, ap) o cum Supply:
 *   INA1 -> DCM, INA2 -> 5V, INA3 -> 3V3_1, INA4 -> 3V3_2
 * Gia tri INA gui NGUYEN 3 byte thanh ghi CURRENT / VBUS (du lieu o bit 23..4): phia giao dien tu
 * dich phai 4 bit va xu ly dau. Cum DCM, RTC: 00 (khong co cam bien).
 *
 * Vung du phong 24 byte (byte 238..261, 0-based) la MO RONG CUA FIRMWARE:
 *   +0  safe_state (SAFE_*)         +1  bit0 PWR_EN dang bat, bit1 PC dang yeu cau bat
 *   +2  so lan trip lien tiep       +3  fault_code (FCODE_*)
 *   +4  mask kenh INA con tuoi      +5  mask kenh INA cau hinh dung
 *   +6  mask chan FT cua DCM        +7  bit0 = cau hinh PSU da luu o Flash (0 = dang dung mac dinh)
 *   +8..+15  DIAG_ALRT tich luy cua INA1..4 (moi cai 2 byte BE)
 *   +16 slot dang chay ('A'/'B'/'?')  +17 trang thai trial (bit7 confirm, bit6..0 boot_count)
 *   +18..+21 version anh dang chay (u32 BE)   +22..+23 = 0 */

#define PSU_MON_OFF_SUPPLY   197u
#define PSU_MON_OFF_RTC      221u
#define PSU_MON_OFF_TRIP     228u
#define PSU_MON_OFF_SPARE    238u

typedef struct {
    uint32_t cur24[4];
    uint32_t vbus24[4];
    uint16_t diag[4];
    uint8_t  fresh_mask;
    uint8_t  cfgok_mask;
    uint8_t  dcm_mask;
    uint8_t  safe_state;
    uint8_t  fault_code;
    uint8_t  trip_count;
    bool     pwr_on;
    bool     pc_wants_on;
    bool     cfg_stored;
    char     slot;
    uint8_t  trial_state;
    uint32_t version;
    const uint8_t *trip;       /* PSU_TRIP_BYTES byte */
} psu_mon_t;

/** Dung khung 266 byte vao f (da co CRC). */
void PsuMon_Build(uint8_t *f, const psu_mon_t *m);

#endif /* PSU_MONITOR_H_ */
