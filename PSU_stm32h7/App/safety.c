/**
 ******************************************************************************
 * @file    safety.c
 * @brief   May trang thai bao ve, MOT may cho moi cum DCM. Xem safety.h.
 ******************************************************************************
 */

#include <comm.h>
#include <fw_update.h>
#include <ina228_driver.h>
#include <led.h>
#include <safety.h>
#include <psu_trip.h>
#include <string.h>
#include <stddef.h>
#include "main.h"

/* ==========================================================================
 * PHAN CUNG TUNG CUM
 * Cum chua co chan EN (port == NULL) van co may trang thai nhung luon OFF.
 * ========================================================================== */
typedef struct {
    GPIO_TypeDef *port;       /* chan EN cua cum; NULL = chua co phan cung */
    uint16_t      pin;
    uint8_t       ina_mask;   /* bit i = INA thu i giam sat DCM cua cum nay  */
    uint8_t       ft_mask;    /* bit n = chan FT thu n cua DCM thuoc cum nay */
} cluster_hw_t;

static const cluster_hw_t k_hw[PSU_CLUSTERS] = {
    /* Cum 1 (mach bring-up): PWR_EN dieu khien 4 DCM; INA1..3 giam sat DCM1..3 (INA4 bo qua);
     * 4 chan FT deu thuoc cum nay. */
    [0] = { PWR_EN_CTRL_GPIO_Port, PWR_EN_CTRL_Pin, 0x07u, 0x0Fu },
    /* Cum 2..4: chua co phan cung -> dien vao day khi co chan EN va INA */
    [1] = { NULL, 0u, 0u, 0u },
    [2] = { NULL, 0u, 0u, 0u },
    [3] = { NULL, 0u, 0u, 0u },
};

/* ==========================================================================
 * TRANG THAI NOI BO
 * ========================================================================== */
typedef struct {
    volatile safe_state_t state;
    volatile bool         fast_trip;    /* set boi EXTI, xoa boi Task */
    fault_code_t          code;
    uint8_t               trip_count;
    uint32_t              t_state;      /* thoi diem vao trang thai hien tai */
    uint32_t              t_trip;
    bool                  pc_wants_on;
    bool                  pwr_on;       /* trang thai LOGIC da ghi ra chan   */
    uint8_t               led_flags;    /* co phu cho LED, tinh o Safety_Task */
} cluster_t;

static cluster_t cl[PSU_CLUSTERS];

static bool hw_present(uint8_t c) { return k_hw[c].port != NULL; }

/* ==========================================================================
 * LOP TRUY CAP CHAN NGUON
 * Doi cuc tinh o DUY NHAT mot cho: macro SAFE_PWR_ACTIVE_HIGH.
 * ========================================================================== */
static inline void pwr_write(uint8_t c, bool on)
{
    if (hw_present(c)) {
        GPIO_PinState lvl;
#if SAFE_PWR_ACTIVE_HIGH
        lvl = on ? GPIO_PIN_SET : GPIO_PIN_RESET;
#else
        lvl = on ? GPIO_PIN_RESET : GPIO_PIN_SET;
#endif
        HAL_GPIO_WritePin(k_hw[c].port, k_hw[c].pin, lvl);
    }
    cl[c].pwr_on = on && hw_present(c);
}

/* Ghi thang BSRR: an toan khi goi tu HardFault/BusFault/Error_Handler/Default_Handler. */
void Safety_ForceOff(void)
{
    for (uint8_t c = 0; c < PSU_CLUSTERS; c++) {
        if (!hw_present(c)) continue;
#if SAFE_PWR_ACTIVE_HIGH
        k_hw[c].port->BSRR = (uint32_t)k_hw[c].pin << 16;
#else
        k_hw[c].port->BSRR = (uint32_t)k_hw[c].pin;
#endif
    }
}

/* Ghi de ham weak trong shared_lib/flash_guard.c */
void Fault_Hook(void)
{
    Safety_ForceOff();
}

