#ifndef PSU_TRIP_H_
#define PSU_TRIP_H_

#include <stdint.h>
#include <stdbool.h>

/* ==========================================================================
 * TRIP CODE (10 byte trong ban tin giam sat) + SO SANH NGUONG TREN STM32
 *
 * *** BANG GAN BIT DUOI DAY LA TAM THOI - cho bang chinh thuc cua nguoi dung. ***
 * Chi sua cac #define o day (va PsuTrip_Set noi goi) khi doi bang.
 *
 * Trip LATCH: giu nguyen cho den khi PsuTrip_Clear() (lenh Clear trip).
 * ========================================================================== */

#define PSU_TRIP_BYTES      10u

/* TRIP1 / TRIP2: bit k = gia tri Supply thu k vuot MAX / duoi MIN.
 * k: 0 I_DCM, 1 V_DCM, 2 I_5V, 3 V_5V, 4 I_3V3_1, 5 V_3V3_1, 6 I_3V3_2, 7 V_3V3_2
 * (kenh INA c: k = 2c la dong, 2c+1 la ap bus) */
#define PSU_TRIP_B_SUP_MAX  0u
#define PSU_TRIP_B_SUP_MIN  1u
/* TRIP3: bit c = INA kenh c bao SHNTOL/POL (qua dong/cong suat); bit 4+c = SHNTUL */
#define PSU_TRIP_B_INA_OC   2u
/* TRIP4: bit c = BUSOL (qua ap bus); bit 4+c = BUSUL (sut ap bus) */
#define PSU_TRIP_B_INA_V    3u
/* TRIP5: bit c = TMPOL (qua nhiet INA kenh c); bit 4+c = chan FT cua DCM c */
#define PSU_TRIP_B_TEMP_FT  4u
/* TRIP6: bit0 mat lien lac I2C (khi SAFE_TRIP_ON_BUS_LOSS), bit1 cat nguon do ALERT nhung
 *        khong xac nhan duoc nguyen nhan, bit2 LOCKOUT, bit3 cau hinh INA khong hop le */
#define PSU_TRIP_B_MISC     5u
#define PSU_TRIP_MISC_BUS       0x01u
#define PSU_TRIP_MISC_UNKNOWN   0x02u
#define PSU_TRIP_MISC_LOCKOUT   0x04u
#define PSU_TRIP_MISC_CFG       0x08u
/* TRIP7..TRIP10: du phong, = 0 */

/* So mau LIEN TIEP (moi mau ~60 ms) vuot nguong truoc khi latch trip */
#ifndef PSU_TRIP_CONFIRM_N
#define PSU_TRIP_CONFIRM_N  2u
#endif

#define PSU_SUP_COUNT       8u     /* 4 INA x (dong, ap) */
#define PSU_SUP_THR_LEN     48u    /* 8 x (MAX, MIN) x 3 byte, dung bo cuc trong ban tin cau hinh */

typedef struct {
    uint32_t cur24[4];        /* thanh ghi CURRENT tho 24 bit (du lieu o bit 23..4), co dau */
    uint32_t vbus24[4];       /* thanh ghi VBUS tho 24 bit, khong dau */
    uint32_t stamp[4];        /* doi khi co mau moi (vd meas.timestamp_ms) */
    bool     fresh[4];
} psu_trip_in_t;

void PsuTrip_Init(void);

/** thr48: 8 gia tri theo thu tu I_DCM,V_DCM,I_5V,V_5V,I_3V3_1,V_3V3_1,I_3V3_2,V_3V3_2, moi gia tri
 *  MAX roi MIN (3 byte big-endian). Gia tri 0 = tat nguong do. Dong (so chan) so sanh CO DAU 24 bit,
 *  ap so sanh khong dau. */
void PsuTrip_SetThresholds(const uint8_t thr48[PSU_SUP_THR_LEN]);

/** So sanh khi armed (SAFE_ON); khong armed thi reset bo dem. Moi mau moi duoc xet 1 lan.
 *  @return true neu LAN GOI NAY co bit trip moi duoc latch tu so sanh nguong */
bool PsuTrip_Evaluate(bool armed, const psu_trip_in_t *in);

/** OR bit vao trip byte (nguon khac: ALERT cua INA, DCM FT, ...). @return true neu bit moi. */
bool PsuTrip_Set(uint8_t byte_idx, uint8_t mask);

void PsuTrip_Clear(void);
bool PsuTrip_Any(void);
const uint8_t *PsuTrip_Bytes(void);    /* PSU_TRIP_BYTES byte */

#endif /* PSU_TRIP_H_ */
