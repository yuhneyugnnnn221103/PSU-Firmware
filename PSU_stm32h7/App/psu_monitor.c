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
    Psu_FrameBegin(f, PSU_LEN_MONITOR, PSU_CMD_MONITOR);     /* cum 2..4, Supply, RTC, du phong = 0 */

    /* Cum 1: DCM n -> I_OUT_ADC(n+1) o byte 5 + 6n, TEMP_ADC(n+1) o byte 8 + 6n; kenh khong tuoi gui 0 */
    for (uint8_t n = 0; n < PSU_DCM_PER_CLUSTER; n++) {
        if ((m->fresh_mask & (1u << n)) == 0u) continue;
        put24(&f[PSU_MON_OFF_CLUSTER1 + 6u * n],      m->cur24[n]);
        put24(&f[PSU_MON_OFF_CLUSTER1 + 6u * n + 3u], (uint32_t)(uint16_t)m->temp[n]);
    }

    if (m->trip != NULL) memcpy(&f[PSU_MON_OFF_TRIP], m->trip, PSU_TRIP_BYTES);

    uint8_t *x = &f[PSU_MON_OFF_SPARE];
    for (uint8_t c = 0; c < PSU_CLUSTERS; c++) {
        x[3u * c]      = m->safe_state[c];
        x[3u * c + 1u] = m->pwr_on[c] ? 0x01u : 0x00u;
        x[3u * c + 2u] = m->trip_count[c];
    }
    x[12] = (uint8_t)m->slot;
    x[13] = (uint8_t)(m->version >> 24); x[14] = (uint8_t)(m->version >> 16);
    x[15] = (uint8_t)(m->version >> 8);  x[16] = (uint8_t)m->version;

    Psu_FrameSeal(f, PSU_LEN_MONITOR);
}
