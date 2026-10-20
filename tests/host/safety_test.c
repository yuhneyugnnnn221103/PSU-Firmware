/* Test may trang thai safety theo cum (safety.c that, phan cung gia).
 * Cum 1 co phan cung (INA1..3, chan EN, 4 chan FT); cum 2..4 chua co. Khong con LOCKOUT. */
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "ina228_driver.h"
#include "safety.h"
#include "comm.h"
#include "led.h"
#include "fw_update.h"
#include "psu_trip.h"

GPIO_TypeDef fake_gpiob;
static uint32_t now_ms; static int pwr_pin;
uint32_t HAL_GetTick(void){ return now_ms; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState st){ (void)p;(void)pin; pwr_pin = (st == GPIO_PIN_SET); }

static ina228_dev_t devs[INA228_CH_COUNT];
ina228_dev_t *ina228_get_dev(uint8_t i){ return i < INA228_CH_COUNT ? &devs[i] : 0; }
static uint8_t dcm;
uint8_t Comm_DcmFault(void){ return dcm; }
bool Comm_PcLinkAlive(uint32_t n){ (void)n; return true; }
fwu_state_t FwUpdate_State(void){ return FWU_IDLE; }
static safe_state_t led_st; static fault_code_t led_code;
void Led_SetStatus(safe_state_t st, fault_code_t c, uint8_t f){ led_st = st; led_code = c; (void)f; }

static int fails;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } }while(0)

static void run(uint32_t ms){ for(uint32_t i=0;i<ms;i++){ now_ms++; for(int c=0;c<INA228_CH_COUNT;c++) devs[c].meas.timestamp_ms = now_ms; Safety_Task(now_ms);} }
static void clear_diag(void){ for(int c=0;c<INA228_CH_COUNT;c++) devs[c].diag_sticky = 0; }
static void fresh_start(void){ memset(devs,0,sizeof devs); dcm=0; Safety_Init(); run(SAFE_BOOT_MS + 5); }
static void thr_none(void){ uint8_t z[4*PSU_TRIP_CLUSTER_STRIDE]; memset(z,0,sizeof z); PsuTrip_SetThresholds(z); }

static void power_up(void)
{
    Safety_Control(0x01, false);
    run(SAFE_OFF_MIN_MS + 5);   /* OFF -> ARMING */
    run(SAFE_ARM_MS + 5);       /* ARMING -> ON  */
}
static void trip_overcurrent(void){ devs[0].diag_sticky |= INA228_FLAG_SHNTOL; run(2); }

