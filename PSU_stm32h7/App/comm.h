#ifndef COMM_H_
#define COMM_H_

#include <ina228_driver.h>
#include <stdint.h>
#include <stdbool.h>

/* ==========================================================================
 * BO CUC KHUNG TX  (INA228_CH_COUNT kenh,  byte moi kenh)
 * ========================================================================== */
#define TLM_HDR1            0xABu
#define TLM_HDR2            0xCDu
#define TLM_TAIL1           0xE1u
#define TLM_TAIL2           0xE2u

/* --- CMD --- */
#define TLM_CMD_TELEMETRY    0x01u

#define TLM_CMD_STATUS       0x03u   /* STM -> PC, dung khung AUX 16 byte */

#define TLM_CMD_PWR_CTRL     0x88u
#define TLM_CMD_SET_LIMIT    0x89u
#define TLM_CMD_LIMIT_ACK    0x8Au
#define TLM_CMD_CLR_FAULT    0x8Bu
#define TLM_CMD_GET_LIMIT    0x8Cu

#define TLM_CMD_FW_BEGIN     0x90u   /* PC -> STM: size + crc32 tong + version */
#define TLM_CMD_FW_DATA      0x91u   /* PC -> STM: 1 chunk anh, co so thu tu   */
#define TLM_CMD_FW_END       0x92u   /* PC -> STM: het du lieu, yeu cau verify */
#define TLM_CMD_FW_COMMIT    0x93u   /* PC -> STM: xac nhan kich hoat, moi reset */
#define TLM_CMD_FW_ACK       0x94u   /* STM -> PC: phan hoi cho ca 4 lenh tren */
#define TLM_CMD_FW_INFO      0x95u   /* PC -> STM: hoi slot + version dang chay */

#define TLM_CMD_FW_ROLLBACK  0x96u

#define TLM_ADDR_BROADCAST   0xFFu   /* danh cho multidrop RS485 sau nay      */
#define TLM_BOARD_ADDR       0x01u   /* hardcode qua macro, chua doc tu GPIO  */

/* --- Kich thuoc khung --- */
#define TLM_CH_BLOCK_SZ      12u
#define TLM_PAYLOAD_OFF      4u
#define TLM_CRC_OFF          (TLM_PAYLOAD_OFF + INA228_CH_COUNT * TLM_CH_BLOCK_SZ)
#define TLM_FRAME_SZ         (TLM_CRC_OFF + 4u)      /* 56 */

#define TLM_RX_FRAME_SZ      8u      /* 0x88 / 0x8B / 0x8C */
#define TLM_CFG_FRAME_SZ     20u     /* 0x89               */
#define TLM_AUX_FRAME_SZ     16u     /* 0x02 / 0x8A        */

/* ==========================================================================
 * KHUNG FW_* (cap nhat firmware) - CO ADDR, KHONG dung chung offset voi
 * cac khung o tren. Bo cuc: AB CD | CMD | ADDR | payload | CRC16 | E1 E2
 * ========================================================================== */
#define TLM_FW_OFF_ADDR       3u    /* giong nhau cho ca 5 loai khung FW_*   */
#define TLM_FW_PAYLOAD_OFF    4u

/* FW_BEGIN (0x90): total_size(u32) + crc32_total(u32) + version(u32) */
#define TLM_FW_BEGIN_OFF_SIZE     4u
#define TLM_FW_BEGIN_OFF_CRC32    8u
#define TLM_FW_BEGIN_OFF_VERSION  12u
#define TLM_FW_BEGIN_SZ           20u

/* FW_DATA (0x91): seq(u16) + chunk_len(u16) + data[TLM_FW_CHUNK_MAX] */
#define TLM_FW_CHUNK_MAX          256u   /* du lieu thuc, chua tinh header/CRC/tail */
#define TLM_FW_DATA_OFF_SEQ       4u
#define TLM_FW_DATA_OFF_LEN       6u
#define TLM_FW_DATA_OFF_PAYLOAD   8u
#define TLM_FW_DATA_FRAME_MAX     (TLM_FW_DATA_OFF_PAYLOAD + TLM_FW_CHUNK_MAX + 4u) /* 268 */

/* FW_END (0x92) / FW_COMMIT (0x93): chi co ADDR, khong payload them */
#define TLM_FW_CTRL_SZ            8u

/* FW_ACK (0x94), STM -> PC: ack_cmd(1) + status(1) + info_u32(4) + reserved(2)
 * Dung du TLM_AUX_FRAME_SZ (16 byte) de tai su dung nguyen ven co che
 * aux_acquire()/aux_seal() da co san (chong ghi de buffer DMA con dang
 * bay) thay vi viet buffer/seal rieng cho khung nay. */
