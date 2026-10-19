#ifndef CFG_RECORD_H_
#define CFG_RECORD_H_

#include "ota_layout.h"

#define CFG_REC_MAGIC       0x43464732u
#define CFG_REC_SIZE        64u
#define CFG_REC_COUNT       (OTA_CFG_SIZE / CFG_REC_SIZE)   /* 2048 ban ghi / bank */
#define CFG_PAYLOAD_MAX     48u

/* type 1 (LIMITS cu) da bo; ban ghi cu bi bo qua */
#define CFG_REC_TYPE_BOOT       2u
#define CFG_REC_TYPE_INSTALL    3u
#define CFG_REC_TYPE_COMMIT     4u   /* danh dau bank hop le, ghi CUOI snapshot */
#define CFG_REC_TYPE_BLOB_CHUNK 5u   /* 1 manh cua blob cau hinh */
#define CFG_REC_TYPE_BLOB_COMMIT 6u  /* ghi SAU CUNG cua mot lan ghi blob: chot do dai + CRC32 */

/* Blob (vd cau hinh PSU 904 byte) luu thanh nhieu manh. Doc chi chap nhan blob
 * khi du manh + CRC32 dung, nen mat dien giua chung khong lam hong ban cu. */
#define CFG_BLOB_MAX            1024u
#define CFG_BLOB_CHUNK_DATA     44u
#define CFG_BLOB_MAX_CHUNKS     ((CFG_BLOB_MAX + CFG_BLOB_CHUNK_DATA - 1u) / CFG_BLOB_CHUNK_DATA)   /* 24 */
#define CFG_BLOB_PSU            0u   /* id blob cau hinh PSU */

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
    uint32_t boot_count;
    uint8_t  slot;
    uint8_t  confirmed;
} cfg_boot_payload_t;

#endif /* CFG_RECORD_H_ */
