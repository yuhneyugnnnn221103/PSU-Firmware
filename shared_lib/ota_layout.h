#ifndef OTA_LAYOUT_H_
#define OTA_LAYOUT_H_

#include <stdint.h>
#include <stdbool.h>

#define OTA_FLASH_BASE		0x08000000u
#define OTA_SECTOR_SIZE		0x00020000u
#define OTA_SECTOR_COUNT	8u
#define OTA_SECTOR_ADDR(n)	(OTA_FLASH_BASE + (uint32_t)(n) * OTA_SECTOR_SIZE)

/* Flash 1 MB = 8 sector x 128 KB (1 bank). Kien truc: bootloader/recovery + MOT app.
 *   0      : bootloader + recovery
 *   1..5   : app (640 KB): header 1 KB + code
 *   6, 7   : cfg bank 0 / bank 1 (luan phien khi compaction)
 */
#define OTA_BOOT_SECTOR		0u
#define OTA_APP_SECTOR		1u
#define OTA_APP_SECTORS		5u
#define OTA_CFG_SECTOR0		6u
#define OTA_CFG_SECTOR1		7u
#define OTA_CFG_BANKS		2u

#define OTA_BOOTLOADER_BASE		OTA_SECTOR_ADDR(OTA_BOOT_SECTOR)
#define OTA_APP_BASE			OTA_SECTOR_ADDR(OTA_APP_SECTOR)
#define OTA_SLOT_SECTORS		OTA_APP_SECTORS
#define OTA_SLOT_SIZE			((uint32_t)OTA_APP_SECTORS * OTA_SECTOR_SIZE)	// 5 * 128KB = 640KB
#define OTA_CFG_SIZE			OTA_SECTOR_SIZE		/* kich thuoc MOT bank */
#define OTA_CFG_SECTOR(b)		((b) == 0u ? OTA_CFG_SECTOR0 : OTA_CFG_SECTOR1)
#define OTA_CFG_BANK_BASE(b)	OTA_SECTOR_ADDR(OTA_CFG_SECTOR(b))

_Static_assert(OTA_APP_SECTOR + OTA_APP_SECTORS <= OTA_CFG_SECTOR0, "app de len vung cfg");
_Static_assert(OTA_CFG_SECTOR1 < OTA_SECTOR_COUNT, "cfg vuot flash");

/* Chi con MOT "slot" (app). Giu kieu ota_slot_t de cfg_store / header dung lai mot cach co he thong. */
typedef enum {
	OTA_SLOT_APP = 0,
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

ota_slot_t Ota_SlotRunning(void);
ota_slot_t Ota_SlotFromTag(char tag);

#endif /* OTA_LAYOUT_H_ */
