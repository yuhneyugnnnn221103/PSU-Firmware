#include "main.h"
#include "boot_main.h"
#include "ota_image.h"
#include "cfg_store.h"
#include "iwdg_hw.h"

#define BOOT_MAX_ATTEMPTS   3u

/* Vung RAM hop le cho MSP ban dau cua app (_estack) */
#define RAM_DTCM_START      0x20000000u
#define RAM_DTCM_END        0x20020000u     /* 128 KB */
#define RAM_AXI_START       0x24000000u
#define RAM_AXI_END         0x24050000u     /* 320 KB */

typedef struct {
    bool     has_rec;
    uint32_t boot_count;
    bool     confirmed;
} slot_state_t;

static slot_state_t read_state(ota_slot_t sl)
{
    slot_state_t st = { false, 0u, false };
    cfg_boot_payload_t bp;

    if (CfgStore_ReadBoot(sl, &bp)) {
        st.has_rec    = true;
        st.boot_count = bp.boot_count;
        st.confirmed  = (bp.confirmed != 0u);
    }
    return st;
}

/* PC da ra lenh ROLLBACK khoi slot nay */
static bool is_rejected(const slot_state_t *st)
{
    return st->has_rec && (st->boot_count == CFG_BOOT_COUNT_FORCE_FAIL);
}

/* Anh CHUA confirm va da thu du so lan ma khong tu confirm duoc */
static bool is_exhausted(const slot_state_t *st)
{
    return st->has_rec && !st->confirmed && (st->boot_count >= BOOT_MAX_ATTEMPTS);
}

/* Kiem 2 word dau cua vector table truoc khi tin tuong nhay vao */
static bool vector_ok(ota_slot_t sl)
{
    const uint32_t base  = Ota_CodeBase(sl);
    const uint32_t msp   = *(const volatile uint32_t *)(base + 0u);
    const uint32_t entry = *(const volatile uint32_t *)(base + 4u);
    const uint32_t lo    = base;
    const uint32_t hi    = g_ota_slots[sl].base + OTA_SLOT_SIZE;

    const bool msp_ok =
        ((msp & 3u) == 0u) &&
        (((msp > RAM_DTCM_START) && (msp <= RAM_DTCM_END)) ||
         ((msp > RAM_AXI_START)  && (msp <= RAM_AXI_END)));

    const bool entry_ok =
        ((entry & 1u) != 0u) &&                     /* bit Thumb */
        ((entry & ~1u) >= lo) && ((entry & ~1u) < hi);

    return msp_ok && entry_ok;
}

/* Anh moi cai hon (install_seq) thang; bang nhau thi so version */
static bool newer(ota_slot_t b, ota_slot_t a)
{
    const ota_image_header_t *ha = Ota_Header(a);
    const ota_image_header_t *hb = Ota_Header(b);

    if (hb->install_seq != ha->install_seq) return hb->install_seq > ha->install_seq;
    return hb->version > ha->version;
}

static ota_slot_t pick(const bool cand[OTA_SLOT_COUNT])
{
    ota_slot_t best = OTA_SLOT_NONE;

    for (ota_slot_t sl = OTA_SLOT_A; sl < OTA_SLOT_COUNT; sl++) {
        if (!cand[sl]) continue;
        if ((best == OTA_SLOT_NONE) || newer(sl, best)) {
            best = sl;
        }
    }
    return best;
}

static ota_slot_t choose_slot(void)
{
    bool         valid[OTA_SLOT_COUNT];
    bool         cand[OTA_SLOT_COUNT];
    slot_state_t st[OTA_SLOT_COUNT];

    for (ota_slot_t sl = OTA_SLOT_A; sl < OTA_SLOT_COUNT; sl++) {
        valid[sl] = Ota_ImageValid(sl) && vector_ok(sl);   /* CRC32 toan anh */
        IWDG_Refresh();
        st[sl]   = read_state(sl);
        cand[sl] = valid[sl] && !is_rejected(&st[sl]) && !is_exhausted(&st[sl]);
    }

    /* Muc 1: ung vien binh thuong */
    ota_slot_t best = pick(cand);
    if (best != OTA_SLOT_NONE) return best;

    /* Muc 2: ca 2 da het luot thu -> bo qua dem, van ton trong ROLLBACK */
    for (ota_slot_t sl = OTA_SLOT_A; sl < OTA_SLOT_COUNT; sl++) {
        cand[sl] = valid[sl] && !is_rejected(&st[sl]);
    }
    best = pick(cand);
    if (best != OTA_SLOT_NONE) return best;

    /* Muc 3: con anh nao dung CRC thi chay, con hon treo board */
    return pick(valid);
}

static void jump_to_slot(ota_slot_t sl)
{
    const uint32_t base  = Ota_CodeBase(sl);
    const uint32_t msp   = *(const volatile uint32_t *)(base + 0u);
    const uint32_t entry = *(const volatile uint32_t *)(base + 4u);

    /* DeInit khi ngat CON BAT: HAL_RCC_DeInit dung HAL_GetTick cho timeout */
    HAL_RCC_DeInit();                 /* goi lai HAL_InitTick -> SysTick bat lai */
    HAL_DeInit();

    __disable_irq();

    /* Tat SysTick SAU DeInit, neu khong se bi bat lai */
    SysTick->CTRL = 0u;
    SysTick->LOAD = 0u;
    SysTick->VAL  = 0u;
    SCB->ICSR     = SCB_ICSR_PENDSTCLR_Msk;

    for (uint32_t i = 0; i < 8u; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }

    HAL_MPU_Disable();

    SCB->VTOR = base;
    __set_CONTROL(0u);
    __DSB();
    __ISB();

    /* Doi MSP va nhay trong CUNG 1 khoi asm: o -O0, bien cuc bo 'entry'
     * nam tren stack, doc lai sau khi doi MSP se ra rac. */
    __asm volatile (
        "msr msp, %0  \n"
        "bx  %1       \n"
        :: "r" (msp), "r" (entry) : "memory");

    while (1) {}
}

static void fatal_no_valid_slot(void)
{
    /* Khong reset lien tuc: reset cung khong tao ra anh hop le.
     * Nhay LED nhanh de bao loi, van giu SWD truy cap duoc. */
    while (1) {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        HAL_Delay(50);
        IWDG_Refresh();
    }
}

void Boot_Run(void)
{
    CfgStore_Init();

    const ota_slot_t sl = choose_slot();
    if (sl == OTA_SLOT_NONE) fatal_no_valid_slot();

    /* CHI dem luot thu cho anh chua confirm. Anh da confirm khong bao
     * gio bi ha cap vi reset (watchdog, mat dien, tat/bat nhanh). */
    const slot_state_t st = read_state(sl);
    if (!st.confirmed && (st.boot_count != CFG_BOOT_COUNT_FORCE_FAIL)) {
        (void)CfgStore_WriteBoot(sl, st.boot_count + 1u, 0u);
    }

    IWDG_Refresh();
    jump_to_slot(sl);

    while (1) {}
}
