/*
 * buffer_test.c
 *
 *  Created on: Aug 13, 2026
 *      Author: HuyND304
 */
#include "buffer_test.h"

/* --------------------------------------------------------------- helpers -- */

static void put_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v);
}

static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v);
}

static void put_be64(uint8_t *p, uint64_t v)
{
    p[0] = (uint8_t)(v >> 56);
    p[1] = (uint8_t)(v >> 48);
    p[2] = (uint8_t)(v >> 40);
    p[3] = (uint8_t)(v >> 32);
    p[4] = (uint8_t)(v >> 24);
    p[5] = (uint8_t)(v >> 16);
    p[6] = (uint8_t)(v >> 8);
    p[7] = (uint8_t)(v);
}

uint16_t pmon_crc16_ccitt(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFFu;

    for (uint32_t i = 0; i < len; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;

        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
    }

    return crc;
}

/* Sóng tam giác biên độ +/- amp, chu kỳ `period` tick */
static int32_t triangle(uint32_t tick, uint32_t period, int32_t amp)
{
    const uint32_t half = period / 2u;
    const uint32_t ph   = tick % period;

    if (ph < half)
        return (int32_t)((int64_t)amp * (2 * (int32_t)ph - (int32_t)half) / (int32_t)half);

    return (int32_t)((int64_t)amp * (3 * (int32_t)half - 2 * (int32_t)ph) / (int32_t)half);
}

/* ------------------------------------------------------------- state ------ */

static uint32_t s_tick;
static int64_t  s_energy_counts[PMON_IC_COUNT];
static int64_t  s_charge_counts[PMON_IC_COUNT];

void pmon_test_init(void)
{
    s_tick = 0;

    for (int i = 0; i < PMON_IC_COUNT; ++i)
    {
        s_energy_counts[i] = 0;
        s_charge_counts[i] = 0;
    }
}

/* -------------------------------------------------------- frame builder --- */

void pmon_test_build_frame(uint8_t *buf)
{
    /* ---- Header / CMD ---- */
    buf[0] = PMON_HEADER1;
    buf[1] = PMON_HEADER2;
    buf[2] = PMON_CMD_MONITOR;

    /* ---- Status: xoay vòng để kiểm tra badge FRESH / DCM FAULT ----
     * fresh : kênh i mất fresh trong 5 s, cứ 40 s một lượt
     * fault : kênh i báo lỗi DCM trong 3 s, cứ 36 s một lượt          */
    uint8_t status = 0x0Fu;

    const uint32_t freshSlot = (s_tick / 50u) % 8u;    /* 50 tick = 5 s */
    if (freshSlot < PMON_IC_COUNT)
        status &= (uint8_t)~(1u << freshSlot);

    const uint32_t faultSlot = (s_tick / 30u) % 12u;   /* 30 tick = 3 s */
    if (faultSlot < PMON_IC_COUNT)
        status |= (uint8_t)(1u << (faultSlot + 4));

    buf[3] = status;

    /* ---- 4 block dữ liệu ---- */
    for (int i = 0; i < PMON_IC_COUNT; ++i)
    {
        uint8_t *p = &buf[4 + i * PMON_IC_SIZE];

        /* --- Giá trị vật lý giả lập --- */

        /* Vbus: 12 / 18 / 24 / 30 V, gợn +/- 250 mV */
        int32_t vbus_mV = 12000 + i * 6000
                        + triangle(s_tick + (uint32_t)i * 17u, 80u, 250);

        /* Dòng: 0.5 / 2.0 / 8.0 A, kênh #4 âm để test sign extension */
        static const int32_t base_mA[PMON_IC_COUNT] = { 500, 2000, 8000, -1500 };

        int32_t current_mA = base_mA[i]
                           + triangle(s_tick + (uint32_t)i * 23u, 120u,
                                      base_mA[i] / 5);

        /* Nhiệt độ: 35 / 37.5 / 40 / 42.5 °C, gợn +/- 1.5 °C (đơn vị 0.01 °C) */
        int32_t temp_cC = 3500 + i * 250
                        + triangle(s_tick + (uint32_t)i * 31u, 200u, 150);

        /* --- Quy đổi sang counts của INA228 --- */

        /* counts = A * 12800 = mA * 12.8 */
        int32_t current_counts = (int32_t)(((int64_t)current_mA * 128) / 10);

        /* Rshunt = 1 mOhm nên Vshunt counts trùng CURRENT counts */
        int32_t vshunt_counts = current_counts;

        /* counts = V * 5120 = mV * 5.12 */
        uint32_t vbus_counts = (uint32_t)(((int64_t)vbus_mV * 512) / 100);

        /* counts = °C * 128 = cC * 1.28 */
        int16_t temp_counts = (int16_t)(((int32_t)temp_cC * 128) / 100);

        /* POWER là trị tuyệt đối theo datasheet.
         * counts = W * 4000 = (mV * mA / 1e6) * 4000 = mV * mA / 250 */
        int32_t abs_mA = (current_mA < 0) ? -current_mA : current_mA;
        uint32_t power_counts =
            (uint32_t)(((int64_t)vbus_mV * abs_mA) / 250);

        /* --- Tích phân ENERGY / CHARGE ---
         * dt = PMON_TEST_PERIOD_MS ms
         * dCharge_counts = current_counts * dt / 1000
         * dEnergy_counts = power_counts * POWER_LSB * dt / ENERGY_LSB
         *                = power_counts * dt / 16000                    */
        s_charge_counts[i] += ((int64_t)current_counts * PMON_TEST_PERIOD_MS) / 1000;
        s_energy_counts[i] += ((int64_t)power_counts  * PMON_TEST_PERIOD_MS) / 16000;

        /* ENERGY là 40-bit unsigned, CHARGE là 40-bit signed.
         * Giới hạn lại cho giống phần cứng thật (INA228 tràn thì dừng). */
        if (s_energy_counts[i] > 0xFFFFFFFFFFLL)
            s_energy_counts[i] = 0xFFFFFFFFFFLL;

        if (s_charge_counts[i] > 0x7FFFFFFFFFLL)
            s_charge_counts[i] = 0x7FFFFFFFFFLL;
        if (s_charge_counts[i] < -0x8000000000LL)
            s_charge_counts[i] = -0x8000000000LL;

        /* --- Đóng gói big-endian, đã sign-extend sẵn --- */
        put_be32(p +  0, (uint32_t)current_counts);          /* int32  */
        put_be32(p +  4, vbus_counts);                       /* uint32 */
        put_be16(p +  8, (uint16_t)temp_counts);             /* int16  */
        put_be32(p + 10, power_counts);                      /* uint32 */
        put_be32(p + 14, (uint32_t)vshunt_counts);           /* int32  */
        put_be64(p + 18, (uint64_t)s_energy_counts[i]);      /* uint64 */
        put_be64(p + 26, (uint64_t)s_charge_counts[i]);      /* int64  */
    }

    /* ---- CRC ---- */
#if PMON_TEST_USE_REAL_CRC
    const uint16_t crc = pmon_crc16_ccitt(&buf[PMON_CRC_START], PMON_CRC_LENGTH);
#else
    const uint16_t crc = 0;
#endif
    put_be16(&buf[PMON_CRC_OFFSET], crc);

    /* ---- Tailer ---- */
    buf[142] = PMON_TAILER1;
    buf[143] = PMON_TAILER2;

    ++s_tick;
}

