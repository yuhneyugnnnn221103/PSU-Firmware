#ifndef OTA_IMAGE_H_
#define OTA_IMAGE_H_

#include "ota_layout.h"

#define OTA_IMG_MAGIC		0x50494D47u
#define OTA_HEADER_SIZE		1024u
#define OTA_IMG_MAX_SIZE	(OTA_SLOT_SIZE - OTA_HEADER_SIZE)

#define OTA_VERSION_STR_LEN	32u

typedef struct {
	uint32_t magic;
	uint32_t version;       /* so (khong dung nua, PC gui 0) */
	uint32_t size;
	uint32_t crc32;
	uint32_t install_seq;
	char     version_str[OTA_VERSION_STR_LEN];   /* chuoi version do PC gui (SET_VERSION), dem 00 */
	uint8_t  reserved[OTA_HEADER_SIZE - 5u * sizeof(uint32_t) - OTA_VERSION_STR_LEN];
} ota_image_header_t;

_Static_assert(sizeof(ota_image_header_t) == OTA_HEADER_SIZE, "header phai dai dung 1 KB");

static inline const ota_image_header_t *Ota_Header(ota_slot_t s)
{
	return (const ota_image_header_t *)g_ota_slots[s].base;
}

/* Doc header qua FlashGuard (an toan voi flash hong ECC). false neu loi doc. */
bool Ota_ReadHeader(ota_slot_t s, ota_image_header_t *out);

static inline uint32_t Ota_CodeBase(ota_slot_t s)
{
	return g_ota_slots[s].base + OTA_HEADER_SIZE;
}

bool Ota_ImageValid(ota_slot_t s);

/** Doc chuoi version trong header (qua FlashGuard). out[OTA_VERSION_STR_LEN] luon duoc ghi (rong neu khong co
 *  header/chua dat). @return true neu header co magic hop le. */
bool Ota_ReadVersionStr(ota_slot_t s, char out[OTA_VERSION_STR_LEN]);

/** Chep src (32 byte) sang dst: giu cac ky tu in duoc 0x20..0x7E dau tien, phan con lai = 0. */
void Ota_VersionSanitize(char dst[OTA_VERSION_STR_LEN], const uint8_t src[OTA_VERSION_STR_LEN]);

/* Kiem 2 word dau vector table (MSP trong RAM, entry Thumb nam trong slot) - doc qua FlashGuard */
bool Ota_VectorOk(ota_slot_t s);

#endif /* OTA_IMAGE_H_ */
