#include "ota_image.h"
#include "crc32_sw.h"

const ota_slot_desc_t g_ota_slots[OTA_SLOT_COUNT] = {
		[OTA_SLOT_A] = { OTA_SLOTA_BASE, OTA_SLOTA_SECTOR, 'A' },
		[OTA_SLOT_B] = { OTA_SLOTB_BASE, OTA_SLOTB_SECTOR, 'B' },
};

extern uint32_t g_pfnVectors;            /* startup_stm32h725xx.s */

ota_slot_t Ota_SlotRunning(void)
{
	const uint32_t vt = (uint32_t)&g_pfnVectors;
	for (ota_slot_t s = OTA_SLOT_A; s < OTA_SLOT_COUNT; s++) {
		if (vt == Ota_CodeBase(s)) {
			return s;
		}
	}

	return OTA_SLOT_NONE;
}

ota_slot_t Ota_SlotFromTag(char tag)
{
	for (ota_slot_t s = OTA_SLOT_A; s < OTA_SLOT_COUNT; s++) {
		if (g_ota_slots[s].tag == tag) {
			return s;
		}
	}

	return OTA_SLOT_NONE;
}

bool Ota_ImageValid(ota_slot_t s)
{
	if (!Ota_SlotValid(s))	return false;
	const ota_image_header_t *h = Ota_Header(s);
	if (h->magic != OTA_IMG_MAGIC || h->size == 0u || h->size > OTA_IMG_MAX_SIZE)	return false;
	return Crc32_Compute((const uint8_t *)Ota_CodeBase(s), h->size) == h->crc32;
}
