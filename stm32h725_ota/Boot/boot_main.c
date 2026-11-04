#include "main.h"
#include "boot_main.h"
#include "ota_image.h"
#include "cfg_store.h"
#include "ota_select.h"
#include "iwdg_hw.h"
#include "boot_recovery.h"

static void tick_wdg(void)
{
    IWDG_Refresh();
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

/* Cua so cho khung tu PC ngay sau khi bat nguon, khi app khoi dong duoc: cho phep nap lai ca khi app
 * "hop le nhung treo" (khong the nhan lenh ENTER_REC). Het gio ma khong co khung nao -> vao app. */
#ifndef BOOT_RECOVERY_WINDOW_MS
#define BOOT_RECOVERY_WINDOW_MS   300u
#endif

void Boot_Run(void)
{
    CfgStore_Init();

    /* App chi khoi dong khi: anh hop le, chua bi danh dau vao recovery, va chua het luot thu.
     * Khong khoi dong duoc -> recovery vinh vien (khong tro ve). */
    ota_slot_t sl = Ota_SelectSlot(tick_wdg);
    if (sl == OTA_SLOT_NONE) {
        Boot_RecoveryRun(0u);                       /* khong tro ve */
        while (1) {}
    }

    /* App khoi dong duoc: nghe PC trong mot cua so ngan truoc khi nhay vao app. */
    Boot_RecoveryRun(BOOT_RECOVERY_WINDOW_MS);

    /* Dem luot thu cho anh chua confirm (anh da confirm khong bi dem). Neu khong ghi duoc bo dem van khoi dong
     * (de thiet bi khong chet), nhung trial khong con bao ve. */
    (void)Ota_CommitTrial(sl);

    IWDG_Refresh();
    jump_to_slot(sl);

    while (1) {}
}