#define TLM_FW_ACK_OFF_CMD        4u
#define TLM_FW_ACK_OFF_STATUS     5u
#define TLM_FW_ACK_OFF_INFO       6u   /* DATA: last_seq da nhan; END: crc tinh duoc;
                                        * INFO: version dang chay              */
#define TLM_FW_ACK_OFF_SLOT       10u  /* CHI dung cho tra loi FW_INFO: 'A'/'B'.
                                        * Byte 10-11 con trong (CRC o 12).      */
#define TLM_FW_ACK_SZ             TLM_AUX_FRAME_SZ   /* = 16 */

#define TLM_FW_ACK_OK               0u
#define TLM_FW_ACK_REFUSED          1u   /* khong o SAFE_OFF                     */
#define TLM_FW_ACK_BAD_SIZE         2u   /* vuot dung luong slot                 */
#define TLM_FW_ACK_BAD_SEQ          3u   /* sai thu tu chunk, gui lai tu info+1   */
#define TLM_FW_ACK_CRC_FAIL         4u   /* CRC32 toan anh sai luc FW_END        */
#define TLM_FW_ACK_BUSY             5u   /* dang co phien OTA khac dang chay     */
#define TLM_FW_ACK_NO_COMMIT_PENDING 6u  /* FW_COMMIT nhung chua FW_END thanh cong */
#define TLM_FW_ACK_ERASE_FAIL       7u   /* xoa sector that bai, info = HAL_FLASH_GetError() */
#define TLM_FW_ACK_WRONG_SLOT       8u   /* anh link cho slot KIA, info = Reset_Handler doc duoc */

/* Buffer RX phai chua duoc khung lon nhat trong TOAN BO protocol - hien tai
 * la FW_DATA (268 byte), lon hon nhieu TLM_CFG_FRAME_SZ (20 byte) truoc day.
 * Dung chung 1 kich thuoc co dinh cho ca 3 cong (huong A da chot: don gian,
 * RAM khong phai diem nghen tren H725 co 564 KB). */
#define TLM_RX_MAX_SZ        TLM_FW_DATA_FRAME_MAX

#define TLM_CRC_START       2u                     /* CRC phu byte 2 .. CRC_OFF-1 */
#define TLM_CRC_LEN         (TLM_CRC_OFF - TLM_CRC_START)

#define TLM_OFF_CURRENT     0u
#define TLM_OFF_VBUS        4u
#define TLM_OFF_DIETEMP     8u
#define TLM_OFF_DIAG         10u

/* Byte trang thai (offset 3)
 *   bit 0..3 : kenh 0..3 con fresh  (ina228_is_fresh)
 *   bit 4..7 : DCM 0..3 bao loi FT (PC4 PC5 PB0 PB1, dao cuc: 1 = CO LOI) */
#define TLM_ST_FRESH_Msk       0x0Fu
#define TLM_ST_DCM_FAULT_Pos   4u

/* --- Offset khung SET_LIMIT --- */
#define TLM_CFG_OFF_CH       3u
#define TLM_CFG_OFF_SOVL     4u
#define TLM_CFG_OFF_BOVL     8u
#define TLM_CFG_OFF_BUVL     12u
//#define TLM_CFG_CH_ALL       0xFFu

/* --- Offset khung ACK --- */
#define TLM_ACK_OFF_CH       3u
#define TLM_ACK_OFF_STATUS   4u
#define TLM_ACK_OFF_SOVL     6u
#define TLM_ACK_OFF_BOVL     8u
#define TLM_ACK_OFF_BUVL     10u

#define TLM_ACK_OK           0u
#define TLM_ACK_I2C_ERR      1u
#define TLM_ACK_PARAM        2u
#define TLM_ACK_NO_DEV       3u
#define TLM_ACK_REFUSED      4u   /* tu choi PWR_CTRL(on) vi dang TRIPPED/LOCKOUT */

/* --- Offset khung STATUS (0x03), dung TLM_AUX_FRAME_SZ = 16 --- */
#define TLM_STS_OFF_STATE    3u   /* bit7..4 = safe_state, bit3..0 = fault_code */
#define TLM_STS_OFF_TRIPMASK 4u   /* bit i = kenh i gop phan gay trip           */
#define TLM_STS_OFF_DCM      5u   /* anh chup s_dcm_fault                       */
#define TLM_STS_OFF_FRESH    6u   /* bit i = kenh i con tuoi                    */
#define TLM_STS_OFF_CFGOK    7u   /* bit i = kenh i cau hinh dung               */
#define TLM_STS_OFF_PWR      8u   /* 1 = PWR_EN dang bat (trang thai LOGIC)     */
#define TLM_STS_OFF_TRIPCNT  9u   /* so lan trip lien tiep                      */

