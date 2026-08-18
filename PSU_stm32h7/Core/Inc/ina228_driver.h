#ifndef INA228_DRIVER_H_
#define INA228_DRIVER_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32h7xx_hal.h"
#include "ina228_cfg.h"

/* Section cho buffer DMA. Phai ton tai trong linker script va nam ngoai
 * DTCM (DMA1/DMA2 khong truy cap duoc DTCM tren STM32H7). Vd:
 *   .dma_buffer (NOLOAD) : { . = ALIGN(32); *(.dma_buffer) . = ALIGN(32); } >RAM_D1
 */
#ifndef INA228_DMA_SECTION
#define INA228_DMA_SECTION      ".dma_buffer"
#endif

/* ==========================================================================
 * KIEU DU LIEU
 * ========================================================================== */

typedef enum {
    INA228_OK = 0,
    INA228_ERR_I2C,      /* loi truyen thong             */
    INA228_ERR_ID,       /* sai MANUF, hoac cau hinh lech */
    INA228_ERR_BUSY,     /* bus dang chay chu ky DMA     */
    INA228_ERR_PARAM
} ina228_status_t;

typedef enum {
    INA228_BUS_IDLE = 0,  /* ranh, co the goi API blocking */
    INA228_BUS_BUSY,      /* chu ky DMA dang chay          */
    INA228_BUS_DONE       /* co du lieu moi cho task lay   */
} ina228_bus_state_t;

/* Ket qua da quy doi sang don vi vat ly */
typedef struct {
    float    v_bus;          /* V    */
    float    v_shunt;        /* V    - chi cap nhat o chu ky day du */
    float    current;        /* A    */
    float    power;          /* W    - tinh tren MCU = v_bus * current */
    float    die_temp;       /* degC - chi cap nhat o chu ky day du */
    uint32_t timestamp_ms;   /* thoi diem hoan tat kenh nay         */
} ina228_meas_t;

/* Gia tri tho */
typedef struct {
    int32_t  vshunt_raw;     /* 20-bit sign-extended */
    uint32_t vbus_raw;       /* 20-bit unsigned      */
    int32_t  current_raw;    /* 20-bit sign-extended */
    uint32_t power_raw;		 /* 24-bit unsigned		 */
    int16_t  dietemp_raw;    /* 16-bit signed        */
} ina228_raw_t;

typedef struct {
    /* --- cau hinh, dien truoc khi goi ina228_bus_init() --- */
    uint8_t        addr7;            /* 0x40 / 0x41 / 0x44 / 0x45 */
    GPIO_TypeDef  *alert_port;
    uint16_t       alert_pin;

    /* --- trang thai --- */
    volatile bool  alert_pending;    /* set boi EXTI ISR       */
    uint16_t		adc_cfg;
    uint16_t       diag_alrt;        /* snapshot lan doc gan nhat */
    uint16_t       diag_sticky;      /* OR don, xoa boi ina228_get_sticky */
    uint32_t       err_count;
    bool           cfg_ok;           /* ket qua verify_config gan nhat */

    /* --- ket qua --- */
    ina228_meas_t  meas;
    ina228_raw_t   raw;
} ina228_dev_t;

/* ==========================================================================
 * KHOI TAO
 * ========================================================================== */

/** @brief Gan bus va mang thiet bi. Goi mot lan, truoc moi API khac. */
ina228_status_t ina228_bus_init(I2C_HandleTypeDef *hi2c, ina228_dev_t *devs);

/** @brief Reset + nap toan bo cau hinh va nguong cho 1 IC. BLOCKING. */
ina228_status_t ina228_dev_init(ina228_dev_t *dev);

/** @brief Init ca 4 kenh. Tra ve OK neu it nhat 1 kenh nap duoc. */
ina228_status_t ina228_init_all(void);

/* ==========================================================================
 * TRUY CAP THANH GHI - BLOCKING (co retry)
 * ========================================================================== */

ina228_status_t ina228_write_reg16(ina228_dev_t *dev, uint8_t reg, uint16_t val);
ina228_status_t ina228_read_reg16 (ina228_dev_t *dev, uint8_t reg, uint16_t *val);

/* ==========================================================================
 * DO LUONG - STREAMING (DMA)
 * ========================================================================== */

/**
 * @retval INA228_ERR_BUSY neu chu ky truoc chua xong.
 */
ina228_status_t ina228_bus_start(void);

ina228_bus_state_t ina228_bus_state(void);

/**
 * @brief Lay du lieu ra khoi driver va tra bus ve IDLE.
 *        Goi khi ina228_bus_state() == INA228_BUS_DONE.
 *        An toan khong can khoa: ISR chi ghi khi state == BUSY.
 * @param out mang ina228_meas_t[INA228_CH_COUNT], co the NULL.
 */
void ina228_bus_collect(ina228_meas_t *out);

/**
 * @brief Goi dinh ky tu task (vd moi 1 ms). Bat buoc.
 *        Neu ngat DMA khong bao gio ve -> vut ca chu ky va reset cung I2C.
 */
void ina228_bus_tick(uint32_t now_ms);

/* Hook goi tu HAL callback trong main.c */
void ina228_i2c_rx_complete(I2C_HandleTypeDef *hi2c);
void ina228_i2c_error(I2C_HandleTypeDef *hi2c);

/* ==========================================================================
 * ALERT
 * ========================================================================== */

/** @brief Goi tu HAL_GPIO_EXTI_Callback. Chi dat co, KHONG cham I2C. */
void ina228_alert_isr(uint16_t gpio_pin);

/**
 * @brief Xu ly cac alert dang cho. BLOCKING.
 *        Voi ALATCH=1, chinh hanh dong doc DIAG_ALRT xoa co va nha chan
 *        ALERT. Vi vay CHI duoc goi ham nay o MOT cho duy nhat trong
 *        firmware, va chi khi bus khong BUSY.
 */
ina228_status_t ina228_alert_process(void);

/** @brief Lay (va tuy chon xoa) co loi tich luy. */
uint16_t ina228_get_sticky(ina228_dev_t *dev, bool clear);

/* ==========================================================================
 * TIEN ICH
 * ========================================================================== */

ina228_dev_t *ina228_get_dev(uint8_t idx);

/** @brief Du lieu cua kenh con moi khong? Thay cho co online/offline. */
static inline bool ina228_is_fresh(const ina228_dev_t *d, uint32_t now_ms)
{
    return (uint32_t)(now_ms - d->meas.timestamp_ms) < INA228_STALE_MS;
}

///** @brief Thoi diem chu ky gan nhat hoan tat - dung de gate IWDG refresh. */
//uint32_t ina228_last_cycle_ms(void);

/** @brief Buffer DMA co nam ngoai DTCM khong. Goi mot lan luc bring-up. */
bool ina228_check_dma_buffer(void);

#endif /* INA228_DRIVER_H_ */
