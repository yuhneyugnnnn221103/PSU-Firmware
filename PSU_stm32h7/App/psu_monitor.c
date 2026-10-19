#include <stddef.h>
#include <string.h>
#include "psu_monitor.h"
#include "psu_trip.h"

static void put24(uint8_t *d, uint32_t v)
{
    d[0] = (uint8_t)(v >> 16); d[1] = (uint8_t)(v >> 8); d[2] = (uint8_t)v;
}

void PsuMon_Build(uint8_t *f, const psu_mon_t *m)
{
    Psu_FrameBegin(f, PSU_LEN_MONITOR, PSU_CMD_MONITOR);     /* 4 cum DCM, RTC, du phong = 0 */

    /* Supply: kenh c -> (dong, ap) o byte 197 + 6c; kenh khong tuoi gui 0 */
    for (uint8_t c = 0; c < 4u; c++) {
        if ((m->fresh_mask & (1u << c)) == 0u) continue;
        put24(&f[PSU_MON_OFF_SUPPLY + 6u * c],      m->cur24[c]);
        put24(&f[PSU_MON_OFF_SUPPLY + 6u * c + 3u], m->vbus24[c]);
    }

    if (m->trip != NULL) memcpy(&f[PSU_MON_OFF_TRIP], m->trip, PSU_TRIP_BYTES);

    uint8_t *x = &f[PSU_MON_OFF_SPARE];
    x[0] = m->safe_state;
    x[1] = (uint8_t)((m->pwr_on ? 0x01u : 0u) | (m->pc_wants_on ? 0x02u : 0u));
    x[2] = m->trip_count;
    x[3] = m->fault_code;
    x[4] = m->fresh_mask;
    x[5] = m->cfgok_mask;
    x[6] = m->dcm_mask;
    x[7] = m->cfg_stored ? 0x01u : 0x00u;
    for (uint8_t c = 0; c < 4u; c++) {
        x[8u + 2u * c]      = (uint8_t)(m->diag[c] >> 8);
        x[8u + 2u * c + 1u] = (uint8_t)m->diag[c];
    }
    x[16] = (uint8_t)m->slot;
    x[17] = m->trial_state;
    x[18] = (uint8_t)(m->version >> 24); x[19] = (uint8_t)(m->version >> 16);
    x[20] = (uint8_t)(m->version >> 8);  x[21] = (uint8_t)m->version;

    Psu_FrameSeal(f, PSU_LEN_MONITOR);
}