static void goto_state(uint8_t c, safe_state_t st, uint32_t now)
{
    if (cl[c].state == st) return;
    cl[c].state   = st;
    cl[c].t_state = now;
}

/* Xoa het co loi tich luy tren moi kenh INA, GIU LAI bit MEMSTAT.
 * MEMSTAT la loi phan cung vinh vien (trim memory hong), khong duoc phep
 * bi xoa boi mot lenh ClearTrip thong thuong. */
static void sticky_clear_mask(uint8_t ina_mask)
{
    for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
        if ((ina_mask & (1u << i)) == 0u) continue;
        ina228_dev_t *d = ina228_get_dev(i);
        if (d != NULL) {
            d->diag_sticky &= INA228_FLAG_MEMSTAT;
            d->alert_new    = false;
        }
    }
}

/* ==========================================================================
 * PHAN LOAI LOI
 * ========================================================================== */

/* Chi 6 bit thuc su kich chan ALERT (theo datasheet) con toi day, vi
 * active_mask() luon xuat phat tu INA228_FAULT_MASK (ina228_cfg.h) - noi
 * da CHU DINH bo MATHOF ra. Xem giai thich chi tiet o do: MATHOF khong
 * bao gio keo ALERT, chi den qua vong doc DIAG_ALRT dinh ky (~250 ms),
 * nen dua no vao day se lam "duong nhanh" tro thanh "duong 250 ms" cho
 * mot dieu kien khong phai la su co dien that su. */
static fault_code_t classify(uint16_t diag)
{
    if (diag & (INA228_FLAG_SHNTOL | INA228_FLAG_SHNTUL)) return FCODE_OVERCUR;
    if (diag &  INA228_FLAG_POL)                          return FCODE_OVERCUR;
    if (diag &  INA228_FLAG_BUSOL)                        return FCODE_OVERVOLT;
    if (diag &  INA228_FLAG_TMPOL)                        return FCODE_OVERTEMP;
    if (diag &  INA228_FLAG_BUSUL)                        return FCODE_UNDERVOLT;
    return FCODE_NONE;
}

/* Bit nao duoc phep gay trip, tuy trang thai nguon cua cum.
 * Nguon TAT -> V_bus = 0 -> BUSUL luon dung -> phai mask, neu khong
 * se khong bao gio thoat duoc latch. */
static uint16_t active_mask(uint8_t c)
{
    uint16_t m = INA228_FAULT_MASK;
    if (cl[c].state != SAFE_ON) {
        m &= (uint16_t)~INA228_FLAG_BUSUL;
    }
    return m;
}

/* Dua co tich luy cua INA thanh bit trip code (bang tam thoi - xem psu_trip.h; chi cum 1 duoc gan bit).
 * include_under: tinh ca co "duoi" (SHNTUL, BUSUL). Sau khi da cat nguon, dong/ap ve 0 co the tu
 * sinh co duoi -> khi TRIPPED chi tin co "qua" (khong phai he qua cua viec cat nguon). */
static void latch_hw_bits(const uint16_t chf[INA228_CH_COUNT], bool include_under)
{
    for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
        const uint16_t f = chf[i];
        const uint8_t  b = (uint8_t)(1u << i);                       /* DCM thu i cua cum 1 */
        const uint8_t  u = (uint8_t)(1u << (PSU_DCM_PER_CLUSTER + i));
        if (f & (INA228_FLAG_SHNTOL | INA228_FLAG_POL)) (void)PsuTrip_Set(PSU_TRIP_B_INA_OC, b);
        if (f & INA228_FLAG_BUSOL)                      (void)PsuTrip_Set(PSU_TRIP_B_INA_V,  b);
        if (f & INA228_FLAG_TMPOL)                      (void)PsuTrip_Set(PSU_TRIP_B_TEMP_FT, b);
        if (include_under) {
            if (f & INA228_FLAG_SHNTUL) (void)PsuTrip_Set(PSU_TRIP_B_INA_OC, u);
            if (f & INA228_FLAG_BUSUL)  (void)PsuTrip_Set(PSU_TRIP_B_INA_V,  u);
        }
    }
}

