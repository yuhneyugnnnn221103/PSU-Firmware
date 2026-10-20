#ifndef PSU_MONITOR_H_
#define PSU_MONITOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "psu_frame.h"
#include "psu_topology.h"

/* Ban tin giam sat 81 81 (266 byte). Phan cung hien tai: INA1..3 giam sat DCM1..3 cua CUM 1:
 *   I_OUT_ADCn  (3 byte) <- thanh ghi CURRENT tho cua INAn (du lieu o bit 23..4, co dau)
 *   TEMP_ADCn   (3 byte) <- thanh ghi DIETEMP tho 16 bit cua INAn, dat o 2 byte thap (byte cao = 00), co dau
 * Phia giao dien tu dich phai / xu ly dau. VBUS khong hien thi. INA4 bo qua hoan toan.
 * Cum DCM 2..4, cum Supply, RTC: 00 (khong co cam bien). Kenh khong "tuoi" gui 00.
 *
 * Vung du phong 24 byte (byte 238..261, 0-based) la MO RONG CUA FIRMWARE:
 *   +0..+2   cum 1: safe_state (SAFE_BOOT 0, OFF 1, ARMING 2, ON 3, TRIPPED 4), PWR_EN (1 = bat), so lan trip lien tiep
 *   +3..+5   cum 2 (cung 3 byte)       +6..+8 cum 3       +9..+11 cum 4
 *   +12      slot firmware dang chay ('A' / 'B' / '?')
 *   +13..+16 version firmware dang chay (u32 BE)
 *   +17..+23 = 0 */

#define PSU_MON_OFF_CLUSTER1  5u       /* byte dau cua cum 1 (0-based); moi DCM chiem 6 byte: I_OUT_ADC, TEMP_ADC */
#define PSU_MON_OFF_RTC       221u
#define PSU_MON_OFF_TRIP      228u
#define PSU_MON_OFF_SPARE     238u

typedef struct {
    uint32_t cur24[PSU_DCM_PER_CLUSTER];
    int16_t  temp[PSU_DCM_PER_CLUSTER];
    uint8_t  fresh_mask;                       /* bit n = DCM n cua cum 1 co du lieu moi */
    uint8_t  safe_state[PSU_CLUSTERS];
    bool     pwr_on[PSU_CLUSTERS];
    uint8_t  trip_count[PSU_CLUSTERS];
    char     slot;
    uint32_t version;
    const uint8_t *trip;                       /* PSU_TRIP_BYTES byte */
} psu_mon_t;

/** Dung khung 266 byte vao f (da co CRC). */
void PsuMon_Build(uint8_t *f, const psu_mon_t *m);

#endif /* PSU_MONITOR_H_ */
