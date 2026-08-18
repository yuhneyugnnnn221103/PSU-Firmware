/*
 * buffer_test.h
 *
 *  Created on: Aug 13, 2026
 *      Author: HuyND304
 */

#ifndef INC_BUFFER_TEST_H_
#define INC_BUFFER_TEST_H_

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Cấu trúc khung (phải khớp protocol.h bên Qt)
 *
 *   [0]      0xAB              HEADER1
 *   [1]      0xCD              HEADER2
 *   [2]      0x01              CMD_MONITOR
 *   [3]      status            bit0..3 = fresh INA#1..4
 *                              bit4..7 = fault DCM#1..4
 *   [4..139] 4 block x 34 byte
 *   [140,141] CRC16-CCITT big-endian, tính trên [2..139]
 *   [142]    0xE1              TAILER1
 *   [143]    0xE2              TAILER2
 *
 * Một block INA228 (34 byte, big-endian, đã sign-extend sẵn):
 *   +0   int32   CURRENT
 *   +4   uint32  VBUS
 *   +8   int16   DIETEMP
 *   +10  uint32  POWER
 *   +14  int32   VSHUNT
 *   +18  uint64  ENERGY
 *   +26  int64   CHARGE
 * ------------------------------------------------------------------------- */

#define PMON_FRAME_SIZE     144
#define PMON_IC_COUNT       4
#define PMON_IC_SIZE        34

#define PMON_HEADER1        0xABu
#define PMON_HEADER2        0xCDu
#define PMON_TAILER1        0xE1u
#define PMON_TAILER2        0xE2u
#define PMON_CMD_MONITOR    0x01u

#define PMON_CRC_OFFSET     140
#define PMON_CRC_START      2
#define PMON_CRC_LENGTH     (PMON_CRC_OFFSET - PMON_CRC_START)   /* 138 */

/* 0 = gửi CRC = 0 (PC bỏ qua nhờ ACCEPT_ZERO_CRC)
 * 1 = gửi CRC16-CCITT thật (dùng để kiểm tra luôn phần CRC bên PC) */
#define PMON_TEST_USE_REAL_CRC   1

/* Chu kỳ gửi khung, đơn vị ms. Dùng để tích phân energy/charge cho đúng. */
#define PMON_TEST_PERIOD_MS      60

/**
  * @brief  Khởi tạo bộ sinh dữ liệu (đặt lại accumulator).
  */
void pmon_test_init(void);

/**
  * @brief  Sinh một khung telemetry hoàn chỉnh.
  * @param  buf  bộ đệm ít nhất PMON_FRAME_SIZE byte
  * @note   Mỗi lần gọi tương ứng một chu kỳ PMON_TEST_PERIOD_MS.
  */
void pmon_test_build_frame(uint8_t *buf);

/**
  * @brief  CRC16-CCITT (init 0xFFFF, poly 0x1021, không reflect).
  */
uint16_t pmon_crc16_ccitt(const uint8_t *data, uint32_t len);

#endif /* INC_BUFFER_TEST_H_ */
