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

void Boot_Run(void)
{
    CfgStore_Init();

    /* Chi slot hop le, chua bi rollback va chua het luot moi duoc chon.
     * Khong con muc "noi long": het luot = khong bao gio chon lai. */
    const ota_slot_t sl = Ota_SelectSlot(tick_wdg);
    if (sl == OTA_SLOT_NONE) Boot_RecoveryRun();   /* khong tro ve */

    /* CHI dem luot thu cho anh chua confirm. Anh da confirm khong bao
     * gio bi ha cap vi reset (watchdog, mat dien, tat/bat nhanh). */
    cfg_boot_payload_t bp;
    const bool has_rec = CfgStore_ReadBoot(sl, &bp);
    if (!has_rec || !bp.confirmed) {
        const uint32_t count = has_rec ? bp.boot_count : 0u;
        (void)CfgStore_WriteBoot(sl, count + 1u, 0u);
    }

    IWDG_Refresh();
    jump_to_slot(sl);

    while (1) {}
}
