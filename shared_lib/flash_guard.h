#ifndef FLASH_GUARD_H_
#define FLASH_GUARD_H_

#include <stdint.h>
#include <stdbool.h>

void FlashGuard_Init(void);

void FlashGuard_Begin(uint32_t lo, uint32_t hi);

bool FlashGuard_End(void);

void FlashGuard_CopyWords(void *dst, uint32_t src, uint32_t len);

/** Goi truoc khi treo cho IWDG o cac fault khong phuc hoi (weak, mac dinh rong). */
void Fault_Hook(void);

extern volatile uint32_t g_flash_ecc_fault_count;

#endif /* FLASH_GUARD_H_ */
