#include <string.h>
#include "psu_trip.h"

static struct {
    int32_t  max[PSU_CLUSTERS][PSU_TRIP_VALUES], min[PSU_CLUSTERS][PSU_TRIP_VALUES];
    bool     has_max[PSU_CLUSTERS][PSU_TRIP_VALUES], has_min[PSU_CLUSTERS][PSU_TRIP_VALUES];

    uint8_t  viol_max[PSU_CLUSTERS][PSU_TRIP_VALUES], viol_min[PSU_CLUSTERS][PSU_TRIP_VALUES];
    uint32_t last_stamp[PSU_CLUSTERS][PSU_DCM_PER_CLUSTER];
    bool     have_stamp[PSU_CLUSTERS][PSU_DCM_PER_CLUSTER];

    uint8_t  trip[PSU_TRIP_BYTES];
} s;

static int32_t sext24(uint32_t v)
{
    v &= 0x00FFFFFFu;
    return (int32_t)((v & 0x00800000u) ? (v | 0xFF000000u) : v);
}

static bool is_current(uint8_t k) { return (k & 1u) == 0u; }

/* k chan: I_OUT_ADC(k/2+1), le: TEMP_ADC(k/2+1) */
static int32_t thr_value(uint8_t k, uint32_t raw24)
{
    return is_current(k) ? sext24(raw24) : (int32_t)(int16_t)(raw24 & 0xFFFFu);
}

/* Cum nao da co bang bit trip (tam thoi chi cum 1) */
static bool sw_assigned(uint8_t c) { return c == 0u; }

void PsuTrip_Init(void)
{
    memset(&s, 0, sizeof(s));
}

void PsuTrip_SetThresholds(const uint8_t *cfg)
{
    for (uint8_t c = 0; c < PSU_CLUSTERS; c++) {
        const uint8_t *blk = &cfg[c * PSU_TRIP_CLUSTER_STRIDE];

        for (uint8_t k = 0; k < PSU_TRIP_VALUES; k++) {
            const uint8_t *p = &blk[k * 6u];
            const uint32_t mx = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
            const uint32_t mn = ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 8) | p[5];

            s.has_max[c][k] = (mx != 0u);
            s.has_min[c][k] = (mn != 0u);
            s.max[c][k] = thr_value(k, mx);
            s.min[c][k] = thr_value(k, mn);
            s.viol_max[c][k] = 0u;
            s.viol_min[c][k] = 0u;
        }
    }
}

bool PsuTrip_Set(uint8_t byte_idx, uint8_t mask)
{
    if (byte_idx >= PSU_TRIP_BYTES) return false;
    const bool fresh = (s.trip[byte_idx] & mask) != mask;
    s.trip[byte_idx] |= mask;
    return fresh;
}

bool PsuTrip_Evaluate(uint8_t c, bool armed, const psu_trip_in_t *in)
{
    bool latched = false;
    if (c >= PSU_CLUSTERS || !sw_assigned(c)) return false;

    for (uint8_t n = 0; n < PSU_DCM_PER_CLUSTER; n++) {
        const bool new_sample = in->fresh[n] && (!s.have_stamp[c][n] || s.last_stamp[c][n] != in->stamp[n]);
        const uint8_t kc = (uint8_t)(2u * n), kt = (uint8_t)(2u * n + 1u);

        if (!armed || !in->fresh[n]) {
            /* khong so sanh: luc OFF/ARMING gia tri ~ 0 luon < MIN; mat du lieu thi het chuoi lien tiep */
            s.viol_max[c][kc] = s.viol_max[c][kt] = 0u;
            s.viol_min[c][kc] = s.viol_min[c][kt] = 0u;
            if (in->fresh[n]) { s.last_stamp[c][n] = in->stamp[n]; s.have_stamp[c][n] = true; }
            continue;
        }
        if (!new_sample) continue;
        s.last_stamp[c][n] = in->stamp[n];
        s.have_stamp[c][n] = true;

        for (uint8_t j = 0; j < 2u; j++) {
            const uint8_t k = (uint8_t)(2u * n + j);
            const int32_t v = (j == 0u) ? sext24(in->cur24[n]) : (int32_t)in->temp[n];
            const uint8_t bit = (uint8_t)(1u << (j == 0u ? n : (PSU_DCM_PER_CLUSTER + n)));

            if (s.has_max[c][k] && v > s.max[c][k]) {
                if (s.viol_max[c][k] < 255u) s.viol_max[c][k]++;
                if (s.viol_max[c][k] >= PSU_TRIP_CONFIRM_N && PsuTrip_Set(PSU_TRIP_B_SW_MAX, bit)) latched = true;
            } else {
                s.viol_max[c][k] = 0u;
            }

            if (s.has_min[c][k] && v < s.min[c][k]) {
                if (s.viol_min[c][k] < 255u) s.viol_min[c][k]++;
                if (s.viol_min[c][k] >= PSU_TRIP_CONFIRM_N && PsuTrip_Set(PSU_TRIP_B_SW_MIN, bit)) latched = true;
            } else {
                s.viol_min[c][k] = 0u;
            }
        }
    }
    return latched;
}

void PsuTrip_Clear(void)
{
    memset(s.trip, 0, sizeof(s.trip));
    memset(s.viol_max, 0, sizeof(s.viol_max));
    memset(s.viol_min, 0, sizeof(s.viol_min));
}

bool PsuTrip_Any(void)
{
    for (uint8_t i = 0; i < PSU_TRIP_BYTES; i++) if (s.trip[i] != 0u) return true;
    return false;
}

const uint8_t *PsuTrip_Bytes(void)
{
    return s.trip;
}
