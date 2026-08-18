/**
 ******************************************************************************
 * @file    ina228_driver.c
 * @brief   Driver INA228 - state machine chi duoc day tien tu ngat DMA.
 ******************************************************************************
 */

#include <string.h>
#include "ina228_driver.h"

///* Ham do CubeMX sinh, dung lai khi reset cung ngoai vi */
//extern void MX_I2C1_Init(void);

/* ==========================================================================
 * TRANG THAI NOI BO
 * ========================================================================== */

typedef struct {
    uint8_t reg;
    uint8_t len;
} ina_read_item_t;

/* Vong day du: them nhiet do va VSHUNT de kiem chung hieu chuan */
static const ina_read_item_t s_seq_full[] = {
		{ INA228_REG_ADC_CONFIG, 2},
	    { INA228_REG_VBUS,    3 },
	    { INA228_REG_CURRENT, 3 },
//	    { INA228_REG_POWER,   3 },
	    { INA228_REG_DIETEMP, 2 },
//	    { INA228_REG_VSHUNT,  3 },
};
#define INA_READ_SEQ_LEN  (sizeof(s_seq_full) / sizeof(s_seq_full[0]))

#if defined(__GNUC__)
  #define INA_DMA_ATTR  __attribute__((section(INA228_DMA_SECTION), aligned(32)))
#elif defined(__ICCARM__)
  #define INA_DMA_ATTR  _Pragma("location=\".dma_buffer\"")
#else
  #define INA_DMA_ATTR
#endif

#define INA228_MAX_REG_LEN   5U
static INA_DMA_ATTR uint8_t s_rxbuf[INA228_MAX_REG_LEN];

/* DTCM tren STM32H725: 0x20000000 - 0x2001FFFF. DMA1/DMA2 khong voi toi. */
#define INA_DTCM_BASE   0x20000000u
#define INA_DTCM_END    0x2001FFFFu

static struct {
    I2C_HandleTypeDef      *hi2c;
    ina228_dev_t           *devs;
    bool                    inited;

    volatile ina228_bus_state_t state;
    volatile uint8_t        ch_idx;
    volatile uint8_t        seq_idx;
    const ina_read_item_t  *seq;
    uint8_t                 seq_len;
    volatile uint32_t       op_start_ms;

    uint32_t                cycle_count;
    uint32_t                timeout_count;
    uint32_t                last_cycle_ms;
} s_bus;

/* ==========================================================================
 * GIAI MA BYTE
 * ========================================================================== */

/* 3 byte big-endian -> 20-bit co dau (data o bit23..4) */
static inline int32_t ina_parse_20_signed(const uint8_t *b)
{
    int32_t v = ((int32_t)b[0] << 16) | ((int32_t)b[1] << 8) | (int32_t)b[2];
    v >>= 4;
    if (v & 0x00080000) v |= (int32_t)0xFFF00000;
    return v;
}

static inline uint32_t ina_parse_20_unsigned(const uint8_t *b)
{
    uint32_t v = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[2];
    return (v >> 4) & 0x000FFFFFu;
}

/* POWER: 24-bit khong dau, dung ca 24 bit */
static inline uint32_t ina_parse_24_unsigned(const uint8_t *b)
{
    return ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[2];
}

static inline uint64_t ina_parse_40_unsigned(const uint8_t *b)
{
    uint64_t v = 0;
    for (int i = 0; i < 5; i++) v = (v << 8) | (uint64_t)b[i];
    return v;
}

static inline int64_t ina_parse_40_signed(const uint8_t *b)
{
    int64_t v = (int64_t)ina_parse_40_unsigned(b);
    if (v & 0x8000000000LL) v |= (int64_t)0xFFFFFF0000000000LL;
    return v;
}

