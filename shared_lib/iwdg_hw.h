#ifndef IWDG_HW_H_
#define IWDG_HW_H_

#include "stm32h7xx_hal.h"

#ifndef WDG_TIMEOUT_MS
#define WDG_TIMEOUT_MS	8000u
#endif

#define IWDG_TICK_MS	8u		// LSI 32kHz / 256

static inline void IWDG_Refresh(void)
{
	IWDG1->KR = 0xAAAAu;
}

static inline void IWDG_Start(uint32_t timeout_ms)
{
	__HAL_DBGMCU_FREEZE_IWDG1();
	IWDG1->KR 	= 0xCCCCu;			// Start, tu bat LSI
	IWDG1->KR 	= 0x5555u;			// Mo khoa ghi cau hinh Prescaler va Reload
	IWDG1->PR 	= 6u;				// Prescaler = 256
	IWDG1->RLR 	= timeout_ms / IWDG_TICK_MS;	// Reload

	while (IWDG1->SR != 0) { }		// Cho ready
	IWDG1->KR	= 0xAAAAu;
}

#endif /* IWDG_HW_H_ */
