#ifndef OTA_LAYOUT_H_
#define OTA_LAYOUT_H_

#include <stdint.h>
#include <stdbool.h>

#define OTA_FLASH_BASE		0x08000000u
#define OTA_SECTOR_SIZE		0x00020000u
#define OTA_SECTOR_COUNT	8u
#define OTA_SECTOR_ADDR(n)	(OTA_FLASH_BASE + (uint32_t)(n) * OTA_SECTOR_SIZE)

/* Flash 1 MB = 8 sector x 128 KB (1 bank)
 *   0      : bootloader
 *   1..2   : slot A (256 KB)
 *   3..4   : slot B (256 KB)
 *   5      : du phong (chua dung)
 *   6, 7   : cfg bank 0 / bank 1 (luan phien khi compaction)
 */
#define OTA_BOOT_SECTOR		0u
#define OTA_SLOTA_SECTOR	1u
#define OTA_SLOTB_SECTOR	3u
#define OTA_SLOT_SECTORS	2u
#define OTA_SPARE_SECTOR	5u
#define OTA_CFG_SECTOR0		6u
#define OTA_CFG_SECTOR1		7u
#define OTA_CFG_BANKS		2u

#define OTA_BOOTLOADER_BASE		OTA_SECTOR_ADDR(OTA_BOOT_SECTOR)
#define OTA_SLOTA_BASE			OTA_SECTOR_ADDR(OTA_SLOTA_SECTOR)
#define OTA_SLOTB_BASE			OTA_SECTOR_ADDR(OTA_SLOTB_SECTOR)
#define OTA_SLOT_SIZE			((uint32_t)OTA_SLOT_SECTORS * OTA_SECTOR_SIZE)	// 2 * 128KB = 256KB
#define OTA_CFG_SIZE			OTA_SECTOR_SIZE		/* kich thuoc MOT bank */
#define OTA_CFG_SECTOR(b)		((b) == 0u ? OTA_CFG_SECTOR0 : OTA_CFG_SECTOR1)
#define OTA_CFG_BANK_BASE(b)	OTA_SECTOR_ADDR(OTA_CFG_SECTOR(b))

_Static_assert(OTA_SLOTA_SECTOR + OTA_SLOT_SECTORS <= OTA_SLOTB_SECTOR, "slot A de len slot B");
_Static_assert(OTA_SLOTB_SECTOR + OTA_SLOT_SECTORS <= OTA_SPARE_SECTOR, "slot B de len vung du phong");
_Static_assert(OTA_CFG_SECTOR1 < OTA_SECTOR_COUNT, "cfg vuot flash");

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
