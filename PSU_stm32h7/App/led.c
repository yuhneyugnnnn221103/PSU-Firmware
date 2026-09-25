/**
 ******************************************************************************
 * @file    led.c
 * @brief   3 LED, bitmap 32 khe. Xem led.h de biet phan vai tung den.
 ******************************************************************************
 */

#include <led.h>
#include "main.h"

/* ==========================================================================
 * BANG PATTERN  (bit 0 = khe 0, phat lan luot bit0 -> bit31)
 * Mot "don vi nhay" = 1 khe sang + 1 khe tat = 200 ms.
 * ========================================================================== */

#define PAT_OFF        0x00000000uL
#define PAT_SOLID      0xFFFFFFFFuL

/* Nhay 5 Hz: doi trang thai moi khe */
#define PAT_5HZ        0x55555555uL

/* Heartbeat: mot cum moi 8 khe (800 ms) -> ~1.25 Hz, mat khong phan biet
 * duoc voi 1 Hz. Chon 8 khe vi 32 chia het cho 8 -> pattern lien tuc. */
#define PAT_HB1        0x01010101uL   /* nhay don : khe 0                  */
#define PAT_HB2        0x05050505uL   /* nhay doi : khe 0, 2               */
#define PAT_HB3        0x15151515uL   /* nhay ba  : khe 0, 2, 4            */

/* Canh bao: nhay don nhung LECH PHA voi heartbeat (khe 4 thay vi khe 0)
 * -> nhin thay do va xanh la thay phien nhau, khong chong nhau. */
#define PAT_WARN       0x10101010uL

/* ==========================================================================
 * TRANG THAI
 * ========================================================================== */

static uint32_t s_pat_g, s_pat_b, s_pat_r;
static uint8_t  s_slot;
static uint32_t s_t_last;

/* ==========================================================================
 * TIEN ICH
 * ========================================================================== */

/* n nhay lien tiep (khe 0,2,4,...) roi nghi het chu ky.
 * n = 6 -> 11 khe dung, 21 khe nghi = 2.1 s. Du dai de mat dem duoc. */
static uint32_t pat_count(uint8_t n)
{
    uint32_t p = 0uL;
    if (n > 8u) n = 8u;
    for (uint8_t i = 0u; i < n; i++) {
        p |= (1uL << (2u * i));
    }
    return p;
}

static inline void led_write(GPIO_TypeDef *port, uint16_t pin, bool on)
{
#if LED_ACTIVE_HIGH
    HAL_GPIO_WritePin(port, pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(port, pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif
}

/* Nhip heartbeat ma hoa trang thai truyen thong. Uu tien tu nang xuong nhe. */
static uint32_t hb_pattern(uint8_t flags)
{
    if (flags & LED_F_CH_STALE) return PAT_HB3;   /* co kenh mat lien lac  */
    if (!(flags & LED_F_PC_LINK)) return PAT_HB2; /* chua tung nhan lenh PC */
    return PAT_HB1;
}

/* ==========================================================================
 * API
 * ========================================================================== */

void Led_Init(void)
{
    s_slot   = 0u;
    s_t_last = HAL_GetTick();

    /* Self-test: ca 3 den sang. Safety se giu SAFE_BOOT trong SAFE_BOOT_MS,
     * nen nguoi van hanh thay duoc ca 3 den truoc khi vao che do binh thuong. */
    s_pat_g = PAT_SOLID;
    s_pat_b = PAT_SOLID;
    s_pat_r = PAT_SOLID;

    led_write(GR_LED_GPIO_Port, GR_LED_Pin, true);
    led_write(BL_LED_GPIO_Port, BL_LED_Pin, true);
    led_write(RD_LED_GPIO_Port, RD_LED_Pin, true);
}

void Led_SetStatus(safe_state_t st, fault_code_t code, uint8_t flags)
{
    uint32_t g, b, r;

    switch (st) {

    case SAFE_BOOT:
        g = PAT_SOLID;  b = PAT_SOLID;  r = PAT_SOLID;   /* self-test */
        break;

    case SAFE_OFF:
        g = hb_pattern(flags);
        b = PAT_OFF;
        r = (flags & (LED_F_WARN | LED_F_BUS_LOST)) ? PAT_WARN : PAT_OFF;
        break;

    case SAFE_ARMING:
        g = hb_pattern(flags);
        b = PAT_5HZ;                                     /* dang len nguon */
        r = PAT_OFF;
        break;

    case SAFE_ON:
        g = hb_pattern(flags);
        b = PAT_SOLID;                                   /* CO DIEN O NGO RA */
        r = (flags & (LED_F_WARN | LED_F_BUS_LOST)) ? PAT_WARN : PAT_OFF;
        break;

    case SAFE_TRIPPED:
        /* Tat xanh la va xanh duong de den do la thu duy nhat dap vao mat */
        g = PAT_OFF;
        b = PAT_OFF;
        r = pat_count((uint8_t)code);
        if (r == 0uL) r = PAT_5HZ;   /* code chua phan loai xong */
        break;

    case SAFE_LOCKOUT:
        g = PAT_OFF;
        b = PAT_OFF;
        r = PAT_SOLID;
        break;

    default:
        g = PAT_OFF;  b = PAT_OFF;  r = PAT_5HZ;
        break;
    }

    /* Chi ghi khi doi -> khong reset pha cua pattern moi vong main loop */
    if (g != s_pat_g) s_pat_g = g;
    if (b != s_pat_b) s_pat_b = b;
    if (r != s_pat_r) s_pat_r = r;
}

void Led_Task(uint32_t now_ms)
{
    if ((uint32_t)(now_ms - s_t_last) < LED_TICK_MS) return;
    s_t_last += LED_TICK_MS;          /* cong don: khong troi nhip */

    /* Neu main loop bi treo lau, bat kip lai thay vi nhay don dap */
    if ((uint32_t)(now_ms - s_t_last) > (10u * LED_TICK_MS)) {
        s_t_last = now_ms;
    }

    uint32_t bit = 1uL << s_slot;

    led_write(GR_LED_GPIO_Port, GR_LED_Pin, (s_pat_g & bit) != 0uL);
    led_write(BL_LED_GPIO_Port, BL_LED_Pin, (s_pat_b & bit) != 0uL);
    led_write(RD_LED_GPIO_Port, RD_LED_Pin, (s_pat_r & bit) != 0uL);

    s_slot = (uint8_t)((s_slot + 1u) & (LED_SLOTS - 1u));
}

void Led_PanicBlink(void)
{
    /* Khong dung HAL_Delay: SysTick co the da bi tat trong Error_Handler. */
    for (;;) {
        led_write(RD_LED_GPIO_Port, RD_LED_Pin, true);
        led_write(GR_LED_GPIO_Port, GR_LED_Pin, false);
        led_write(BL_LED_GPIO_Port, BL_LED_Pin, false);
        for (volatile uint32_t i = 0; i < 2000000u; i++) { __NOP(); }

        led_write(RD_LED_GPIO_Port, RD_LED_Pin, false);
        for (volatile uint32_t i = 0; i < 2000000u; i++) { __NOP(); }
    }
}
