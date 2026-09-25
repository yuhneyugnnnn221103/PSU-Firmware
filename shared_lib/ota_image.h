#ifndef OTA_IMAGE_H_
#define OTA_IMAGE_H_

#include "ota_layout.h"

#define OTA_IMG_MAGIC		0x50494D47u
#define OTA_HEADER_SIZE		1024u
#define OTA_IMG_MAX_SIZE	(OTA_SLOT_SIZE - OTA_HEADER_SIZE)

typedef struct {
	uint32_t magic;
	uint32_t version;
	uint32_t size;
	uint32_t crc32;
	uint32_t install_seq;
	uint8_t  reserved[OTA_HEADER_SIZE - 5u * sizeof(uint32_t)];
} ota_image_header_t;

static inline const ota_image_header_t *Ota_Header(ota_slot_t s)
{
	return (const ota_image_header_t *)g_ota_slots[s].base;
}

static inline uint32_t Ota_CodeBase(ota_slot_t s)
{
	return g_ota_slots[s].base + OTA_HEADER_SIZE;
}

bool Ota_ImageValid(ota_slot_t s);

#endif /* OTA_IMAGE_H_ */
