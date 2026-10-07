#ifndef OTA_PROTO_H_
#define OTA_PROTO_H_

/* Khung FW_* dung chung giua App (comm.h) va Bootloader recovery.
 * Bo cuc: AB CD | CMD | ADDR | payload | CRC16 (BE) | E1 E2
 * CRC16-CCITT phu tu byte 2 den ngay truoc CRC. Gia tri PHAI khop comm.h
 * (fw_update.c co _Static_assert kiem tra). */

#define OTAP_HDR1             0xABu
#define OTAP_HDR2             0xCDu
#define OTAP_TAIL1            0xE1u
#define OTAP_TAIL2            0xE2u
#define OTAP_BOARD_ADDR       0x01u

#define OTAP_CMD_FW_BEGIN     0x90u
#define OTAP_CMD_FW_DATA      0x91u
#define OTAP_CMD_FW_END       0x92u
#define OTAP_CMD_FW_COMMIT    0x93u
#define OTAP_CMD_FW_ACK       0x94u
#define OTAP_CMD_FW_INFO      0x95u

#define OTAP_CHUNK_MAX        256u
#define OTAP_BEGIN_SZ         20u
#define OTAP_DATA_FRAME_SZ    268u   /* co dinh: chunk cuoi phai duoc PC dem */
#define OTAP_CTRL_SZ          8u
#define OTAP_ACK_SZ           16u
#define OTAP_RX_MAX           OTAP_DATA_FRAME_SZ

#define OTAP_BEGIN_OFF_SIZE     4u
#define OTAP_BEGIN_OFF_CRC32    8u
#define OTAP_BEGIN_OFF_VERSION  12u
#define OTAP_DATA_OFF_SEQ       4u
#define OTAP_DATA_OFF_LEN       6u
#define OTAP_DATA_OFF_PAYLOAD   8u

#define OTAP_ACK_OFF_CMD      4u
#define OTAP_ACK_OFF_STATUS   5u
#define OTAP_ACK_OFF_INFO     6u
#define OTAP_ACK_OFF_SLOT     10u

#define OTAP_ST_OK            0u
#define OTAP_ST_REFUSED       1u
#define OTAP_ST_BAD_SIZE      2u
#define OTAP_ST_BAD_SEQ       3u
#define OTAP_ST_CRC_FAIL      4u
#define OTAP_ST_BUSY          5u
#define OTAP_ST_NO_COMMIT     6u
#define OTAP_ST_ERASE_FAIL    7u
#define OTAP_ST_WRONG_SLOT    8u
#define OTAP_ST_PROG_FAIL     9u

/* Tra loi FW_INFO o che do recovery: byte slot = 'R' (App tra 'A'/'B') */
#define OTAP_SLOT_RECOVERY    'R'

#endif /* OTA_PROTO_H_ */
