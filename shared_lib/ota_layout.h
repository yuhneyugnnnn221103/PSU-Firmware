#ifndef OTA_LAYOUT_H_
#define OTA_LAYOUT_H_

#include <stdint.h>
#include <stdbool.h>

#define OTA_FLASH_BASE		0x08000000u
#define OTA_SECTOR_SIZE		0x00020000u
#define OTA_SECTOR_COUNT	8u
#define OTA_SECTOR_ADDR(n)	(OTA_FLASH_BASE + (uint32_t)(n) * OTA_SECTOR_SIZE)

#define OTA_BOOT_SECTOR		0u
#define OTA_SLOTA_SECTOR	1u
#define OTA_SLOTB_SECTOR	5u
#define OTA_SLOT_SECTORS	3u
#define OTA_CFG_SECTOR		7u

#define OTA_BOOTLOADER_BASE		OTA_SECTOR_ADDR(OTA_BOOT_SECTOR)
#define OTA_SLOTA_BASE			OTA_SECTOR_ADDR(OTA_SLOTA_SECTOR)
#define OTA_SLOTB_BASE			OTA_SECTOR_ADDR(OTA_SLOTB_SECTOR)
#define OTA_SLOT_SIZE			OTA_SLOT_SECTORS * OTA_SECTOR_SIZE	// 3 * 128KB = 384KB
#define OTA_CFG_BASE			OTA_SECTOR_ADDR(OTA_CFG_SECTOR)
#define OTA_CFG_SIZE			OTA_SECTOR_SIZE

typedef enum {
	OTA_SLOT_A = 0,
	OTA_SLOT_B,
	OTA_SLOT_COUNT,
	OTA_SLOT_NONE = 0xFF
} ota_slot_t;

typedef struct {
	uint32_t base;
	uint8_t sector_first;
	char tag;
} ota_slot_desc_t;

extern const ota_slot_desc_t g_ota_slots[OTA_SLOT_COUNT];

static inline bool Ota_SlotValid(ota_slot_t s)
{
	return (unsigned)s < OTA_SLOT_COUNT;
}

static inline ota_slot_t Ota_Other(ota_slot_t s)
{
	return (s == OTA_SLOT_A) ? OTA_SLOT_B : OTA_SLOT_A;
}

ota_slot_t Ota_SlotRunning(void);
ota_slot_t Ota_SlotFromTag(char tag);

#endif /* OTA_LAYOUT_H_ */
