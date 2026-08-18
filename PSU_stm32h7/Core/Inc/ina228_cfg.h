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
                             INA228_FLAG_BUSUL  | INA228_FLAG_POL    | \
                             INA228_FLAG_MATHOF)

///* ==========================================================================
// * 5. NGUONG CANH BAO
// *
// * LUU Y DATASHEET: cac thanh ghi nguong tro ve mac dinh sau MOI chu ky
// * nguon VS -> phai nap lai moi lan cap nguon. ina228_verify_config() chay
// * dinh ky chinh la co che phat hien va nap lai.
// *
// * LSB cua thanh ghi nguong lon gap 16 lan LSB do luong (thanh ghi nguong
// * 16-bit vs du lieu 20-bit).
// * ========================================================================== */
//
///* SOVL/SUVL: 1.25 uV/LSB khi ADCRANGE = 1  (= 78.125 nV x 16) */
//#define INA228_SOVL_LSB_V         1.25e-6f
///* BOVL/BUVL: 3.125 mV/LSB  (= 195.3125 uV x 16), khong dau, 15-bit */
//#define INA228_BOVL_LSB_V         3.125e-3f
///* TEMP_LIMIT: 7.8125 m degC/LSB, two's complement */
//#define INA228_TLIM_LSB_C         7.8125e-3f
///* PWR_LIMIT: 256 x POWER_LSB = 64 mW/LSB, khong dau */
//#define INA228_PLIM_LSB_W         (256.0f * INA228_POWER_LSB)
//
///* Macro quy doi don vi vat ly -> gia tri thanh ghi */
//#define INA228_AMP_TO_SOVL(a)     ((uint16_t)(int16_t)((a) * INA228_R_SHUNT_OHM / INA228_SOVL_LSB_V))
//#define INA228_VOLT_TO_BOVL(v)    ((uint16_t)((v) / INA228_BOVL_LSB_V))
//#define INA228_DEGC_TO_TLIM(t)    ((uint16_t)(int16_t)((t) / INA228_TLIM_LSB_C))
//#define INA228_WATT_TO_PLIM(w)    ((uint16_t)((w) / INA228_PLIM_LSB_W))
//
///* --- Nguong thuc te. Sua o day, khong sua cho khac. --- */
//#define INA228_OVERCURRENT_A       28.0f    /* 25 A dinh muc + 12% du         */
//#define INA228_REVCURRENT_A       (-5.0f)   /* dong nguoc dang ke             */
//#define INA228_OVERTEMP_C          100.0f   /* die temp, package gioi han 125 */
//
//#define INA228_SOVL_VALUE         INA228_AMP_TO_SOVL(INA228_OVERCURRENT_A)
//#define INA228_SUVL_VALUE         INA228_AMP_TO_SOVL(INA228_REVCURRENT_A)
//#define INA228_TEMP_LIMIT_VALUE   INA228_DEGC_TO_TLIM(INA228_OVERTEMP_C)
//
///* TODO: dien theo dai bus thuc te cua DCM4623TD2H26F0T00 truoc khi ra board.
// * De o gia tri reset = tat canh bao (BOVL 7FFFh = 102 V, BUVL 0h = 0 V). */
//#define INA228_BOVL_ENABLE        0
//#define INA228_BUVL_ENABLE        0
//#define INA228_BUS_OVERVOLT_V     30.0f
//#define INA228_BUS_UNDERVOLT_V    18.0f
//
//#if INA228_BOVL_ENABLE
//  #define INA228_BOVL_VALUE       INA228_VOLT_TO_BOVL(INA228_BUS_OVERVOLT_V)
//#else
//  #define INA228_BOVL_VALUE       0x7FFFu
//#endif
//#if INA228_BUVL_ENABLE
//  #define INA228_BUVL_VALUE       INA228_VOLT_TO_BOVL(INA228_BUS_UNDERVOLT_V)
//#else
//  #define INA228_BUVL_VALUE       0x0000u
//#endif
//
///* PWR_LIMIT phu thuoc dien ap bus -> de mac dinh (max) cho toi khi chot */
//#define INA228_PWR_LIMIT_VALUE    0xFFFFu

/* ==========================================================================
 * 6. THAM SO BUS / THOI GIAN
 * ========================================================================== */
#define INA228_I2C_TIMEOUT_MS     5u     /* timeout 1 giao dich blocking     */
#define INA228_I2C_RETRY_MAX      3u     /* CHI ap dung cho lop blocking     */
#define INA228_DMA_TIMEOUT_MS     10u    /* callback DMA khong ve -> vut chu ky */
#define INA228_STALE_MS           500u   /* qua han nay coi nhu du lieu chet */

#define INA228_SCAN_PERIOD_MS     60u    /* > 50.5 ms chu ky ADC             */

#endif /* INA228_CFG_H_ */
