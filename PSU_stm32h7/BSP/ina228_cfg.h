/**
 ******************************************************************************
 * @file    ina228_cfg.h
 * @brief   Cau hinh cho 4 x INA228 tren bus I2C1 (STM32H725RGV3)
 *
 * PHAN CUNG:
 *   R_shunt   : 1 mOhm (HoLRS3920, 5 W, TCR <= 25 ppm/C)
 *   I dinh muc: 25 A -> V_sense = 25 mV
 *   Dai ADC   : ADCRANGE = 1 (+/-40.96 mV) -> dung 61% thang
 *   Dia chi   : 0x40 / 0x41 / 0x44 / 0x45 (A1/A0 hardwired tren PCB)
 *   ALERT     : PA4 / PA5 / PA6 / PA7 (EXTI, active low, open-drain)
 *
 * CHOT: CURRENT_LSB = 78.125 uA (KHONG dung 25/2^19 = 47.68 uA).
 *   Ly do: khop dai CURRENT (+/-40.96 A) voi dung dai ADC -> khong bi kep
 *   khi su co day dong vuot 25 A. Do phan giai mat di khong dang ke vi
 *   nhieu thuc te ~4 mA p-p tai AVG = 16.
 ******************************************************************************
 */

#ifndef INA228_CFG_H_
#define INA228_CFG_H_

#include <stdint.h>

/* ==========================================================================
 * 1. DIA CHI I2C (7-bit)
 * ========================================================================== */
#define INA228_ADDR_CH1           0x40u   /* A1=GND, A0=GND */
#define INA228_ADDR_CH2           0x41u   /* A1=GND, A0=VS  */
#define INA228_ADDR_CH3           0x44u   /* A1=VS , A0=GND */
#define INA228_ADDR_CH4           0x45u   /* A1=VS , A0=VS  */

#define INA228_CH_COUNT           4u

/* HAL dung dia chi da dich trai 1 bit */
#define INA228_HAL_ADDR(a)        ((uint16_t)((a) << 1))

/* ==========================================================================
 * 2. MAP REGISTER
 * ========================================================================== */
#define INA228_REG_CONFIG         0x00u  /* 16-bit */
#define INA228_REG_ADC_CONFIG     0x01u  /* 16-bit */
#define INA228_REG_SHUNT_CAL      0x02u  /* 16-bit */
#define INA228_REG_SHUNT_TEMPCO   0x03u  /* 16-bit */

#define INA228_REG_VSHUNT         0x04u  /* 24-bit, data 20-bit tai bit23..4 */
#define INA228_REG_VBUS           0x05u  /* 24-bit, data 20-bit, luon duong  */
#define INA228_REG_DIETEMP        0x06u  /* 16-bit, two's complement         */
#define INA228_REG_CURRENT        0x07u  /* 24-bit, data 20-bit, co dau      */
#define INA228_REG_POWER          0x08u  /* 24-bit, khong dau                */
#define INA228_REG_ENERGY         0x09u  /* 40-bit, khong dau                */
#define INA228_REG_CHARGE         0x0Au  /* 40-bit, two's complement         */

#define INA228_REG_DIAG_ALRT      0x0Bu  /* 16-bit */

#define INA228_REG_SOVL           0x0Cu  /* 16-bit */
#define INA228_REG_SUVL           0x0Du  /* 16-bit */
#define INA228_REG_BOVL           0x0Eu  /* 16-bit */
#define INA228_REG_BUVL           0x0Fu  /* 16-bit */
#define INA228_REG_TEMP_LIMIT     0x10u  /* 16-bit */
#define INA228_REG_PWR_LIMIT      0x11u  /* 16-bit */

#define INA228_REG_MANUF_ID       0x3Eu  /* = 0x5449 ("TI") */
#define INA228_REG_DEVICE_ID      0x3Fu  /* = 0x2281        */

#define INA228_MANUF_ID_EXPECTED  0x5449u
#define INA228_DEVICE_ID_EXPECTED 0x2281u
#define INA228_DIEID_MASK         0xFFF0u  /* bo 4 bit REV_ID khi so sanh */

/* ==========================================================================
 * 3. HANG SO HIEU CHUAN
 * ========================================================================== */
#define INA228_R_SHUNT_OHM        0.001f

/* CURRENT_LSB = LSB_shunt / R_shunt = 78.125 nV / 1 mOhm */
#define INA228_CURRENT_LSB        78.125e-6f    /* A/LSB */
#define INA228_POWER_LSB          250.0e-6f     /* W/LSB = 3.2 x I_LSB  */
#define INA228_ENERGY_LSB         4.0e-3f       /* J/LSB = 16  x P_LSB  */
#define INA228_CHARGE_LSB         INA228_CURRENT_LSB   /* C/LSB */

/* LSB do luong truc tiep tu ADC */
#define INA228_VSHUNT_LSB         78.125e-9f    /* V/LSB (ADCRANGE = 1) */
#define INA228_VBUS_LSB           195.3125e-6f  /* V/LSB   */
#define INA228_DIETEMP_LSB        7.8125e-3f    /* degC/LSB */

#define INA228_I_FULLSCALE_A      40.96f        /* +/- , khop dung dai ADC */
#define INA228_I_RATED_A          25.0f         /* dinh muc he thong       */

/* ==========================================================================
 * 4. GIA TRI THANH GHI KHOI TAO
 * ========================================================================== */

/* CONFIG (0x00): RST=0, RSTACC=0, CONVDLY=0, TEMPCOMP=0, ADCRANGE=1 */
#define INA228_CONFIG_VALUE       0x0010u
#define INA228_CONFIG_RESET       0x8000u
#define INA228_CONFIG_RSTACC      0x4000u