/* Nap gia tri doc duoc vao dev. Dung chung cho ca duong blocking lan DMA. */
static void ina_store(ina228_dev_t *dev, uint8_t reg, const uint8_t *b)
{
    switch (reg) {
    case INA228_REG_ADC_CONFIG:
    	dev->adc_cfg = (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
    	dev->cfg_ok = (dev->adc_cfg == INA228_ADC_CONFIG_VALUE);
    	break;

    case INA228_REG_VBUS:
        dev->raw.vbus_raw = ina_parse_20_unsigned(b);
        dev->meas.v_bus   = (float)dev->raw.vbus_raw * INA228_VBUS_LSB;
        break;

    case INA228_REG_CURRENT:
        dev->raw.current_raw = ina_parse_20_signed(b);
        dev->meas.current    = (float)dev->raw.current_raw * INA228_CURRENT_LSB;
        break;

//    case INA228_REG_POWER:
//        dev->raw.power_raw = ina_parse_24_unsigned(b);
//        dev->meas.power    = (float)dev->raw.power_raw * INA228_POWER_LSB;
//        break;

    case INA228_REG_DIETEMP:
        dev->raw.dietemp_raw = (int16_t)(((uint16_t)b[0] << 8) | b[1]);
        dev->meas.die_temp   = (float)dev->raw.dietemp_raw * INA228_DIETEMP_LSB;
        break;

//    case INA228_REG_VSHUNT:
//        dev->raw.vshunt_raw = ina_parse_20_signed(b);
//        dev->meas.v_shunt   = (float)dev->raw.vshunt_raw * INA228_VSHUNT_LSB;
//        break;

    default:
        break;
    }
}

/* ==========================================================================
 * BLOCKING	(hàm test bring up nhanh, khi implement sẽ dùng DMA)
 * ========================================================================== */

static ina228_status_t ina_read_blocking(ina228_dev_t *dev, uint8_t reg,
                                         uint8_t *buf, uint8_t len)
{
    for (uint32_t i = 0; i < INA228_I2C_RETRY_MAX; i++) {
        if (HAL_I2C_Mem_Read(s_bus.hi2c, INA228_HAL_ADDR(dev->addr7),
                             reg, I2C_MEMADD_SIZE_8BIT,
                             buf, len, INA228_I2C_TIMEOUT_MS) == HAL_OK) {
            return INA228_OK;
        }
    }
    dev->err_count++;
    return INA228_ERR_I2C;
}

ina228_status_t ina228_write_reg16(ina228_dev_t *dev, uint8_t reg, uint16_t val)
{
    uint8_t b[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xFFu) };

    for (uint32_t i = 0; i < INA228_I2C_RETRY_MAX; i++) {
        if (HAL_I2C_Mem_Write(s_bus.hi2c, INA228_HAL_ADDR(dev->addr7),
                              reg, I2C_MEMADD_SIZE_8BIT,
                              b, 2, INA228_I2C_TIMEOUT_MS) == HAL_OK) {
            return INA228_OK;
        }
    }
    dev->err_count++;
    return INA228_ERR_I2C;
}

ina228_status_t ina228_read_reg16(ina228_dev_t *dev, uint8_t reg, uint16_t *val)
{
    uint8_t b[2];
    ina228_status_t st = ina_read_blocking(dev, reg, b, 2);
    if (st == INA228_OK) *val = ((uint16_t)b[0] << 8) | b[1];
    return st;
}

/* ==========================================================================
 * KHOI TAO
 * ========================================================================== */

ina228_status_t ina228_bus_init(I2C_HandleTypeDef *hi2c, ina228_dev_t *devs)
{
    if (hi2c == NULL || devs == NULL) return INA228_ERR_PARAM;

    memset(&s_bus, 0, sizeof(s_bus));
    s_bus.hi2c    = hi2c;
    s_bus.devs    = devs;
    s_bus.seq     = s_seq_full;
    s_bus.seq_len = INA_READ_SEQ_LEN;
    s_bus.state   = INA228_BUS_IDLE;
    s_bus.inited  = true;
    return INA228_OK;
}

