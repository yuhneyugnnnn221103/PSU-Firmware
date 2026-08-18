/**
 ******************************************************************************
 * @file    telemetry.h
 * @brief   Dong khung byte tu du lieu ina228_driver va phat ra
 *          USART1 (RS422 #1), UART5 (RS422 #2), UART4 (FT232).
 *
 * PHAN CONG TRACH NHIEM:
 *   ina228_driver : so huu bus I2C1, state machine DMA, parse thanh ghi.
 *   telemetry     : KHONG cham I2C. Chi doc ina228_dev_t.raw / .accum,
 *                   ma hoa nguoc ve dang day, tinh CRC, phat qua 3 UART.
 *
 * Moi dinh nghia thanh ghi / dia chi / LSB nam trong ina228_cfg.h.
 * File nay chi dinh nghia bo cuc KHUNG.
 *
 * CHU KY: goi TLM_OnTick() moi INA228_SCAN_PERIOD_MS (60 ms, ~16.7 Hz).
 *         Luon quet full de POWER/DIETEMP tuoi bang CURRENT/VBUS.
 ******************************************************************************
 */

#ifndef COMM_H_
#define COMM_H_

#include <stdint.h>
#include <stdbool.h>
#include "ina228_driver.h"

/* ==========================================================================
 * BO CUC KHUNG TX  (INA228_CH_COUNT kenh,  byte moi kenh)
 * ========================================================================== */

#define TLM_HDR1            0xABu
#define TLM_HDR2            0xCDu
#define TLM_TAIL1           0xE1u
#define TLM_TAIL2           0xE2u

#define TLM_CMD_TELEMETRY   0x01u

#define TLM_CH_BLOCK_SZ     10u    /* CURRENT4 VBUS4 TEMP2 */
#define TLM_PAYLOAD_OFF     4u     /* sau HDR1 HDR2 CMD1 CMD2 */

#define TLM_CRC_OFF         (TLM_PAYLOAD_OFF + INA228_CH_COUNT * TLM_CH_BLOCK_SZ)
#define TLM_FRAME_SZ        (TLM_CRC_OFF + 4u)     /* + CRC16 + 2 byte tailer */

#define TLM_CRC_START       2u                     /* CRC phu byte 2 .. CRC_OFF-1 */
#define TLM_CRC_LEN         (TLM_CRC_OFF - TLM_CRC_START)

/* Offset trong mot khoi kenh
 * Các giá trị thanh ghi 20 hoặc 24 bit tốn 3 byte,
 * nhưng để lưu trữ và xử lý cần dùng kiểu dữ liệu 32 bit tương đương 4 byte,
 * nên truyền 4 byte sẽ đơn giản hơn (đã bao gồm sign extend và bit shift)
 *  */
#define TLM_OFF_CURRENT     0u
#define TLM_OFF_VBUS        4u
#define TLM_OFF_DIETEMP     8u
//#define TLM_OFF_POWER       10u
//#define TLM_OFF_VSHUNT		14u

/* Byte trang thai (offset 3)
 *   bit 0..3 : kenh 0..3 con fresh  (ina228_is_fresh)
 *   bit 4..7 : DCM 0..3 bao loi FT (PC4 PC5 PB0 PB1, dao cuc: 1 = CO LOI) */
#define TLM_ST_FRESH_Msk       0x0Fu
#define TLM_ST_DCM_FAULT_Pos   4u

/* ==========================================================================
 * KHUNG DIEU KHIEN (PC -> STM32), 8 byte co dinh
 *   0    HDR1 = 0xAB
 *   1    HDR2 = 0xCD
 *   2    CMD  = 0x88
 *   3    PWR_EN, 0 = disable, 1 = enable
 *   4..5 CRC16-CCITT-FALSE tren byte 2..3 (hien tai = 0x0000)
 *   6    TAIL1 = 0xE1
 *   7    TAIL2 = 0xE2
 * ========================================================================== */

#define TLM_CMD_PWR_CTRL     0x88u
#define TLM_RX_FRAME_SZ      8u

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

#endif /* COMM_H_ */
