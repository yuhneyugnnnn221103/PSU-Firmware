#include <string.h>
#include "psu_trip.h"

static struct {
    int32_t  max[PSU_SUP_COUNT], min[PSU_SUP_COUNT];
    bool     has_max[PSU_SUP_COUNT], has_min[PSU_SUP_COUNT];

    uint8_t  viol_max[PSU_SUP_COUNT], viol_min[PSU_SUP_COUNT];
    uint32_t last_stamp[4];
    bool     have_stamp[4];

    uint8_t  trip[PSU_TRIP_BYTES];
} s;

static int32_t sext24(uint32_t v)
{
    v &= 0x00FFFFFFu;
    return (int32_t)((v & 0x00800000u) ? (v | 0xFF000000u) : v);
}

static bool is_current(uint8_t k) { return (k & 1u) == 0u; }

/* Gia tri ngưỡng 0 (ca 3 byte) = tat. Dong: co dau; ap: khong dau (< 2^23 nen cung la so duong) */
static int32_t thr_value(uint8_t k, uint32_t raw24)
{
    return is_current(k) ? sext24(raw24) : (int32_t)(raw24 & 0x00FFFFFFu);
}

void PsuTrip_Init(void)
{
    memset(&s, 0, sizeof(s));
}

void PsuTrip_SetThresholds(const uint8_t thr48[PSU_SUP_THR_LEN])
{
    for (uint8_t k = 0; k < PSU_SUP_COUNT; k++) {
        const uint8_t *p = &thr48[k * 6u];
        const uint32_t mx = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
        const uint32_t mn = ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 8) | p[5];

        s.has_max[k] = (mx != 0u);
        s.has_min[k] = (mn != 0u);
        s.max[k] = thr_value(k, mx);
        s.min[k] = thr_value(k, mn);
        s.viol_max[k] = 0u;
        s.viol_min[k] = 0u;
    }
}

bool PsuTrip_Set(uint8_t byte_idx, uint8_t mask)
{
    if (byte_idx >= PSU_TRIP_BYTES) return false;
    const bool fresh = (s.trip[byte_idx] & mask) != mask;
    s.trip[byte_idx] |= mask;
    return fresh;
}

bool PsuTrip_Evaluate(bool armed, const psu_trip_in_t *in)
{
    bool latched = false;

    for (uint8_t c = 0; c < 4u; c++) {
        const bool new_sample = in->fresh[c] && (!s.have_stamp[c] || s.last_stamp[c] != in->stamp[c]);

        if (!armed || !in->fresh[c]) {
            /* khong so sanh: V ~ 0 luc OFF/ARMING luon < MIN; mat du lieu thi het chuoi lien tiep */
            s.viol_max[2u * c] = s.viol_max[2u * c + 1u] = 0u;
            s.viol_min[2u * c] = s.viol_min[2u * c + 1u] = 0u;
            if (in->fresh[c]) { s.last_stamp[c] = in->stamp[c]; s.have_stamp[c] = true; }
            continue;
        }
        if (!new_sample) continue;
        s.last_stamp[c] = in->stamp[c];
        s.have_stamp[c] = true;

        for (uint8_t j = 0; j < 2u; j++) {
            const uint8_t k = (uint8_t)(2u * c + j);
            const int32_t v = thr_value(k, j == 0u ? in->cur24[c] : in->vbus24[c]);
            const uint8_t bit = (uint8_t)(1u << k);

            if (s.has_max[k] && v > s.max[k]) {
                if (s.viol_max[k] < 255u) s.viol_max[k]++;
                if (s.viol_max[k] >= PSU_TRIP_CONFIRM_N && PsuTrip_Set(PSU_TRIP_B_SUP_MAX, bit)) latched = true;
            } else {
                s.viol_max[k] = 0u;
            }

            if (s.has_min[k] && v < s.min[k]) {
                if (s.viol_min[k] < 255u) s.viol_min[k]++;
                if (s.viol_min[k] >= PSU_TRIP_CONFIRM_N && PsuTrip_Set(PSU_TRIP_B_SUP_MIN, bit)) latched = true;
            } else {
                s.viol_min[k] = 0u;
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
