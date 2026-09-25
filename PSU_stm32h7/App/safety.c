/**
 ******************************************************************************
 * @file    safety.c
 * @brief   May trang thai bao ve. Xem safety.h de biet nguyen tac thiet ke.
 ******************************************************************************
 */

#include <comm.h>
#include <fw_update.h>
#include <ina228_driver.h>
#include <led.h>
#include <safety.h>
#include <stddef.h>
#include "main.h"

/* ==========================================================================
 * TRANG THAI NOI BO
 * ========================================================================== */

static struct {
    volatile safe_state_t state;
    volatile bool         fast_trip;    /* set boi EXTI, xoa boi Task */
    fault_code_t          code;
    uint8_t               trip_mask;
    uint8_t               trip_count;
    uint32_t              t_state;      /* thoi diem vao trang thai hien tai */
    uint32_t              t_trip;
    bool                  pc_wants_on;
    bool                  pwr_on;       /* trang thai LOGIC da ghi ra chan   */
} s;

/* ==========================================================================
 * LOP TRUY CAP CHAN NGUON
 * Doi cuc tinh o DUY NHAT mot cho: macro SAFE_PWR_ACTIVE_HIGH.
 * ========================================================================== */

static inline void pwr_write(bool on)
{
    GPIO_PinState lvl;

#if SAFE_PWR_ACTIVE_HIGH
    lvl = on ? GPIO_PIN_SET : GPIO_PIN_RESET;
#else
    lvl = on ? GPIO_PIN_RESET : GPIO_PIN_SET;
#endif

    HAL_GPIO_WritePin(PWR_EN_CTRL_GPIO_Port, PWR_EN_CTRL_Pin, lvl);
    s.pwr_on = on;
}

static void goto_state(safe_state_t st, uint32_t now)
{
    if (s.state == st) return;
    s.state   = st;
    s.t_state = now;
}

/* Xoa het co loi tich luy tren ca 4 kenh, GIU LAI bit MEMSTAT.
 * MEMSTAT la loi phan cung vinh vien (trim memory hong), khong duoc phep
 * bi xoa boi mot lenh CLR_FAULT thong thuong. */