/* Trip den tu duong nhanh (ISR) chua biet ma loi. Sau SAFE_SETTLE_MS thi vet
 * lai tu diag da duoc ina228_alert_process() cap nhat. */
static void resolve_trip_code(uint8_t c, uint32_t now, uint16_t any_fault, bool dcm_fault)
{
    if (cl[c].code != FCODE_NONE) return;
    if ((uint32_t)(now - cl[c].t_trip) < SAFE_SETTLE_MS) return;

    if (any_fault != 0u) {
        cl[c].code = classify(any_fault);
    } else if (dcm_fault) {
        cl[c].code = FCODE_DCM;
    } else if ((PsuTrip_Bytes()[PSU_TRIP_B_SW_MAX] | PsuTrip_Bytes()[PSU_TRIP_B_SW_MIN]) != 0u) {
        cl[c].code = FCODE_THRESHOLD;
    } else {
        /* Chan ALERT da kich nhung I2C khong xac nhan duoc bit nao */
        cl[c].code = FCODE_BUS;
        (void)PsuTrip_Set(PSU_TRIP_B_MISC, PSU_TRIP_MISC_UNKNOWN);
    }
}

static void count_trip(uint8_t c)
{
    if (cl[c].trip_count < 255u) cl[c].trip_count++;
}

/* ==========================================================================
 * KHOI TAO
 * ========================================================================== */

void Safety_Init(void)
{
    const uint32_t t0 = HAL_GetTick();

    PsuTrip_Init();
    memset(cl, 0, sizeof(cl));
    for (uint8_t c = 0; c < PSU_CLUSTERS; c++) {
        cl[c].state   = SAFE_BOOT;
        cl[c].code    = FCODE_NONE;
        cl[c].t_state = t0;
        cl[c].t_trip  = t0;
        pwr_write(c, false);
    }
}

/* ==========================================================================
 * DUONG NHANH - CHAY TRONG EXTI ISR
 * Chi dat chan + mot co. Task se phan loai va chuyen trang thai.
 * ========================================================================== */

uint8_t Safety_ClusterOfIna(uint8_t ina_idx)
{
    for (uint8_t c = 0; c < PSU_CLUSTERS; c++) {
        if (ina_idx < 8u && (k_hw[c].ina_mask & (1u << ina_idx)) != 0u) return c;
    }
    return 0xFFu;
}

void Safety_FastTrip(uint8_t ina_idx)
{
    const uint8_t c = Safety_ClusterOfIna(ina_idx);
    if (c == 0xFFu) return;

    /* ON: giam sat day du. ARMING: nguon da CO DIEN that (chi tam bo qua
     * ALERT o Task de khong nham inrush thanh loi) - mot ngan mach that
     * trong cua so nay van phai cat ngay, khong the doi het ARM_MS.
     * BOOT/OFF/TRIPPED: chua co dien hoac da cat -> bo qua. */
    if (cl[c].state != SAFE_ON && cl[c].state != SAFE_ARMING) return;

    pwr_write(c, false);
    cl[c].fast_trip = true;
}

/* ==========================================================================
 * TASK CHINH - mot cum
 * ========================================================================== */

