/**
 ******************************************************************************
 * @file    led.h
 * @brief   Dieu khien 3 LED (do / xanh la / xanh duong) bang bitmap pattern.
 *
 * PHAN VAI: moi den tra loi DUNG MOT cau hoi.
 *   XANH LA    - Firmware con song khong?  (heartbeat, nhip ma hoa trang thai
 *                truyen thong: 1 nhay = co lien lac PC, 2 = chua tung nhan
 *                lenh, 3 = co kenh INA228 mat lien lac)
 *   XANH DUONG - Ngo ra CO DIEN khong?     (sang lien tuc = PWR_EN dang bat.
 *                Day la den an toan cho nguoi thao tac.)
 *   DO         - Loi gi?                   (so nhay = ma loi fault_code_t)
 *
 * CO CHE: bitmap 32 khe x LED_TICK_MS. Them pattern moi = them mot hang so,
 * khong dung vao logic. Led_Task() chay het trong ~1 us, khong blocking.
 ******************************************************************************
 */

#ifndef LED_H_
#define LED_H_

#include <safety.h>
#include <stdint.h>
#include <stdbool.h>

/* ==========================================================================
 * CAU HINH
 * ========================================================================== */

#define LED_TICK_MS     100u    /* do rong mot khe                        */
#define LED_SLOTS       32u     /* chu ky = 32 x 100 ms = 3.2 s           */

/* 1 = GPIO muc CAO lam LED sang. 0 = muc THAP (LED noi len VCC). */
#ifndef LED_ACTIVE_HIGH
#define LED_ACTIVE_HIGH 0
#endif

/* ==========================================================================
 * CO TRANG THAI PHU (truyen tu Safety_Task)
 * ========================================================================== */

#define LED_F_PC_LINK   0x01u   /* nhan duoc khung hop le tu PC gan day   */
#define LED_F_CH_STALE  0x02u   /* it nhat 1 kenh INA228 mat lien lac     */
#define LED_F_BUS_LOST  0x04u   /* ca 4 kenh deu mat lien lac             */
#define LED_F_WARN      0x08u   /* co canh bao nhung chua den muc trip    */

/* ==========================================================================
 * API
 * ========================================================================== */

/** @brief Goi mot lan sau MX_GPIO_Init(). */
void Led_Init(void);

/** @brief Goi tu Safety_Task(). Chi cap nhat bitmap, khong ghi GPIO. */
void Led_SetStatus(safe_state_t st, fault_code_t code, uint8_t flags);

/** @brief Goi moi vong main loop. Tu gian nhip theo LED_TICK_MS. */
void Led_Task(uint32_t now_ms);

/** @brief Nhay do lien tuc, blocking vinh vien. Dung trong Error_Handler().
 *         Khong phu thuoc SysTick (dung vong tre dem chu ky). */
void Led_PanicBlink(void);

#endif /* LED_H_ */
