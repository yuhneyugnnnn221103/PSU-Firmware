#include "stm32h7xx_hal.h"
#include "flash_guard.h"

static volatile bool s_fault;
static volatile bool s_active;
static volatile uint32_t s_hi, s_lo;

volatile uint32_t g_flash_ecc_fault_count;

#define ECC_FLAGS   (FLASH_FLAG_DBECCERR_BANK1 | FLASH_FLAG_SNECCERR_BANK1)

void FlashGuard_Init(void)						// Enable ngắt BusFault liên quan đến đọc/ghi bộ nhớ
{
	HAL_NVIC_SetPriority(BusFault_IRQn, 0, 0);
    SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk;
    __DSB();
    __ISB();
}

void FlashGuard_Begin(uint32_t lo, uint32_t hi)
{
	__HAL_FLASH_CLEAR_FLAG_BANK1(ECC_FLAGS);		// Xóa cờ lỗi ECC
	s_lo = lo;
	s_hi = hi;
	s_fault = false;
	__DSB();
	s_active = true;
	__asm volatile ("" ::: "memory");
}

bool FlashGuard_End(void)
{
	__asm volatile ("" ::: "memory");
	__DSB();
	s_active = false;

	const bool dbecc = __HAL_FLASH_GET_FLAG_BANK1(FLASH_FLAG_DBECCERR_BANK1);
	__HAL_FLASH_CLEAR_FLAG_BANK1(ECC_FLAGS);

	return !(s_fault || dbecc);

}

void FlashGuard_CopyWords(void *dst, uint32_t src, uint32_t len)
{
    uint32_t *d = (uint32_t *)dst;
    const volatile uint32_t *p = (const volatile uint32_t *)src;

    for (uint32_t i = 0; i < (len / 4u); i++) {
        d[i] = p[i];
    }
}

/* frame[] = stack frame do phan cung day vao: r0 r1 r2 r3 r12 lr pc xpsr */
void flash_guard_busfault_c(uint32_t *frame)
{
    const uint32_t cfsr    = SCB->CFSR;
    const uint32_t bfar    = SCB->BFAR;
    const bool     precise = (cfsr & SCB_CFSR_PRECISERR_Msk) != 0u;
    const bool     in_win  = ((cfsr & SCB_CFSR_BFARVALID_Msk) != 0u) &&
                             (bfar >= s_lo) && (bfar < s_hi);
    const bool     ecc     = __HAL_FLASH_GET_FLAG_BANK1(FLASH_FLAG_DBECCERR_BANK1);

    if (s_active && precise && (in_win || ecc)) {
        s_fault = true;
        g_flash_ecc_fault_count++;

        __HAL_FLASH_CLEAR_FLAG_BANK1(ECC_FLAGS);
        SCB->CFSR = SCB_CFSR_BUSFAULTSR_Msk;          /* ghi 1 de xoa cac bit BusFault */

        /* Bo qua lenh load bi loi: Thumb-2 32-bit neu bit[15:11] >= 0b11101 */
        const uint16_t op = *(const uint16_t *)frame[6];
        frame[6] += ((op >> 11) >= 0x1Du) ? 4u : 2u;
        return;
    }

    /* BusFault that (khong phai doc flash co bao ve) -> de IWDG reset */
    __disable_irq();
    while (1) {}
}

__attribute__((naked)) void BusFault_Handler(void)
{
    __asm volatile (
        "tst   lr, #4                 \n"
        "ite   eq                     \n"
        "mrseq r0, msp                \n"
        "mrsne r0, psp                \n"
        "b     flash_guard_busfault_c \n");
}