static void cluster_task(uint8_t c, uint32_t now, bool pc_link)
{
    cluster_t *k = &cl[c];
    const cluster_hw_t *hw = &k_hw[c];

    uint16_t any_fault = 0u;
    uint8_t  stale_ch  = 0u;
    uint8_t  cfgbad_ch = 0u;
    uint8_t  alive_ch  = 0u;
    uint8_t  n_ina     = 0u;
    uint16_t mact      = active_mask(c);
    uint16_t chf[INA228_CH_COUNT];
    psu_trip_in_t ti;

    memset(chf, 0, sizeof(chf));
    memset(&ti, 0, sizeof(ti));

    /* --- Gom trang thai cac INA cua cum --- */
    uint8_t dcm_idx = 0u;
    for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
        if ((hw->ina_mask & (1u << i)) == 0u) continue;
        n_ina++;

        ina228_dev_t *d = ina228_get_dev(i);
        if (d == NULL) { stale_ch |= (uint8_t)(1u << i); dcm_idx++; continue; }

        if (dcm_idx < PSU_DCM_PER_CLUSTER) {
            ti.cur24[dcm_idx]  = d->raw.current_reg24;
            ti.temp[dcm_idx]   = d->raw.dietemp_raw;
            ti.stamp[dcm_idx]  = d->meas.timestamp_ms;
            ti.fresh[dcm_idx]  = ina228_is_fresh(d, now);
        }
        dcm_idx++;

        uint16_t f = (uint16_t)(d->diag_sticky & mact);
        chf[i] = f;
        if (f != 0u) any_fault |= f;

        if (ina228_is_fresh(d, now)) {
            alive_ch |= (uint8_t)(1u << i);
            if (!d->cfg_ok) cfgbad_ch |= (uint8_t)(1u << i);
        } else {
            stale_ch |= (uint8_t)(1u << i);
        }
    }

    /* Chan FT cua DCM la push-pull TICH CUC ca khi co loi LAN khi EN muc
     * thap (theo datasheet DCM4623). Nhung KHAC voi BUSUL (can doi het
     * 16-300 ms de dien ap on dinh moi dang tin), FT NHA CHI ~200 us sau
     * khi EN len muc cao - nhanh hon nhieu so voi chu ky main loop
     * (~1 ms). Vi vay ngay tu tick DAU TIEN trong SAFE_ARMING, FT DA la
     * tin hieu dang tin: neu no van active luc do, day KHONG PHAI la
     * qua trinh khoi dong binh thuong nua ma la loi that. Chi mask khi
     * dang SAFE_BOOT/OFF, luc EN CHU DONG o muc thap nen FT active la
     * dung theo thiet ke, khong phai loi. */
    const uint8_t dcm_raw = (uint8_t)(Comm_DcmFault() & hw->ft_mask);
    const bool dcm_fault = (dcm_raw != 0u) && (k->state == SAFE_ON || k->state == SAFE_ARMING);
    const bool bus_lost  = (n_ina != 0u) && (alive_ch == 0u);

    /* --- Trip code: so sanh nguong (chi khi SAFE_ON) + co phan cung. Bang bit hien chi gan cho cum 1. --- */
    const bool sw_trip = PsuTrip_Evaluate(c, k->state == SAFE_ON, &ti);
    if (c == 0u) {
        if (k->state == SAFE_ON)           latch_hw_bits(chf, true);
        else if (k->state == SAFE_TRIPPED) latch_hw_bits(chf, false);
        if (dcm_fault) (void)PsuTrip_Set(PSU_TRIP_B_TEMP_FT, (uint8_t)((dcm_raw & 0x0Fu) << PSU_DCM_PER_CLUSTER));
    }

    /* --- Duong nhanh tu ISR: chuyen trang thai o day, khong o ISR --- */
    if (k->fast_trip) {
        k->fast_trip = false;
        if (k->state == SAFE_ON || k->state == SAFE_ARMING) {
            k->code   = FCODE_NONE;   /* chua biet, se vet lai o SAFE_TRIPPED */
            k->t_trip = now;
            count_trip(c);
            goto_state(c, SAFE_TRIPPED, now);
        }
    }

    /* --- May trang thai --- */
    switch (k->state) {

    case SAFE_BOOT:
        pwr_write(c, false);
        if ((uint32_t)(now - k->t_state) >= SAFE_BOOT_MS) {
            sticky_clear_mask(hw->ina_mask);   /* vut co POR sinh ra luc khoi tao */
            goto_state(c, SAFE_OFF, now);
        }
        break;

    case SAFE_OFF:
        pwr_write(c, false);
        /* Datasheet DCM4623 (tOFF-MONOTONIC): can it nhat 100 ms o trang
         * thai disable de dam bao soft-start lan sau van dung nhu du
         * tinh (xem SAFE_OFF_MIN_MS). Neu chua du, VAN GIU pc_wants_on
         * (khong huy yeu cau cua PC) - chi tri hoan viec thuc thi. */
        if (k->pc_wants_on && hw_present(c) &&
            (uint32_t)(now - k->t_state) >= SAFE_OFF_MIN_MS) {
            sticky_clear_mask(hw->ina_mask);   /* xoa ton du truoc khi arm */
            pwr_write(c, true);
            goto_state(c, SAFE_ARMING, now);
        }
        break;

    case SAFE_ARMING:
        if (!k->pc_wants_on) {
            pwr_write(c, false);
            goto_state(c, SAFE_OFF, now);
            break;
        }

        /* Loi that trong luc dang len nguon (vd ngan mach ngay khi vua
         * bat) duoc FT bao lai gan nhu tuc thi (~200 us) - cat ngay,
         * khong doi het SAFE_ARM_MS. dcm_fault o day da duoc tin cay tu
         * dau ARMING (xem giai thich o tren). */
        if (dcm_fault) {
            pwr_write(c, false);
            k->code   = FCODE_DCM;
            k->t_trip = now;
            count_trip(c);
            goto_state(c, SAFE_TRIPPED, now);
            break;
        }

        /* Khong the dung FT de xac nhan dien ap ra da on dinh (FT nha
         * qua som, ~200 us, trong khi dien ap can toi 16-300 ms theo
         * datasheet DCM). Chi con cach dung TIMER thuan tuy, dat du dai
         * de trum het bien tren cua khoang do (xem SAFE_ARM_MS). */
        if ((uint32_t)(now - k->t_state) >= SAFE_ARM_MS) {
            sticky_clear_mask(hw->ina_mask);   /* vut co sinh ra trong inrush */
            goto_state(c, SAFE_ON, now);
        }
        break;

    case SAFE_ON:
        if (!k->pc_wants_on) {
            pwr_write(c, false);
            goto_state(c, SAFE_OFF, now);
            break;
        }

        /* Chay on dinh du lau -> dem trip lien tiep ve 0 */
        if ((uint32_t)(now - k->t_state) >= SAFE_STABLE_MS) {
            k->trip_count = 0u;
        }

        if (any_fault != 0u || dcm_fault || sw_trip
#if SAFE_TRIP_ON_BUS_LOSS
            || bus_lost
#endif
           ) {
            pwr_write(c, false);
            if (dcm_fault)      k->code = FCODE_DCM;
            else if (any_fault) k->code = classify(any_fault);
            else if (sw_trip)   k->code = FCODE_THRESHOLD;
            else {
                k->code = FCODE_BUS;
                (void)PsuTrip_Set(PSU_TRIP_B_MISC, PSU_TRIP_MISC_BUS);
            }
            k->t_trip = now;
            count_trip(c);
            goto_state(c, SAFE_TRIPPED, now);
        }
        break;

    case SAFE_TRIPPED:
        pwr_write(c, false);             /* ghi lai moi vong, phong glitch */

        /* Giu TRIPPED cho den khi PC gui ClearTrip. Khong co trang thai khoa. */
        resolve_trip_code(c, now, any_fault, dcm_fault);
        break;

    default:
        goto_state(c, SAFE_BOOT, now);
        break;
    }

    /* --- Co phu cho LED --- */
    uint8_t flags = 0u;
    if (pc_link)                               flags |= LED_F_PC_LINK;
    if (stale_ch != 0u)                        flags |= LED_F_CH_STALE;
    if (bus_lost)                              flags |= LED_F_BUS_LOST;
    if (cfgbad_ch != 0u || dcm_fault ||
        (any_fault != 0u && k->state != SAFE_TRIPPED))
                                               flags |= LED_F_WARN;
    k->led_flags = flags;
}

