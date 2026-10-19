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
 * Vung du phong 24 byte (byte 238..261, 0-based) la MO RONG CUA FIRMWARE, chi giu cac muc sau:
 *   +0  safe_state (SAFE_BOOT 0, OFF 1, ARMING 2, ON 3, TRIPPED 4, LOCKOUT 5)
 *   +1  trang thai PWR_EN (1 = dang bat, 0 = tat; trang thai LOGIC cua chan)
 *   +2  so lan trip lien tiep
 *   +3  slot firmware dang chay ('A' / 'B' / '?')
 *   +4..+7  version firmware dang chay (u32 BE)
 *   +8..+23 = 0 */

#define PSU_MON_OFF_SUPPLY   197u
#define PSU_MON_OFF_RTC      221u
#define PSU_MON_OFF_TRIP     228u
#define PSU_MON_OFF_SPARE    238u

typedef struct {
    uint32_t cur24[4];
    uint32_t vbus24[4];
    uint8_t  fresh_mask;       /* kenh INA nao co du lieu moi (kenh khong tuoi gui 00 00 00) */
    uint8_t  safe_state;
    uint8_t  trip_count;
    bool     pwr_on;
    char     slot;
    uint32_t version;
    const uint8_t *trip;       /* PSU_TRIP_BYTES byte */
} psu_mon_t;

/** Dung khung 266 byte vao f (da co CRC). */
void PsuMon_Build(uint8_t *f, const psu_mon_t *m);

#endif /* PSU_MONITOR_H_ */