static void sticky_clear_all(void)
{
    for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
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

/* Bit nao duoc phep gay trip, tuy trang thai nguon.
 * Nguon TAT -> V_bus = 0 -> BUSUL luon dung -> phai mask, neu khong
 * se khong bao gio thoat duoc latch. */
static uint16_t active_mask(void)
{
    uint16_t m = INA228_FAULT_MASK;
    if (s.state != SAFE_ON) {
        m &= (uint16_t)~INA228_FLAG_BUSUL;
    }
    return m;
}

/* ==========================================================================
 * KHOI TAO
 * ========================================================================== */

void Safety_Init(void)
{
    s.state       = SAFE_BOOT;
    s.fast_trip   = false;
    s.code        = FCODE_NONE;
    s.trip_mask   = 0u;
    s.trip_count  = 0u;
    s.pc_wants_on = false;
    s.t_state     = HAL_GetTick();
    s.t_trip      = s.t_state;

    pwr_write(false);
}

/* ==========================================================================
 * DUONG NHANH - CHAY TRONG EXTI ISR
 * Chi dat chan + mot co. Task se phan loai va chuyen trang thai.
 * ========================================================================== */

void Safety_FastTrip(void)
{
    /* ON: giam sat day du. ARMING: nguon da CO DIEN that (chi tam bo qua
     * ALERT o Task de khong nham inrush thanh loi) - mot ngan mach that
     * trong cua so nay van phai cat ngay, khong the doi het ARM_MS.
     * BOOT/OFF/TRIPPED/LOCKOUT: chua co dien hoac da cat -> bo qua. */
    if (s.state != SAFE_ON && s.state != SAFE_ARMING) return;

    pwr_write(false);
    s.fast_trip = true;
}

/* ==========================================================================
 * TASK CHINH
 * ========================================================================== */

void Safety_Task(uint32_t now)
{
    uint16_t any_fault = 0u;
    uint8_t  fault_ch  = 0u;
    uint8_t  stale_ch  = 0u;
    uint8_t  cfgbad_ch = 0u;
    uint8_t  alive_ch  = 0u;
    uint16_t mact      = active_mask();

    /* --- Gom trang thai 4 kenh --- */
    for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
        ina228_dev_t *d = ina228_get_dev(i);
        if (d == NULL) { stale_ch |= (uint8_t)(1u << i); continue; }

        uint16_t f = (uint16_t)(d->diag_sticky & mact);
        if (f != 0u) {
            any_fault |= f;
            fault_ch  |= (uint8_t)(1u << i);
        }
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
    bool dcm_fault_raw = (Comm_DcmFault() != 0u);
    bool dcm_fault      = dcm_fault_raw &&
                          (s.state == SAFE_ON || s.state == SAFE_ARMING);
    bool bus_lost  = (alive_ch == 0u);

    /* --- Duong nhanh tu ISR: chuyen trang thai o day, khong o ISR --- */
    if (s.fast_trip) {
        s.fast_trip = false;
        if (s.state == SAFE_ON || s.state == SAFE_ARMING) {
            s.code     = FCODE_NONE;   /* chua biet, se vet lai o SAFE_TRIPPED */
            s.trip_mask = 0u;
            s.t_trip   = now;
            if (s.trip_count < 255u) s.trip_count++;
            goto_state(SAFE_TRIPPED, now);
        }
    }

    /* --- May trang thai --- */
    switch (s.state) {

    case SAFE_BOOT:
        pwr_write(false);
        if ((uint32_t)(now - s.t_state) >= SAFE_BOOT_MS) {
            sticky_clear_all();          /* vut co POR sinh ra luc khoi tao */
            goto_state(SAFE_OFF, now);
        }
        break;

    case SAFE_OFF:
        pwr_write(false);
        /* Datasheet DCM4623 (tOFF-MONOTONIC): can it nhat 100 ms o trang
         * thai disable de dam bao soft-start lan sau van dung nhu du
         * tinh (xem SAFE_OFF_MIN_MS). Neu chua du, VAN GIU pc_wants_on
         * (khong huy yeu cau cua PC) - chi tri hoan viec thuc thi. */
        if (s.pc_wants_on &&
            (uint32_t)(now - s.t_state) >= SAFE_OFF_MIN_MS) {
            sticky_clear_all();          /* xoa ton du truoc khi arm */
            pwr_write(true);
            goto_state(SAFE_ARMING, now);
        }
        break;

    case SAFE_ARMING:
        if (!s.pc_wants_on) {
            pwr_write(false);
            goto_state(SAFE_OFF, now);
            break;
        }

        /* Loi that trong luc dang len nguon (vd ngan mach ngay khi vua
         * bat) duoc FT bao lai gan nhu tuc thi (~200 us) - cat ngay,
         * khong doi het SAFE_ARM_MS. dcm_fault o day da duoc tin cay tu
         * dau ARMING (xem giai thich o tren). */
        if (dcm_fault) {
            pwr_write(false);
            s.code      = FCODE_DCM;
            s.trip_mask = 0u;   /* PWR_EN dung chung, khong co kenh rieng */
            s.t_trip    = now;
            if (s.trip_count < 255u) s.trip_count++;
            goto_state(SAFE_TRIPPED, now);
            break;
        }

        /* Khong the dung FT de xac nhan dien ap ra da on dinh (FT nha
         * qua som, ~200 us, trong khi dien ap can toi 16-300 ms theo
         * datasheet DCM). Chi con cach dung TIMER thuan tuy, dat du dai
         * de trum het bien tren cua khoang do (xem SAFE_ARM_MS). */
        if ((uint32_t)(now - s.t_state) >= SAFE_ARM_MS) {
            sticky_clear_all();          /* vut co sinh ra trong inrush */
            goto_state(SAFE_ON, now);
        }
        break;

    case SAFE_ON:
        if (!s.pc_wants_on) {
            pwr_write(false);
            goto_state(SAFE_OFF, now);
            break;
        }

        /* Chay on dinh du lau -> coi nhu su co truoc do da het */
        if ((uint32_t)(now - s.t_state) >= SAFE_STABLE_MS) {
            s.trip_count = 0u;
        }

        if (any_fault != 0u || dcm_fault
#if SAFE_TRIP_ON_BUS_LOSS
            || bus_lost
#endif
           ) {
            pwr_write(false);
            s.trip_mask = fault_ch;
            if (dcm_fault)      s.code = FCODE_DCM;
            else if (any_fault) s.code = classify(any_fault);
            else                s.code = FCODE_BUS;
            s.t_trip = now;
            if (s.trip_count < 255u) s.trip_count++;
            goto_state(SAFE_TRIPPED, now);
        }
        break;

    case SAFE_TRIPPED:
        pwr_write(false);                /* ghi lai moi vong, phong glitch */

        /* Trip den tu duong nhanh: chua biet ma loi. Cho V_bus xa xong roi
         * doc lai diag da duoc ina228_alert_process() cap nhat. */
        if (s.code == FCODE_NONE &&
            (uint32_t)(now - s.t_trip) >= SAFE_SETTLE_MS) {
            if (any_fault != 0u) {
                s.code      = classify(any_fault);
                s.trip_mask = fault_ch;
            } else if (dcm_fault) {
                s.code = FCODE_DCM;
            } else {
                /* Chan ALERT da kich nhung I2C khong xac nhan duoc bit nao */
                s.code = FCODE_BUS;
            }
        }

        if (s.trip_count >= SAFE_TRIP_LOCKOUT_N) {
            goto_state(SAFE_LOCKOUT, now);
        }
        break;

    case SAFE_LOCKOUT:
        pwr_write(false);
        break;

    default:
        goto_state(SAFE_BOOT, now);
        break;
    }

    /* --- Cap nhat LED --- */
    {
        uint8_t flags = 0u;

        if (Comm_PcLinkAlive(now))                 flags |= LED_F_PC_LINK;
        if (stale_ch != 0u)                        flags |= LED_F_CH_STALE;
        if (bus_lost)                              flags |= LED_F_BUS_LOST;
        if (cfgbad_ch != 0u || dcm_fault ||
            (any_fault != 0u && s.state != SAFE_TRIPPED))
                                                   flags |= LED_F_WARN;

        Led_SetStatus(s.state, s.code, flags);
    }
}

/* ==========================================================================
 * LENH TU PC
 * ========================================================================== */

bool Safety_RequestPower(bool on)
{
    if (on && (s.state == SAFE_TRIPPED || s.state == SAFE_LOCKOUT)) {
        return false;                    /* PC phai gui CLR_FAULT truoc */
    }

    if(on && FwUpdate_IsBusy()) return false;

    s.pc_wants_on = on;
    return true;
}

void Safety_ClearFault(void)
{
    sticky_clear_all();

    s.code        = FCODE_NONE;
    s.trip_mask   = 0u;
    s.pc_wants_on = false;               /* KHONG tu dong bat lai nguon */

    if (s.state == SAFE_TRIPPED) {
        s.trip_count = 0u;
        goto_state(SAFE_OFF, HAL_GetTick());
    }
    /* SAFE_LOCKOUT: chi thoat duoc bang reset phan cung. Co y. */
}

/* ==========================================================================
 * TRUY VAN
 * ========================================================================== */

safe_state_t Safety_State(void)    { return s.state;     }
fault_code_t Safety_Code(void)     { return s.code;      }
uint8_t      Safety_TripMask(void) { return s.trip_mask; }
bool         Safety_PowerIsOn(void){ return s.pwr_on;    }
uint8_t      Safety_TripCount(void){ return s.trip_count;}