int main(void)
{
    thr_none();
    fresh_start();
    for (int c=0;c<4;c++) CHECK(Safety_State(c)==SAFE_OFF, "cum %d -> OFF sau boot", c);
    CHECK(Safety_AllOff() && !Safety_AnyPowerOn() && !pwr_pin, "tat ca OFF luc dau");

    /* --- Cum 2..4 chua co phan cung: luon OFF, bo qua lenh bat --- */
    Safety_Control(0x0E, false); run(SAFE_OFF_MIN_MS + 50);
    for (int c=1;c<4;c++) CHECK(Safety_State(c)==SAFE_OFF && !Safety_PowerIsOn(c), "cum %d khong co EN: van OFF", c);
    CHECK(!Safety_RequestPower(1,true) && Safety_RequestPower(1,false), "cum chua co EN: tu choi bat, chap nhan tat");
    CHECK(Safety_State(0)==SAFE_OFF && !pwr_pin, "bit 1..3 cua mask khong dieu khien EN cum 1");

    /* --- Cum 1 len nguon --- */
    power_up();
    CHECK(Safety_State(0)==SAFE_ON && pwr_pin && Safety_IsArmed(0) && Safety_AnyPowerOn() && !Safety_AllOff(), "cum 1 ON (state=%d)", Safety_State(0));
    CHECK(Safety_ClusterOfIna(0)==0 && Safety_ClusterOfIna(1)==0 && Safety_ClusterOfIna(2)==0 && Safety_ClusterOfIna(3)==0xFF, "INA1..3 thuoc cum 1; INA4 khong thuoc cum nao");
    CHECK(led_st==SAFE_ON, "LED nhan trang thai cum noi bat nhat");

    /* --- Trip: giu cho den ClearTrip --- */
    trip_overcurrent();
    CHECK(Safety_State(0)==SAFE_TRIPPED && Safety_Code(0)==FCODE_OVERCUR && !pwr_pin, "trip qua dong");
    CHECK(led_st==SAFE_TRIPPED && led_code==FCODE_OVERCUR, "LED bao TRIPPED");
    run(5000); clear_diag(); run(5);
    CHECK(Safety_State(0)==SAFE_TRIPPED, "TRIPPED giu cho den ClearTrip");
    Safety_Control(0x01, false);
    CHECK(Safety_State(0)==SAFE_TRIPPED, "dang TRIPPED: lenh bat bi tu choi");
    Safety_Control(0x01, true);                             /* Clear + bat trong cung khung */
    CHECK(Safety_State(0)==SAFE_OFF && !PsuTrip_Any(), "ClearTrip: xoa trip code, ve OFF");
    run(SAFE_OFF_MIN_MS + 50);
    CHECK(Safety_State(0)==SAFE_OFF && !pwr_pin, "lenh bat cung khung voi ClearTrip bi bo qua");
    Safety_Control(0x01, false); run(SAFE_OFF_MIN_MS + 5); run(SAFE_ARM_MS + 5);
    CHECK(Safety_State(0)==SAFE_ON, "bat lai o khung sau");
    Safety_Control(0x01, true); run(5);
    CHECK(Safety_State(0)==SAFE_ON && pwr_pin, "ClearTrip khi dang ON khong tat nguon");
    Safety_Control(0x00, false); run(5);
    CHECK(Safety_State(0)==SAFE_OFF && !pwr_pin, "mask bit0 = 0 -> tat");

    /* --- KHONG con LOCKOUT: trip nhieu lan lien tiep van chi cat nguon, ClearTrip thoat duoc --- */
    fresh_start();
    for (int n=1; n<=6; n++) {
        power_up();
        CHECK(Safety_State(0)==SAFE_ON, "lan %d len ON duoc (khong khoa)", n);
        trip_overcurrent();
        CHECK(Safety_State(0)==SAFE_TRIPPED && Safety_TripCount(0)==n, "trip lan %d (count=%u)", n, Safety_TripCount(0));
        Safety_Control(0x00, true); clear_diag();
    }
    power_up(); run(SAFE_STABLE_MS + 50);
    CHECK(Safety_TripCount(0)==0, "chay on dinh -> dem trip ve 0");

    /* --- Duong nhanh (ISR): theo INA, INA4 bo qua --- */
    fresh_start(); power_up();
    Safety_FastTrip(3);                                     /* INA4: khong thuoc cum nao */
    run(3);
    CHECK(Safety_State(0)==SAFE_ON && pwr_pin, "ALERT cua INA4 bi bo qua");
    Safety_FastTrip(1);
    CHECK(!pwr_pin, "FastTrip INA2 cat EN cum 1 ngay");
    run(2);
    CHECK(Safety_State(0)==SAFE_TRIPPED && Safety_Code(0)==FCODE_NONE, "TRIPPED chua co ma");
    devs[2].diag_sticky |= INA228_FLAG_BUSOL;
    run(SAFE_SETTLE_MS + 5);
    CHECK(Safety_Code(0)==FCODE_OVERVOLT && (PsuTrip_Bytes()[PSU_TRIP_B_INA_V] & 0x04), "ma loi phan loai sau settle; bit BUSOL INA3");
    fresh_start(); power_up(); Safety_FastTrip(0); run(SAFE_SETTLE_MS + 10);
    CHECK(Safety_Code(0)==FCODE_BUS && (PsuTrip_Bytes()[PSU_TRIP_B_MISC] & PSU_TRIP_MISC_UNKNOWN), "khong co diag -> BUS + bit UNKNOWN");

    /* --- So nguong tren STM (cum 1): I_OUT_ADC2 MAX, TEMP_ADC3 MIN --- */
    fresh_start();
    { uint8_t cfg[4*PSU_TRIP_CLUSTER_STRIDE]; memset(cfg,0,sizeof cfg);
      cfg[12+0]=0x10;                       /* I_OUT_ADC2 MAX = 0x100000 (offset 12) */
      cfg[33+1]=0x01;                       /* TEMP_ADC3 MIN = 0x000100 (offset 30 MAX, 33 MIN: 00 01 00) */
      PsuTrip_SetThresholds(cfg); }
    for (int c=0;c<3;c++) { devs[c].raw.current_reg24 = 0x000010; devs[c].raw.dietemp_raw = 0x0800; }
    power_up();
    CHECK(Safety_State(0)==SAFE_ON && !PsuTrip_Any(), "trong nguong");
    devs[1].raw.current_reg24 = 0x110000; run(5);
    CHECK(Safety_State(0)==SAFE_TRIPPED && Safety_Code(0)==FCODE_THRESHOLD, "I_OUT_ADC2 vuot MAX (state=%d code=%d)", Safety_State(0), Safety_Code(0));
    CHECK(PsuTrip_Bytes()[PSU_TRIP_B_SW_MAX]==0x02 && PsuTrip_Bytes()[PSU_TRIP_B_SW_MIN]==0, "TRIP1 bit1");
    devs[1].raw.current_reg24 = 0x000010; run(2000);
    CHECK(PsuTrip_Bytes()[PSU_TRIP_B_SW_MAX]==0x02, "trip giu nguyen khi gia tri tro lai binh thuong");
    Safety_Control(0x00, true);
    CHECK(!PsuTrip_Any() && Safety_State(0)==SAFE_OFF, "ClearTrip xoa");
    power_up();
    devs[2].raw.dietemp_raw = 0x0010; run(5);                /* duoi MIN 0x100 */
    CHECK(Safety_State(0)==SAFE_TRIPPED && (PsuTrip_Bytes()[PSU_TRIP_B_SW_MIN] & (1u<<(PSU_DCM_PER_CLUSTER+2))), "TEMP_ADC3 duoi MIN -> TRIP2 bit5");

    /* Duoi MIN chi tinh khi ON: luc OFF/ARMING gia tri ~ 0 khong trip */
    fresh_start();
    { uint8_t cfg[4*PSU_TRIP_CLUSTER_STRIDE]; memset(cfg,0,sizeof cfg); cfg[3+1]=0x10; PsuTrip_SetThresholds(cfg); }  /* I_OUT_ADC1 MIN = 0x001000 */
    for (int c=0;c<3;c++) devs[c].raw.current_reg24 = 0;
    Safety_Control(0x01,false); run(SAFE_OFF_MIN_MS + 5); run(SAFE_ARM_MS - 50);
    CHECK(Safety_State(0)==SAFE_ARMING && !PsuTrip_Any(), "ARMING: gia tri thap khong trip");
    for (int c=0;c<3;c++) devs[c].raw.current_reg24 = 0x002000;
    run(100);
    CHECK(Safety_State(0)==SAFE_ON && !PsuTrip_Any(), "ON binh thuong");

    /* --- Co phan cung INA -> bit trip --- */
    fresh_start(); thr_none(); power_up();
    devs[2].diag_sticky |= INA228_FLAG_SHNTOL | INA228_FLAG_BUSOL; run(3);
    CHECK(Safety_Code(0)==FCODE_OVERCUR && (PsuTrip_Bytes()[PSU_TRIP_B_INA_OC] & 0x04) && (PsuTrip_Bytes()[PSU_TRIP_B_INA_V] & 0x04), "co INA3 -> TRIP3/TRIP4 bit 2");
    Safety_ClearFault(); clear_diag();
    CHECK(!PsuTrip_Any(), "ClearTrip xoa bit INA");

    /* --- Chan FT cua DCM thuoc cum 1 (ca FT4) --- */
    fresh_start(); power_up(); dcm = 0x08; run(3);
    CHECK(Safety_Code(0)==FCODE_DCM && (PsuTrip_Bytes()[PSU_TRIP_B_TEMP_FT] & 0x40), "FT cua DCM4 -> bit 6 cua TRIP5");
    dcm = 0;

    /* --- Safety_ForceOff: ghi BSRR reset bit, khong phu thuoc state --- */
    fake_gpiob.BSRR = 0;
    Safety_ForceOff();
    CHECK(fake_gpiob.BSRR == ((uint32_t)PWR_EN_CTRL_Pin << 16),"ForceOff BSRR=%08x",fake_gpiob.BSRR);

    printf("safety tests done, fails=%d\n", fails);
    return fails ? 1 : 0;
}