ina228_status_t ina228_dev_init(ina228_dev_t *dev)
{
    uint16_t id;
    ina228_status_t st;

    if (!s_bus.inited || dev == NULL)     return INA228_ERR_PARAM;
    if (s_bus.state == INA228_BUS_BUSY)   return INA228_ERR_BUSY;

    dev->cfg_ok = false;

    /* 1. Xac nhan dung IC dung dia chi */
    st = ina228_read_reg16(dev, INA228_REG_MANUF_ID, &id);
    if (st != INA228_OK) return st;
    if (id != INA228_MANUF_ID_EXPECTED) return INA228_ERR_ID;

    /* 2. Reset ve mac dinh */
    st = ina228_write_reg16(dev, INA228_REG_CONFIG, INA228_CONFIG_RESET);
    if (st != INA228_OK) return st;
    HAL_Delay(2);

    /* 3. Cau hinh. Thu tu quan trong: CONFIG (ADCRANGE) TRUOC SHUNT_CAL,
     *    vi he so 4 trong SHUNT_CAL phu thuoc ADCRANGE. */
    struct { uint8_t reg; uint16_t val; } cfg[] = {
        { INA228_REG_CONFIG,       INA228_CONFIG_VALUE       },
        { INA228_REG_SHUNT_CAL,    INA228_SHUNT_CAL_VALUE    },
        { INA228_REG_SHUNT_TEMPCO, INA228_SHUNT_TEMPCO_VALUE },
        { INA228_REG_ADC_CONFIG,   INA228_ADC_CONFIG_VALUE   },

        /* Nguong: datasheet noi ro cac thanh ghi nay ve mac dinh sau moi
         * chu ky nguon VS -> phai nap lai moi lan khoi tao. */
//        { INA228_REG_SOVL,         INA228_SOVL_VALUE         },
//        { INA228_REG_SUVL,         INA228_SUVL_VALUE         },
//        { INA228_REG_BOVL,         INA228_BOVL_VALUE         },
//        { INA228_REG_BUVL,         INA228_BUVL_VALUE         },
//        { INA228_REG_TEMP_LIMIT,   INA228_TEMP_LIMIT_VALUE   },
//        { INA228_REG_PWR_LIMIT,    INA228_PWR_LIMIT_VALUE    },

        /* DIAG_ALRT sau cung: chi bat ALERT khi moi nguong da san sang */
        { INA228_REG_DIAG_ALRT,    INA228_DIAG_ALRT_VALUE    },
    };

    for (uint32_t i = 0; i < sizeof(cfg) / sizeof(cfg[0]); i++) {
        st = ina228_write_reg16(dev, cfg[i].reg, cfg[i].val);
        if (st != INA228_OK) return st;
    }

    /* 4. Doc DIAG_ALRT de xoa co POR va kiem tra checksum trim memory */
    st = ina228_read_reg16(dev, INA228_REG_DIAG_ALRT, &dev->diag_alrt);
    if (st != INA228_OK) return st;
    if ((dev->diag_alrt & INA228_FLAG_MEMSTAT) == 0u) {
        dev->diag_sticky |= INA228_FLAG_MEMSTAT;   /* IC co the sai so nang */
    }

    dev->alert_pending = false;
    dev->cfg_ok        = true;
    return INA228_OK;
}

ina228_status_t ina228_init_all(void)
{
    uint8_t ok = 0;

    if (!s_bus.inited) return INA228_ERR_PARAM;

    for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
        if (ina228_dev_init(&s_bus.devs[i]) == INA228_OK) ok++;
    }
    return (ok > 0) ? INA228_OK : INA228_ERR_I2C;
}

/* ==========================================================================
 * STATE MACHINE - CHI CHAY TRONG NGAT DMA
 * ========================================================================== */

static void ina_finish_cycle(void)
{
    s_bus.last_cycle_ms = HAL_GetTick();
    s_bus.cycle_count++;
    s_bus.state = INA228_BUS_DONE;
}

/* Phat lenh doc ke tiep. Neu launch that bai -> bo ca kenh, sang kenh sau.
 * Vong while thay cho de quy: 4 kenh chet het van ket thuc trong 4 vong. */
static void ina_kick(void)
{
    while (s_bus.ch_idx < INA228_CH_COUNT) {
        const ina_read_item_t *it = &s_bus.seq[s_bus.seq_idx];

        s_bus.op_start_ms = HAL_GetTick();

        if (HAL_I2C_Mem_Read_DMA(s_bus.hi2c,
                                 INA228_HAL_ADDR(s_bus.devs[s_bus.ch_idx].addr7),
                                 it->reg, I2C_MEMADD_SIZE_8BIT,
                                 s_rxbuf, it->len) == HAL_OK) {
            return;                       /* cho ngat DMA goi ina_advance() */
        }

        s_bus.devs[s_bus.ch_idx].err_count++;
        s_bus.seq_idx = 0;
        s_bus.ch_idx++;
    }
    ina_finish_cycle();
}

static void ina_advance(bool ok)
{
    if (ok) {
        if (++s_bus.seq_idx < s_bus.seq_len) {
            ina_kick();
            return;
        }
        s_bus.devs[s_bus.ch_idx].meas.timestamp_ms = HAL_GetTick();
    } else {
        s_bus.devs[s_bus.ch_idx].err_count++;
        /* Khong retry o tang streaming: lan thu lai la chu ky ke tiep. */
    }

    s_bus.seq_idx = 0;
    s_bus.ch_idx++;
    ina_kick();
}

ina228_status_t ina228_bus_start(void)
{
    if (!s_bus.inited)                  return INA228_ERR_PARAM;
    if (s_bus.state == INA228_BUS_BUSY) return INA228_ERR_BUSY;

	s_bus.seq     = s_seq_full;
	s_bus.seq_len = (uint8_t)(sizeof(s_seq_full) / sizeof(s_seq_full[0]));

    s_bus.ch_idx  = 0;
    s_bus.seq_idx = 0;
    s_bus.state   = INA228_BUS_BUSY;    /* tu day tro di ISR so huu con tro */

    ina_kick();
    return INA228_OK;
}