/* Cum "dang noi bat nhat" cho LED: TRIPPED > ARMING > ON > OFF > BOOT; bang nhau thi cum so nho */
static uint8_t led_rank(safe_state_t st)
{
    switch (st) {
    case SAFE_TRIPPED: return 5u;
    case SAFE_ARMING:  return 4u;
    case SAFE_ON:      return 3u;
    case SAFE_OFF:     return 2u;
    default:           return 1u;
    }
}

void Safety_Task(uint32_t now)
{
    const bool pc_link = Comm_PcLinkAlive(now);

    for (uint8_t c = 0; c < PSU_CLUSTERS; c++) {
        cluster_task(c, now, pc_link);
    }

    uint8_t best = 0u;
    for (uint8_t c = 1; c < PSU_CLUSTERS; c++) {
        if (led_rank(cl[c].state) > led_rank(cl[best].state)) best = c;
    }
    Led_SetStatus(cl[best].state, cl[best].code, cl[best].led_flags);
}

/* ==========================================================================
 * LENH TU PC
 * ========================================================================== */

bool Safety_RequestPower(uint8_t c, bool on)
{
    if (c >= PSU_CLUSTERS) return false;

    if (!hw_present(c)) {                       /* cum chua co chan EN: chi nhan lenh tat */
        cl[c].pc_wants_on = false;
        return !on;
    }

    if (on && cl[c].state == SAFE_TRIPPED) {
        return false;                    /* PC phai gui ClearTrip truoc */
    }

    if (on && FwUpdate_IsBusy()) return false;

    cl[c].pc_wants_on = on;
    return true;
}

