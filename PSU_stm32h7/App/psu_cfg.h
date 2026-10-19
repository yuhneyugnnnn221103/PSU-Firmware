#ifndef PSU_CFG_H_
#define PSU_CFG_H_

#include <stdint.h>
#include <stdbool.h>

/* Anh cau hinh PSU 904 byte = phan trong khung 04 04 / 82 82 nam giua Mask (2 byte) va CRC:
 *   [0..767]   4 cum DCM x 192 B (nguong ADC/AMC/XDP, cau hinh XDP, ADS) - chi luu va tra lai
 *   [768..839] 4 INA x 9 thanh ghi x 2 B (CONFIG, ADC_CONFIG, SHUNT_CAL, SHUNT_TEMPCO,
 *              SOVL, SUVL, BOVL, BUVL, TEMP_LIMIT)  - nap vao INA
 *   [840..887] nguong Supply 8 x (MAX, MIN) x 3 B     - firmware tu so sanh (psu_trip)
 *   [888..903] du phong 16 B                          - chi luu va tra lai */
#define PSU_CFG_LEN          904u
#define PSU_CFG_OFF_INA      768u
#define PSU_CFG_OFF_SUPPLY   840u
#define PSU_CFG_OFF_SPARE    888u

#define PSU_INA_REGS         9u
enum { INAR_CONFIG = 0, INAR_ADC_CONFIG, INAR_SHUNT_CAL, INAR_SHUNT_TEMPCO,
       INAR_SOVL, INAR_SUVL, INAR_BOVL, INAR_BUVL, INAR_TEMP_LIMIT };

/** Nap tu Flash (hoac mac dinh neu chua co) va ap nguong Supply. Goi sau CfgStore_Init(). */
void PsuCfg_Load(void);

/** Cau hinh dang dung (PSU_CFG_LEN byte): dung de tra loi 82 82. */
const uint8_t *PsuCfg_Blob(void);

/** true neu cau hinh nay da duoc luu o Flash (false = mac dinh). */
bool PsuCfg_IsStored(void);

/** Ghi cau hinh moi (PSU_CFG_LEN byte): luu Flash roi moi dung. false neu ghi Flash loi
 *  (cau hinh cu giu nguyen). Goi PsuCfg_ConsumeDirty() de biet can nap lai INA. */
bool PsuCfg_Store(const uint8_t *blob);

/** true (1 lan) neu cau hinh vua doi -> can nap lai INA. */
bool PsuCfg_ConsumeDirty(void);

/** Thanh ghi INA HIEU LUC cho kenh ch (0..3): neu SHUNT_CAL = 0 (chua cau hinh) thi dung mac dinh
 *  trong ina228_cfg.h. CONFIG bi xoa bit RST/RSTACC. */
void PsuCfg_InaRegs(uint8_t ch, uint16_t out[PSU_INA_REGS]);

/** Cau hinh mac dinh: moi thu = 0 tru thanh ghi INA = mac dinh. */
void PsuCfg_BuildDefault(uint8_t *blob);

#endif /* PSU_CFG_H_ */
