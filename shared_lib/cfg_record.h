#ifndef CFG_RECORD_H_
#define CFG_RECORD_H_

#include "ota_layout.h"

#define CFG_REC_MAGIC       0x43464732u
#define CFG_REC_SIZE        64u
#define CFG_REC_COUNT       (OTA_CFG_SIZE / CFG_REC_SIZE)   /* 2048 ban ghi */
#define CFG_PAYLOAD_MAX     48u

#define CFG_REC_TYPE_LIMITS     1u
#define CFG_REC_TYPE_BOOT       2u
#define CFG_REC_TYPE_INSTALL    3u

#define CFG_BOOT_COUNT_FORCE_FAIL   0xFFFFFFFFu

/* CRC32 o 4 byte CUOI (flash-word thu 2). Mat dien giua 2 flash-word
 * -> crc32 van la 0xFFFFFFFF -> ban ghi bi loai, khong nap payload do. */
typedef struct {
    uint32_t magic;                     /*  0 */
    uint32_t seq;                       /*  4 */
    uint8_t  type;                      /*  8 */
    uint8_t  len;                       /*  9 : so byte payload that */
    uint8_t  rsv[2];                    /* 10 : = 0 */
    uint8_t  payload[CFG_PAYLOAD_MAX];  /* 12 */
    uint32_t crc32;                     /* 60 : CRC32 cua byte 0..59 */
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
