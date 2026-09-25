#include "main.h"
#include "boot_main.h"
#include "ota_image.h"
#include "cfg_store.h"
#include "iwdg_hw.h"

#define BOOT_MAX_ATTEMPTS		3u

typedef void (*app_entry_t)(void);

static bool slot_exceeded_attempts(ota_slot_t sl)
{
	cfg_boot_payload_t bp;
	if (!CfgStore_ReadBoot(sl, &bp)) {
		return false;
	}

	return !bp.confirmed && (bp.boot_count >= BOOT_MAX_ATTEMPTS);
}

static bool slot_rejected(ota_slot_t sl)
{
	cfg_boot_payload_t bp;
	return CfgStore_ReadBoot(sl, &bp) && (bp.boot_count == CFG_BOOT_COUNT_FORCE_FAIL);
}

static bool newer(ota_slot_t b, ota_slot_t a)
{
	const ota_image_header_t *ha = Ota_Header(a);
	const ota_image_header_t *hb = Ota_Header(b);

	if (hb->version != ha->version) return hb->version > ha->version;
	return hb->install_seq > ha->install_seq;
}

static ota_slot_t pick(const bool cand[OTA_SLOT_COUNT])
{
	ota_slot_t best = OTA_SLOT_NONE;

	for (ota_slot_t sl = OTA_SLOT_A; sl < OTA_SLOT_COUNT; sl++) {
		if (cand[sl] && (best = OTA_SLOT_NONE || newer(sl, best))) {
			best = sl;
		}
	}

	return best;
}

static ota_slot_t choose_slot(void)
{
	bool ok[OTA_SLOT_COUNT], cand[OTA_SLOT_COUNT];

	for (ota_slot_t sl = OTA_SLOT_A; sl < OTA_SLOT_COUNT; sl++) {
		ok[sl] = Ota_SlotValid(sl);
		cand[sl] = ok[sl] && (!slot_exceeded_attempts(sl));

		IWDG_Refresh();
	}

	ota_slot_t best = pick(cand);

//	if (best == OTA_SLOT_NONE) {
//		for (ota_slot_t sl = OTA_SLOT_A; sl < OTA_SLOT_COUNT; sl++) {
//			cand[sl] = ok[sl] && !slot_rejected(sl);
//		}
//		best = pick(cand);
//	}

	return best;
}

static void jump_to_slot(ota_slot_t sl)
{
	const uint32_t base = Ota_CodeBase(sl);
	const uint32_t msp	= *(volatile uint32_t *)(base + 0u);
	const uint32_t entry = *(volatile uint32_t *)(base + 4u);

	for (uint32_t i = 0; i < 8; i++) {
		NVIC->ICER[i] = 0xFFFFFFFFu;
		NVIC->ICPR[i] = 0xFFFFFFFFu;
	}

	SysTick->CTRL = 0u;
	SysTick->LOAD = 0u;
	SysTick->VAL  = 0u;

	HAL_MPU_Disable();
	HAL_RCC_DeInit();
	HAL_DeInit();
	__disable_irq();

	SCB->VTOR = base;
	__set_MSP(msp);
	__set_CONTROL(0u);
	__DSB();
	__ISB();

	((app_entry_t)entry)();

	while (1) {}
}

static void fatal_no_valid_slot(void)
{
	__disable_irq();
	while (1) {}
}

void Boot_Run(void)
{
	CfgStore_Init();

    for (uint8_t i = 0; i < 10u; i++) {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        HAL_Delay(100);
    }

    const ota_slot_t sl = choose_slot();

    if (sl == OTA_SLOT_NONE) fatal_no_valid_slot();

    cfg_boot_payload_t cur;
    const uint32_t prev = CfgStore_ReadBoot(sl, &cur) ? cur.boot_count : 0u;
    const uint32_t next = (prev == CFG_BOOT_COUNT_FORCE_FAIL) ? prev : (prev + 1);
    (void)CfgStore_WriteBoot(sl, next, 0u);

    IWDG_Refresh();
    jump_to_slot(sl);

    while (1) {}
}
