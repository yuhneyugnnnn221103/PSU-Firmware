#ifndef COMM_H_
#define COMM_H_

#include <stdint.h>
#include <stdbool.h>
#include "psu_frame.h"

/* ==========================================================================
 * TRUYEN THONG UART - khung va lenh: shared_lib/psu_frame.h, docs/PSU_PROTOCOL.md
 *
 * Firmware KHONG tu phat: chi tra loi khi nhan lenh hop le (dung CRC, dung dia chi PSU_ADDR).
 *   02 02 hoi giam sat -> 81 81         03 03 hoi cau hinh -> 82 82
 *   01 01 dieu khien, 04 04 ghi cau hinh: khong ACK         90..97: nap code, co ACK 94 94
 * 3 cong: tra loi tren CHINH cong da nhan lenh (cong con lai la du phong).
 * ========================================================================== */

#define COMM_PORT_RS422_1   0u      /* UART5,  DE = PB2, RE = PB10 */
#define COMM_PORT_RS422_2   1u      /* USART1, DE = PC6, RE = PC9  */
#define COMM_PORT_FT232     2u      /* UART4,  CTS# = PA2, RTS# = PA3 */
#define COMM_PORT_COUNT     3u

#define COMM_BAUD           115200u /* phai khop MX_*_UART_Init */

/** Khoi tao cong, bat DMA RX vong. Goi sau ina228_init_all(). */
void Comm_Init(void);

/** Goi lien tuc trong main loop: tach khung tu buffer DMA, kiem CRC + dia chi, thuc thi lenh. */
void Comm_RxPoll(void);

/** Goi dinh ky (~10 ms): huy lan phat bi ket qua han. */
void Comm_TxWatchdog(uint32_t now_ms);

/** Goi moi vong lap: gui ACK tre (xoa xong / loi xoa) cho OTA. */
void Comm_FwPoll(void);

/** Doc 4 chan FT cua DCM (goi moi 10 ms), bo phieu 2/3. */
void dcm_poll(void);

/** Anh chup loi FT tu DCM (bit i = cum i CO LOI). */
uint8_t Comm_DcmFault(void);

/** Con nhan duoc khung hop le tu PC trong 3 s gan nhat khong? */
bool Comm_PcLinkAlive(uint32_t now_ms);

#endif /* COMM_H_ */
