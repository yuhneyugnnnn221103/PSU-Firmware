#ifndef PSU_MONITOR_H_
#define PSU_MONITOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "psu_frame.h"
#include "psu_topology.h"

/* Ban tin giam sat 81 81 (310 byte). Phan cung hien tai: INA1..3 giam sat DCM1..3 cua CUM 1:
 *   I_OUT_ADCn  (3 byte) <- thanh ghi CURRENT tho cua INAn (du lieu o bit 23..4, co dau)
 *   TEMP_ADCn   (3 byte) <- thanh ghi DIETEMP tho 16 bit cua INAn, dat o 2 byte thap (byte cao = 00), co dau
 * Phia giao dien tu dich phai / xu ly dau. VBUS khong hien thi. INA4 bo qua hoan toan.
 * Cum DCM 2..4, cum Supply, RTC: 00 (khong co cam bien). Kenh khong "tuoi" gui 00.
 *
 * Vung du phong 68 byte (byte 238..305, 0-based) la MO RONG CUA FIRMWARE:
 *   +0..+2   cum 1: safe_state (SAFE_BOOT 0, OFF 1, ARMING 2, ON 3, TRIPPED 4), PWR_EN (1 = bat), so lan trip lien tiep
 *   +3..+5   cum 2 (cung 3 byte)       +6..+8 cum 3       +9..+11 cum 4
 *   +12      slot firmware dang chay ('A' = app / '?')
 *   +13..+16 so version cu do BEGIN khai bao (u32 BE; PC gui 0 thi 0). Version nay la chuoi, xem GET_VERSION
 *   +17      lenh dieu khien/cau hinh cuoi: ma lenh (0x01 dieu khien, 0x04 ghi cau hinh; 0 = chua co)
 *   +18      ket qua (PSU_RES_*)
 *   +19..+20 bo dem lenh da ghi nhan o +17 (u16 BE, tran quay vong)
 *   +21..+38 9 bo dem chan doan 16 bit BE (tran quay vong): rx_frames, rx_badcrc, rx_badaddr, rx_uart_err,
 *            rx_overrun, tx_sent, tx_skipped, tx_error, tx_stuck
 *   +39..+42 thoi gian chay (giay, u32 BE)
 *   +43      phien ban giao thuc (PSU_PROTO_REV)
 *   +44..+67 = 0 (du phong) */

#define PSU_MON_OFF_CLUSTER1  5u       /* byte dau cua cum 1 (0-based); moi DCM chiem 6 byte: I_OUT_ADC, TEMP_ADC */
#define PSU_MON_OFF_RTC       221u
#define PSU_MON_OFF_TRIP      228u
#define PSU_MON_OFF_SPARE     238u

/* Ket qua lenh cuoi (dieu khien 01 01 / ghi cau hinh 04 04 khong co ACK) */
#define PSU_RES_OK            0u
#define PSU_RES_NOT_OFF       1u   /* ghi cau hinh bi tu choi: con cum chua OFF */
#define PSU_RES_REFUSED       2u   /* lenh bat bi tu choi: cum dang TRIPPED (hoac vua clear trong cung khung) */
#define PSU_RES_FLASH_FAIL    3u   /* ghi cau hinh vao Flash that bai */

#define PSU_PROTO_REV         1u
#define PSU_DIAG_COUNT        9u

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
    uint8_t  last_cmd, last_result;            /* lenh dieu khien/cau hinh cuoi va ket qua */
    uint16_t last_count;
    uint16_t diag[PSU_DIAG_COUNT];
    uint32_t uptime_s;
} psu_mon_t;

/** Dung khung 310 byte vao f (da co CRC). */
void PsuMon_Build(uint8_t *f, const psu_mon_t *m);

#endif /* PSU_MONITOR_H_ */
