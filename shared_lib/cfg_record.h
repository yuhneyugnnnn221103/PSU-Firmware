#ifndef CFG_RECORD_H_
#define CFG_RECORD_H_

#include "ota_layout.h"

#define CFG_REC_MAGIC	0x43464731u
#define CFG_REC_SIZE	64u									// boi so 32 (1 flash-word)
#define CFG_REC_COUNT	(OTA_CFG_SIZE / CFG_REC_SIZE)		// 2048 ban ghi

#define CFG_REC_TYPE_LIMITS		1u
#define CFG_REC_TYPE_BOOT		2u
#define CFG_REC_TYPE_INSTALL	3u

#define CFG_BOOT_COUNT_FORCE_FAIL   0xFFFFFFFFu

typedef struct {
    uint32_t magic;
    uint32_t seq;
    uint16_t crc16;
    uint8_t  type;
    uint8_t  payload[27];
} cfg_record_t;

typedef struct {
    uint16_t sovl[4];
    uint16_t bovl[4];
    uint16_t buvl[4];
} cfg_limits_payload_t;

typedef struct {
    uint32_t boot_count;
    uint8_t  slot;
    uint8_t  confirmed;
} cfg_boot_payload_t;

#endif /* CFG_RECORD_H_ */
