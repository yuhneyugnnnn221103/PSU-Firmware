#ifndef BOOT_RECOVERY_H_
#define BOOT_RECOVERY_H_

#include <stdint.h>
#include <stdbool.h>

/* So cong nhan lenh (khop thu tu comm.c cua app): 0 = RS422 #1 (UART5),
 * 1 = RS422 #2 (USART1), 2 = FT232 (UART4). */
#define REC_PORTS   3u

/** Che do cuu ho: khi KHONG co anh hop le o ca 2 slot. Nhan firmware qua
 *  UART bang cung giao thuc FW_* voi App (INFO/BEGIN/DATA/END/COMMIT),
 *  ghi vao slot, reset khi COMMIT. KHONG BAO GIO tro ve. */
void Boot_RecoveryRun(void);

/* --- Phan giao thuc (khong phu thuoc phan cung, test duoc tren host) --- */
void Boot_RecoveryFeed(uint8_t port, uint8_t byte);
void Boot_RecoveryResync(uint8_t port);     /* bo khung do khi im lang lau */

/* --- Phan cung (boot_recovery_hw.c; test host thay bang ban gia) --- */
void RecHw_Init(void);
bool RecHw_RxByte(uint8_t port, uint8_t *byte);
void RecHw_Tx(uint8_t port, const uint8_t *buf, uint16_t len);
void RecHw_ResetSystem(void);               /* cho TX xong roi NVIC_SystemReset */

#endif /* BOOT_RECOVERY_H_ */
