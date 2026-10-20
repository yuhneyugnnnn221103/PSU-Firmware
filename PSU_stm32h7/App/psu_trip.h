#ifndef PSU_TRIP_H_
#define PSU_TRIP_H_

#include <stdint.h>
#include <stdbool.h>
#include "psu_topology.h"

/* ==========================================================================
 * TRIP CODE (10 byte trong ban tin giam sat) + SO SANH NGUONG TREN STM32
 *
 * Moi cum co 3 DCM; moi DCM co 2 gia tri giam sat: dong (I_OUT_ADCn) va nhiet do (TEMP_ADCn),
 * moi gia tri co ngung MAX/MIN trong khoi cau hinh cua cum (192 B/cum, 36 byte dau):
 *   +0 I_OUT_ADC1 MAX, +3 MIN, +6 TEMP_ADC1 MAX, +9 MIN, +12 I_OUT_ADC2 ..., +30 TEMP_ADC3 MIN
 * Mach bring-up: INA1..3 = DCM1..3 cua CUM 1 (dong = thanh ghi CURRENT 24 bit co dau,
 * nhiet do = thanh ghi DIETEMP 16 bit co dau). Cum 2..4 chua co phan cung / chua co bang bit.
 *
 * *** BANG GAN BIT DUOI DAY LA TAM THOI - cho bang chinh thuc cua nguoi dung. ***
 * Chi sua cac #define o day (va noi goi PsuTrip_Set) khi doi bang. Hien chi cum 1.
 *
 * Trip LATCH: giu nguyen cho den khi PsuTrip_Clear() (lenh ClearTrip).
 * ========================================================================== */

#define PSU_TRIP_BYTES      10u

/* TRIP1 / TRIP2 (so sanh tren STM): bit n (0..2) = I_OUT_ADC(n+1) vuot MAX / duoi MIN;
 *                                   bit 3+n       = TEMP_ADC(n+1) vuot MAX / duoi MIN */
#define PSU_TRIP_B_SW_MAX   0u
#define PSU_TRIP_B_SW_MIN   1u
/* TRIP3: bit n = INA n bao SHNTOL/POL (qua dong/cong suat); bit 3+n = SHNTUL */
#define PSU_TRIP_B_INA_OC   2u
/* TRIP4: bit n = BUSOL (qua ap bus); bit 3+n = BUSUL (sut ap bus) */
#define PSU_TRIP_B_INA_V    3u
/* TRIP5: bit n (0..2) = TMPOL (qua nhiet INA n); bit 3+n (n = 0..3) = chan FT cua DCM n+1 */
#define PSU_TRIP_B_TEMP_FT  4u
/* TRIP6: bit0 mat lien lac I2C (khi SAFE_TRIP_ON_BUS_LOSS), bit1 cat do ALERT nhung khong xac nhan
 *        duoc nguyen nhan, bit3 cau hinh INA khong hop le (bit2 du phong) */
#define PSU_TRIP_B_MISC     5u
#define PSU_TRIP_MISC_BUS       0x01u
#define PSU_TRIP_MISC_UNKNOWN   0x02u
#define PSU_TRIP_MISC_CFG       0x08u
/* TRIP7..TRIP10: du phong, = 0 */

/* So mau LIEN TIEP (moi mau ~60 ms) vuot nguong truoc khi latch trip */
#ifndef PSU_TRIP_CONFIRM_N
#define PSU_TRIP_CONFIRM_N  2u
#endif

#define PSU_TRIP_CLUSTER_STRIDE 192u    /* khoang cach giua cac khoi cum trong anh cau hinh */
#define PSU_TRIP_VALUES         (2u * PSU_DCM_PER_CLUSTER)

typedef struct {
    uint32_t cur24[PSU_DCM_PER_CLUSTER];     /* thanh ghi CURRENT tho 24 bit (du lieu bit 23..4), co dau */
    int16_t  temp[PSU_DCM_PER_CLUSTER];      /* thanh ghi DIETEMP tho 16 bit, co dau */
    uint32_t stamp[PSU_DCM_PER_CLUSTER];     /* doi khi co mau moi (vd meas.timestamp_ms) */
    bool     fresh[PSU_DCM_PER_CLUSTER];
} psu_trip_in_t;

void PsuTrip_Init(void);

/** cfg: anh cau hinh PSU (khoi dau = cum 1). Ngung 3 byte, gia tri 0 = tat ngung do. Dong so sanh CO DAU 24 bit;
 *  nhiet do lay 16 bit thap, so sanh CO DAU 16 bit. */
void PsuTrip_SetThresholds(const uint8_t *cfg);

/** So sanh cho cum c khi armed (SAFE_ON); khong armed thi reset bo dem. Moi mau moi duoc xet 1 lan.
 *  @return true neu LAN GOI NAY co bit trip moi duoc latch tu so sanh nguong (cum chua co bang bit: luon false) */
bool PsuTrip_Evaluate(uint8_t c, bool armed, const psu_trip_in_t *in);

/** OR bit vao trip byte (nguon khac: ALERT cua INA, DCM FT, ...). @return true neu bit moi. */
bool PsuTrip_Set(uint8_t byte_idx, uint8_t mask);

void PsuTrip_Clear(void);
bool PsuTrip_Any(void);
const uint8_t *PsuTrip_Bytes(void);    /* PSU_TRIP_BYTES byte */

#endif /* PSU_TRIP_H_ */