#define TLM_RX_OFF_CMD       2u
#define TLM_RX_OFF_CTRL      3u
#define TLM_RX_OFF_CRC       4u
#define TLM_RX_CRC_START     2u
#define TLM_RX_CRC_LEN       2u    /* phu CMD + MASK */

/* ==========================================================================
 * SO CONG PHAT
 * ========================================================================== */
#define TLM_PORT_RS422_1    0u
#define TLM_PORT_RS422_2    1u
#define TLM_PORT_FT232      2u
#define TLM_PORT_COUNT      3u

#define TLM_TX_TIMEOUT_MS   50u

/* ==========================================================================
 * CHAN DOAN
 * ========================================================================== */
typedef struct {
    uint32_t frames_built;
    uint32_t tx_sent[TLM_PORT_COUNT];
    uint32_t tx_skipped[TLM_PORT_COUNT];   /* cong con ban khi den ky phat */
    uint32_t tx_error[TLM_PORT_COUNT];
    uint32_t tx_stuck[TLM_PORT_COUNT];

    uint32_t rx_bytes[TLM_PORT_COUNT];
    uint32_t rx_frames[TLM_PORT_COUNT];
    uint32_t rx_badcrc[TLM_PORT_COUNT];
    uint32_t rx_badframe[TLM_PORT_COUNT];
    uint32_t rx_overrun[TLM_PORT_COUNT];
    uint32_t rx_uart_err[TLM_PORT_COUNT];

    uint32_t scan_busy;                    /* bus chua ranh khi den ky quet */
    uint32_t ch_stale;                     /* so lan mot kenh bi danh dau cu */
} tlm_stats_t;

/* ==========================================================================
 * API
 * ========================================================================== */

void dcm_poll(void);

/** @brief Goi sau ina228_bus_init() + ina228_init_all(). */
void Comm_Init(void);

/** @brief Goi moi INA228_SCAN_PERIOD_MS (vd tu TIM6). Khoi dong chu ky quet. */
void Comm_OnTick(void);

/** @brief Goi lien tuc trong main loop. Dong khung khi bus bao DONE. */
void Comm_Poll(uint32_t now_ms);

//const tlm_stats_t *TLM_GetStats(void);

void Comm_TxWatchdog(uint32_t now_ms);

/** @brief Bat DMA RX Circular vong tron cho ca 3 cong. Goi trong Comm_Init(). */
void Comm_RxStart(void);

/** @brief Quet buffer RX, tach khung, thuc thi lenh. Goi trong main loop. */
void Comm_RxPoll(void);

/** @brief Ap dung cac yeu cau cau hinh dang cho. Goi trong main loop
 *         khi ina228_bus_state() == INA228_BUS_IDLE. */
void Comm_CfgTask(void);

/** @brief Anh chup co loi FT tu DCM (bit i = kenh i CO LOI). */
uint8_t Comm_DcmFault(void);

/** @brief Con nhan duoc khung hop le tu PC trong 3 s gan nhat khong? */
bool Comm_PcLinkAlive(uint32_t now_ms);

/** @brief Dong va phat khung STATUS 0x03. Goi moi vong main loop;
 *         tu gian nhip 1 s va phat ngay khi trang thai doi. */
void Comm_StatusTask(uint32_t now_ms);

/** @brief Phat FW_ACK KHONG DOI LENH khi FwUpdate_State() chuyen
 *         ERASING -> RECEIVING - dong PC khoi phai doan xem xoa sector
 *         xong chua truoc khi bat dau gui FW_DATA. Goi moi vong main loop. */
void Comm_FwPoll(void);

/* ==========================================================================
 * KHOA KICH THUOC KHUNG - phat hien drift luc bien dich thay vi luc chay
 * ========================================================================== */
//_Static_assert(TLM_FRAME_SZ     == 56u, "TLM frame size drift");
//_Static_assert(TLM_CFG_FRAME_SZ == 20u, "SET_LIMIT frame size drift");
//_Static_assert(TLM_AUX_FRAME_SZ == 16u, "AUX frame size drift");
//_Static_assert(TLM_RX_MAX_SZ >= TLM_CFG_FRAME_SZ, "rx buffer too small");
//_Static_assert(TLM_STS_OFF_TRIPCNT < (TLM_AUX_FRAME_SZ - 4u), "STATUS overflow");

#endif /* COMM_H_ */
