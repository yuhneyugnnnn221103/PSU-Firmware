#include <string.h>
#include <ina228_cfg.h>
#include "psu_cfg.h"
#include "psu_trip.h"
#include "cfg_store.h"

static uint8_t s_blob[PSU_CFG_LEN];
static bool    s_stored;
static bool    s_dirty;

static void default_ina(uint16_t r[PSU_INA_REGS])
{
    r[INAR_CONFIG]       = INA228_CONFIG_VALUE;
    r[INAR_ADC_CONFIG]   = INA228_ADC_CONFIG_VALUE;
    r[INAR_SHUNT_CAL]    = INA228_SHUNT_CAL_VALUE;
    r[INAR_SHUNT_TEMPCO] = INA228_SHUNT_TEMPCO_VALUE;
    r[INAR_SOVL]         = INA228_SOVL_DISABLED;
    r[INAR_SUVL]         = INA228_SUVL_DISABLED;
    r[INAR_BOVL]         = INA228_BOVL_DISABLED;
    r[INAR_BUVL]         = INA228_BUVL_DISABLED;
    r[INAR_TEMP_LIMIT]   = 0x7FFFu;
}

static void put16(uint8_t *d, uint16_t v) { d[0] = (uint8_t)(v >> 8); d[1] = (uint8_t)v; }
static uint16_t get16(const uint8_t *b)   { return (uint16_t)(((uint16_t)b[0] << 8) | b[1]); }

void PsuCfg_BuildDefault(uint8_t *blob)
{
    memset(blob, 0, PSU_CFG_LEN);
    uint16_t r[PSU_INA_REGS];
    default_ina(r);
    for (uint8_t c = 0; c < 4u; c++) {
        for (uint8_t i = 0; i < PSU_INA_REGS; i++) {
            put16(&blob[PSU_CFG_OFF_INA + (c * PSU_INA_REGS + i) * 2u], r[i]);
        }
    }
}

static void apply_thresholds(void)
{
    PsuTrip_SetThresholds(&s_blob[PSU_CFG_OFF_SUPPLY]);
}

void PsuCfg_Load(void)
{
    uint16_t n = 0;
    uint8_t tmp[CFG_BLOB_MAX];

    if (CfgStore_ReadBlob(CFG_BLOB_PSU, tmp, sizeof(tmp), &n) && n == PSU_CFG_LEN) {
        memcpy(s_blob, tmp, PSU_CFG_LEN);
        s_stored = true;
    } else {
        PsuCfg_BuildDefault(s_blob);
        s_stored = false;
    }
    s_dirty = false;
    apply_thresholds();
}

const uint8_t *PsuCfg_Blob(void)  { return s_blob; }
bool PsuCfg_IsStored(void)        { return s_stored; }

bool PsuCfg_Store(const uint8_t *blob)
{
    if (!CfgStore_WriteBlob(CFG_BLOB_PSU, blob, PSU_CFG_LEN)) return false;

    memcpy(s_blob, blob, PSU_CFG_LEN);
    s_stored = true;
    s_dirty = true;
    apply_thresholds();
    return true;
}

bool PsuCfg_ConsumeDirty(void)
{
    const bool d = s_dirty;
    s_dirty = false;
    return d;
}

void PsuCfg_InaRegs(uint8_t ch, uint16_t out[PSU_INA_REGS])
{
    if (ch >= 4u) { default_ina(out); return; }

    const uint8_t *p = &s_blob[PSU_CFG_OFF_INA + ch * PSU_INA_REGS * 2u];
    for (uint8_t i = 0; i < PSU_INA_REGS; i++) out[i] = get16(&p[i * 2u]);

    if (out[INAR_SHUNT_CAL] == 0u) {            /* chua cau hinh INA nay -> mac dinh */
        default_ina(out);
        return;
    }
    out[INAR_CONFIG] &= (uint16_t)~(INA228_CONFIG_RESET | INA228_CONFIG_RSTACC);
}