void Safety_ClearFault(void)
{
    PsuTrip_Clear();

    for (uint8_t c = 0; c < PSU_CLUSTERS; c++) {
        sticky_clear_mask(k_hw[c].ina_mask);
        cl[c].code = FCODE_NONE;

        if (cl[c].state == SAFE_TRIPPED) {
            /* Huy yeu cau bat (KHONG tu bat lai) va ve OFF. KHONG xoa trip_count: chi reset sau
             * SAFE_STABLE_MS chay on dinh o SAFE_ON. */
            cl[c].pc_wants_on = false;
            goto_state(c, SAFE_OFF, HAL_GetTick());
        }
    }
}

void Safety_Control(uint8_t mask, bool clear)
{
    bool skip[PSU_CLUSTERS];

    for (uint8_t c = 0; c < PSU_CLUSTERS; c++) skip[c] = false;

    if (clear) {
        for (uint8_t c = 0; c < PSU_CLUSTERS; c++) skip[c] = (cl[c].state == SAFE_TRIPPED);
        Safety_ClearFault();
    }

    for (uint8_t c = 0; c < PSU_CLUSTERS; c++) {
        if (skip[c] || !hw_present(c)) continue;           /* lenh bat trong cung khung voi clear bi bo qua */
        (void)Safety_RequestPower(c, (mask & (1u << c)) != 0u);
    }
}

bool Safety_IsArmed(uint8_t c)
{
    return (c < PSU_CLUSTERS) && cl[c].state == SAFE_ON;
}

bool Safety_AllOff(void)
{
    for (uint8_t c = 0; c < PSU_CLUSTERS; c++) {
        if (cl[c].state != SAFE_OFF) return false;
    }
    return true;
}

bool Safety_AnyPowerOn(void)
{
    for (uint8_t c = 0; c < PSU_CLUSTERS; c++) {
        if (cl[c].pwr_on) return true;
    }
    return false;
}

/* ==========================================================================
 * TRUY VAN
 * ========================================================================== */

safe_state_t Safety_State(uint8_t c)    { return (c < PSU_CLUSTERS) ? cl[c].state : SAFE_BOOT; }
fault_code_t Safety_Code(uint8_t c)     { return (c < PSU_CLUSTERS) ? cl[c].code  : FCODE_NONE; }
bool         Safety_PowerIsOn(uint8_t c){ return (c < PSU_CLUSTERS) && cl[c].pwr_on; }
uint8_t      Safety_TripCount(uint8_t c){ return (c < PSU_CLUSTERS) ? cl[c].trip_count : 0u; }