/* SHUNT_CAL (0x02) = 4 x 13107.2e6 x CURRENT_LSB x R_SHUNT
 *                  = 4 x 13107.2e6 x 78.125e-6 x 0.001 = 4096
 * CANH BAO: trung dung gia tri reset (1000h) cua thanh ghi -> KHONG dung
 * thanh ghi nay de phat hien IC bi reset ngam. Dung ADC_CONFIG + ADCRANGE. */
#define INA228_SHUNT_CAL_VALUE    0x1000u

/* SHUNT_TEMPCO (0x03): 14-bit (13..0), 1 ppm/degC/LSB, full scale 16383.
 * De 0 o giai doan dau. Bu nay dung DIETEMP cua IC chu KHONG phai nhiet do
 * shunt -> chi nap sau khi do thuc te thay tuong quan tuyen tinh ro rang. */
#define INA228_SHUNT_TEMPCO_VALUE 0x0000u

/* ADC_CONFIG (0x01)
 *   MODE=Fh (lien tuc shunt+bus+temp), VBUSCT=VSHCT=VTCT=5h (1052 us),
 *   AVG=2h (16 mau)  ->  3 x 1052 us x 16 = 50.5 ms (~19.8 Hz)
 * Gia tri reset la 0xFB68 (AVG=0) -> khac gia tri nay -> dung de phat hien
 * IC bi reset ngam trong ina228_verify_config(). */
#define INA228_ADC_CONFIG_VALUE   0xFB6Au
#define INA228_ADC_CONFIG_RESET   0xFB68u

/* DIAG_ALRT (0x0B)
 *   ALATCH=1    : giu co + chan ALERT den khi doc thanh ghi nay
 *   CNVR=0      : khong dua conversion-ready ra chan ALERT
 *   SLOWALERT=0 : so sanh tren gia tri CHUA trung binh (BAT BUOC - neu =1
 *                 voi AVG=16 thi dap ung ALERT cham toi ~75 ms)
 *   APOL=0      : active low, khop pull-up tren PA4..PA7
 *   MEMSTAT     : doc ve 1 khi trim memory OK */
#define INA228_DIAG_ALRT_VALUE    0x8001u

/* Mat na bit trong DIAG_ALRT */
#define INA228_FLAG_ALATCH        (1u << 15)
#define INA228_FLAG_CNVR          (1u << 14)
#define INA228_FLAG_SLOWALERT     (1u << 13)
#define INA228_FLAG_APOL          (1u << 12)
#define INA228_FLAG_ENERGYOF      (1u << 11)
#define INA228_FLAG_CHARGEOF      (1u << 10)
#define INA228_FLAG_MATHOF        (1u <<  9)
#define INA228_FLAG_TMPOL         (1u <<  7)
#define INA228_FLAG_SHNTOL        (1u <<  6)  /* qua dong   */
#define INA228_FLAG_SHNTUL        (1u <<  5)  /* dong nguoc */
#define INA228_FLAG_BUSOL         (1u <<  4)
#define INA228_FLAG_BUSUL         (1u <<  3)
#define INA228_FLAG_POL           (1u <<  2)
#define INA228_FLAG_CNVRF         (1u <<  1)
#define INA228_FLAG_MEMSTAT       (1u <<  0)

#define INA228_FAULT_MASK   (INA228_FLAG_TMPOL  | INA228_FLAG_SHNTOL | \
                             INA228_FLAG_SHNTUL | INA228_FLAG_BUSOL  | \
                             INA228_FLAG_BUSUL  | INA228_FLAG_POL)

/* ==========================================================================
 * 5. HE SO QUY DOI NGUONG CANH BAO
 *   LSB cua thanh ghi ngUONG lon gap 16 lan LSB do luong 20-bit tuong ung
 *   (thanh ghi nguong chi 16-bit).
 * ========================================================================== */
#define INA228_SOVL_LSB_V     1.25e-6f    /* V/LSB, ADCRANGE = 1 (16 x 78.125 nV) */
#define INA228_BUSVL_LSB_V    3.125e-3f   /* V/LSB (16 x 195.3125 uV)             */

/* Gia tri "khong bao gio trip" */
#define INA228_SOVL_DISABLED  0x7FFFu
#define INA228_SUVL_DISABLED  0x8000u
#define INA228_BOVL_DISABLED  0x7FFFu
#define INA228_BUVL_DISABLED  0x0000u

/* Bien vat ly toi da bieu dien duoc */
#define INA228_SOVL_MAX_A     (32767.0f * INA228_SOVL_LSB_V / INA228_R_SHUNT_OHM)  /* 40.95 A */
#define INA228_BUSVL_MAX_V    (32767.0f * INA228_BUSVL_LSB_V)                      /* 102.4 V */

/* ==========================================================================
 * 6. THAM SO BUS / THOI GIAN
 * ========================================================================== */
#define INA228_I2C_TIMEOUT_MS     5u     /* timeout 1 giao dich blocking     */
#define INA228_I2C_RETRY_MAX      3u     /* CHI ap dung cho lop blocking     */
#define INA228_DMA_TIMEOUT_MS     10u    /* callback DMA khong ve -> vut chu ky */
#define INA228_STALE_MS           500u   /* qua han nay coi nhu du lieu chet */

#define INA228_SCAN_PERIOD_MS     60u    /* > 50.5 ms chu ky ADC             */

#define INA228_ALERT_MIN_GAP_MS   20u

#define INA228_DIAG_REFRESH_MS    250u

#endif /* INA228_CFG_H_ */
