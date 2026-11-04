#ifndef BOOT_RECOVERY_H_
#define BOOT_RECOVERY_H_

#include <stdint.h>
#include <stdbool.h>

/* So cong nhan lenh (khop thu tu comm.c cua app): 0 = RS422 #1 (UART5),
 * 1 = RS422 #2 (USART1), 2 = FT232 (UART4). */
#define REC_PORTS   3u

/** Recovery: nhan firmware qua UART (INFO/ERASE/BEGIN/DATA/END/COMMIT), ghi vung app, reset khi COMMIT.
 *  @param window_ms 0  : o lai vinh vien (app khong khoi dong duoc) - KHONG BAO GIO tro ve.
 *                   >0 : app khoi dong duoc; chi nghe `window_ms` ms sau khi bat nguon. Neu khong co khung hop le
 *                        nao gui toi PSU nay thi tro ve (bootloader nhay vao app); co thi o lai vinh vien (de PC nap lai). */
void Boot_RecoveryRun(uint32_t window_ms);

/** Da nhan it nhat 1 khung hop le gui toi PSU nay? (test host / cua so cho) */
bool Boot_RecoveryEngaged(void);

/* --- Phan giao thuc (khong phu thuoc phan cung, test duoc tren host) --- */
void Boot_RecoveryFeed(uint8_t port, uint8_t byte);
void Boot_RecoveryResync(uint8_t port);     /* bo khung do khi im lang lau */

/* --- Phan cung (boot_recovery_hw.c; test host thay bang ban gia) --- */
void RecHw_Init(void);
bool RecHw_RxByte(uint8_t port, uint8_t *byte);
void RecHw_Tx(uint8_t port, const uint8_t *buf, uint16_t len);
void RecHw_ResetSystem(void);               /* cho TX xong roi NVIC_SystemReset */

#endif /* BOOT_RECOVERY_H_ */
