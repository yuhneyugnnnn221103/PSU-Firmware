#ifndef PSU_FRAME_H_
#define PSU_FRAME_H_

#include <stdint.h>
#include <stdbool.h>

/* Khung PSU (xem docs/PSU_PROTOCOL.md). Dung chung app + bootloader.
 *   AB CD | CMD (2 byte, BE) | ADDR | payload | CRC16 (2 byte, BE) | E1 E2
 * Moi so nhieu byte la big-endian. */

#define PSU_HDR1        0xABu
#define PSU_HDR2        0xCDu
#define PSU_TAIL1       0xE1u
#define PSU_TAIL2       0xE2u

#define PSU_OFF_CMD     2u
#define PSU_OFF_ADDR    4u
#define PSU_OFF_PAYLOAD 5u

/* CRC16-CCITT-FALSE (0x1021, init 0xFFFF).
 * 1 = tinh tu byte CMD (khop psu_messages.md va ACTL Tool hien tai).
 * 0 = tinh tu byte ADDR (bo qua CMD). Doi o DUY NHAT cho nay (cung phai doi ben PC). */
#ifndef PSU_CRC_INCLUDE_CMD
#define PSU_CRC_INCLUDE_CMD  1
#endif
#define PSU_CRC_START   (PSU_CRC_INCLUDE_CMD ? PSU_OFF_CMD : PSU_OFF_ADDR)

/* Dia chi PSU (tam thoi co dinh trong code, PSU_ADDR co the dat bang -D luc build). */
#ifndef PSU_ADDR
#define PSU_ADDR        1u
#endif

/* ---- CMD (16 bit) ---- */
#define PSU_CMD_CONTROL      0x0101u   /* PC -> PSU : bat/tat cum, clear trip   */
#define PSU_CMD_MON_REQ      0x0202u   /* PC -> PSU : hoi giam sat               */
#define PSU_CMD_CFG_QUERY    0x0303u   /* PC -> PSU : hoi cau hinh               */
#define PSU_CMD_CFG_WRITE    0x0404u   /* PC -> PSU : ghi cau hinh (khong ACK)   */
#define PSU_CMD_MONITOR      0x8181u   /* PSU -> PC : giam sat                   */
#define PSU_CMD_CFG_REPLY    0x8282u   /* PSU -> PC : tra loi cau hinh           */
#define PSU_CMD_FW_BEGIN     0x9090u
#define PSU_CMD_FW_DATA      0x9191u
#define PSU_CMD_FW_END       0x9292u
#define PSU_CMD_FW_COMMIT    0x9393u
#define PSU_CMD_FW_ACK       0x9494u   /* PSU -> PC */
#define PSU_CMD_FW_INFO      0x9595u
#define PSU_CMD_FW_ERASE     0x9696u   /* xoa slot dich, ACK khi xoa xong        */
#define PSU_CMD_FW_ROLLBACK  0x9797u

/* ---- Do dai khung (byte) ---- */
#define PSU_LEN_CONTROL      13u
#define PSU_LEN_MON_REQ      13u
#define PSU_LEN_CFG_QUERY    13u
#define PSU_LEN_CFG_WRITE    915u
#define PSU_LEN_MONITOR      266u
#define PSU_LEN_CFG_REPLY    915u
#define PSU_LEN_FW_BEGIN     21u
#define PSU_LEN_FW_DATA      269u
#define PSU_LEN_FW_CTRL      9u        /* END / COMMIT / INFO / ERASE / ROLLBACK */
#define PSU_LEN_FW_ACK       17u
#define PSU_LEN_MAX          PSU_LEN_CFG_WRITE

/** Do dai khung PC -> PSU theo CMD; 0 neu khong phai lenh PSU nhan. */
uint16_t Psu_RxFrameLen(uint16_t cmd);

static inline uint16_t Psu_Cmd(const uint8_t *f)  { return (uint16_t)(((uint16_t)f[2] << 8) | f[3]); }
static inline uint8_t  Psu_Addr(const uint8_t *f) { return f[PSU_OFF_ADDR]; }

/** CRC16 cua khung n byte theo PSU_CRC_START (den ngay truoc truong CRC). */
uint16_t Psu_Crc16(const uint8_t *f, uint16_t n);

/** Dung header/tail/CRC? (khong kiem dia chi) */
bool Psu_FrameValid(const uint8_t *f, uint16_t n);

/** Khung gui di: xoa 0, ghi header, CMD, ADDR (= PSU_ADDR), tail. Payload dien sau, roi Psu_FrameSeal. */
void Psu_FrameBegin(uint8_t *f, uint16_t n, uint16_t cmd);
void Psu_FrameSeal(uint8_t *f, uint16_t n);

/* ---- Bo tach khung theo tung byte (moi cong 1 instance) ---- */
typedef struct {
    uint8_t  *buf;
    uint16_t  cap;     /* khung dai hon cap bi bo */
    uint16_t  idx;
    uint16_t  len;
} psu_rx_t;

void Psu_RxInit(psu_rx_t *r, uint8_t *buf, uint16_t cap);
void Psu_RxReset(psu_rx_t *r);

/** Nap 1 byte. Tra ve do dai khung khi da du khung co header + tail dung
 *  (CRC va dia chi CHUA kiem - goi Psu_FrameValid / Psu_Addr), nguoc lai 0. */
uint16_t Psu_RxFeed(psu_rx_t *r, uint8_t c);

#endif /* PSU_FRAME_H_ */
