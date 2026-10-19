#ifndef OTA_PROTO_H_
#define OTA_PROTO_H_

#include "psu_frame.h"

/* Phan OTA dung chung giua App va Bootloader recovery. Khung (CMD, do dai, CRC)
 * dinh nghia o psu_frame.h. Vi tri byte duoi day tinh tu 0 (AB = byte 0).
 *
 *  BEGIN (21): [5..8] size  [9..12] crc32  [13..16] version
 *  DATA (269): [5..6] seq   [7..8] len     [9..264] data (256 B, chunk cuoi dem 0xFF)
 *  END/COMMIT/INFO/ERASE/ROLLBACK (9): chi co ADDR
 *  ACK  (17) : [5] AckCmd (1 byte)  [6] status  [7..10] info  [11] slot  [12] state */

#define OTAP_CHUNK_MAX          256u

#define OTAP_BEGIN_OFF_SIZE     5u
#define OTAP_BEGIN_OFF_CRC32    9u
#define OTAP_BEGIN_OFF_VERSION  13u
#define OTAP_DATA_OFF_SEQ       5u
#define OTAP_DATA_OFF_LEN       7u
#define OTAP_DATA_OFF_PAYLOAD   9u

#define OTAP_ACK_OFF_CMD        5u
#define OTAP_ACK_OFF_STATUS     6u
#define OTAP_ACK_OFF_INFO       7u
#define OTAP_ACK_OFF_SLOT       11u   /* 'A' / 'B' (app) hoac 'R' (recovery) */
#define OTAP_ACK_OFF_STATE      12u   /* CHI tra loi INFO: bit7 = da confirm, bit6..0 = boot_count (127 = rollback) */
#define OTAP_STATE_CONFIRMED    0x80u
#define OTAP_STATE_COUNT_MAX    0x7Fu

/* AckCmd la byte lap lai o CMD cua lenh duoc tra loi (vd 0x90 cho BEGIN) */
#define OTAP_ACKCMD(cmd16)      ((uint8_t)((cmd16) & 0xFFu))

#define OTAP_ST_OK              0u
#define OTAP_ST_REFUSED         1u
#define OTAP_ST_BAD_SIZE        2u
#define OTAP_ST_BAD_SEQ         3u
#define OTAP_ST_CRC_FAIL        4u
#define OTAP_ST_BUSY            5u
#define OTAP_ST_NO_COMMIT       6u
#define OTAP_ST_ERASE_FAIL      7u
#define OTAP_ST_WRONG_SLOT      8u
#define OTAP_ST_PROG_FAIL       9u
#define OTAP_ST_NO_TARGET       10u   /* ROLLBACK: slot kia khong the khoi dong */
#define OTAP_ST_NOT_ERASED      11u   /* BEGIN khi chua ERASE */

#define OTAP_SLOT_RECOVERY      'R'

#endif /* OTA_PROTO_H_ */