void ina228_i2c_rx_complete(I2C_HandleTypeDef *hi2c)			// Đặt trong callback
{
    if (hi2c != s_bus.hi2c || s_bus.state != INA228_BUS_BUSY) return;

    ina_store(&s_bus.devs[s_bus.ch_idx],
              s_bus.seq[s_bus.seq_idx].reg, s_rxbuf);
    ina_advance(true);
}

void ina228_i2c_error(I2C_HandleTypeDef *hi2c)					// Đặt trong callback
{
    if (hi2c != s_bus.hi2c || s_bus.state != INA228_BUS_BUSY) return;
    ina_advance(false);
}

ina228_bus_state_t ina228_bus_state(void)
{
    return s_bus.state;
}

void ina228_bus_collect(ina228_meas_t *out)
{
    if (s_bus.state != INA228_BUS_DONE) return;

    if (out != NULL) {
        for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
            out[i] = s_bus.devs[i].meas;
        }
    }
    s_bus.state = INA228_BUS_IDLE;
}

void ina228_bus_tick(uint32_t now_ms)	// có thể thêm 9-bit clock recover ở đây sau
{
    if (s_bus.state != INA228_BUS_BUSY) return;
    if ((uint32_t)(now_ms - s_bus.op_start_ms) <= INA228_DMA_TIMEOUT_MS) return;

    __disable_irq();
    s_bus.state = INA228_BUS_IDLE;
    __enable_irq();

    /* Ep ngoai vi ve READY. Abort_IT khong blocking, khong goi HAL_Delay. */
	(void)HAL_I2C_Master_Abort_IT(s_bus.hi2c,
			  INA228_HAL_ADDR(s_bus.devs[s_bus.ch_idx].addr7));
	s_bus.hi2c->ErrorCode = HAL_I2C_ERROR_NONE;

    s_bus.timeout_count++;
}

/* ==========================================================================
 * ALERT
 * ========================================================================== */

void ina228_alert_isr(uint16_t gpio_pin)
{
    if (!s_bus.inited) return;

    for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
        if (s_bus.devs[i].alert_pin == gpio_pin) {
            s_bus.devs[i].alert_pending = true;
            return;
        }
    }
}

ina228_status_t ina228_alert_process(void)
{
    ina228_status_t rc = INA228_OK;

    if (s_bus.state == INA228_BUS_BUSY) return INA228_ERR_BUSY;

    for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
        ina228_dev_t *dev = &s_bus.devs[i];

        if (!dev->alert_pending) continue;
        dev->alert_pending = false;

        /* Voi ALATCH = 1, chinh lenh doc nay xoa co va nha chan ALERT.
         * Do do KHONG duoc doc DIAG_ALRT o bat ky cho nao khac luc chay. */
        if (ina228_read_reg16(dev, INA228_REG_DIAG_ALRT, &dev->diag_alrt)
            != INA228_OK) {
            dev->alert_pending = true;   /* giu lai, thu lai vong sau */
            rc = INA228_ERR_I2C;
            continue;
        }
        dev->diag_sticky |= (dev->diag_alrt & INA228_FAULT_MASK);

        /* Chan van thap sau khi doc -> nguyen nhan chua het */
        if (HAL_GPIO_ReadPin(dev->alert_port, dev->alert_pin) == GPIO_PIN_RESET) {
            dev->alert_pending = true;
        }
    }
    return rc;
}

uint16_t ina228_get_sticky(ina228_dev_t *dev, bool clear)
{
    uint16_t v = dev->diag_sticky;
    if (clear) dev->diag_sticky = 0;
    return v;
}

/* ==========================================================================
 * TIEN ICH
 * ========================================================================== */

ina228_dev_t *ina228_get_dev(uint8_t idx)
{
    if (!s_bus.inited || idx >= INA228_CH_COUNT) return NULL;
    return &s_bus.devs[idx];
}

bool ina228_check_dma_buffer(void)
{
    uint32_t addr = (uint32_t)(uintptr_t)s_rxbuf;
    return !(addr >= INA_DTCM_BASE && addr <= INA_DTCM_END);
}

// Callback

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) { ina228_i2c_rx_complete(hi2c); }
void HAL_I2C_ErrorCallback   (I2C_HandleTypeDef *hi2c) { ina228_i2c_error(hi2c); }
void HAL_GPIO_EXTI_Callback  (uint16_t pin)            { ina228_alert_isr(pin); }
