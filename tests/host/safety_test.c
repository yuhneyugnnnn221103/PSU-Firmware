/* Test may trang thai safety (safety.c that, phan cung gia). */
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
void Led_SetStatus(safe_state_t st, fault_code_t c, uint8_t f){ (void)st;(void)c;(void)f; }

static int fails;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } }while(0)

static void run(uint32_t ms){ for(uint32_t i=0;i<ms;i++){ now_ms++; for(int c=0;c<4;c++) devs[c].meas.timestamp_ms = now_ms; Safety_Task(now_ms);} }
static void clear_diag(void){ for(int c=0;c<4;c++) devs[c].diag_sticky = 0; }

static void power_up(void)
{
    CHECK(Safety_RequestPower(true),"request on");
    run(SAFE_OFF_MIN_MS + 5);   /* OFF -> ARMING */
    run(SAFE_ARM_MS + 5);       /* ARMING -> ON  */
}
static void trip_overcurrent(void){ devs[0].diag_sticky |= INA228_FLAG_SHNTOL; run(2); }

int main(void)
{
    for (int c=0;c<4;c++) devs[c].meas.timestamp_ms = 0;
    Safety_Init();
    CHECK(Safety_State()==SAFE_BOOT && !pwr_pin,"boot");
    run(SAFE_BOOT_MS + 5);
    CHECK(Safety_State()==SAFE_OFF,"-> OFF");
    power_up();
    CHECK(Safety_State()==SAFE_ON && pwr_pin,"-> ON (state=%d)",Safety_State());

    /* --- C1: CLR_FAULT khong reset bo dem; 3 trip lien tiep -> LOCKOUT --- */
    trip_overcurrent();
    CHECK(Safety_State()==SAFE_TRIPPED && Safety_Code()==FCODE_OVERCUR && !pwr_pin,"trip 1");
    CHECK(!Safety_RequestPower(true),"TRIPPED phai tu choi bat");
    Safety_ClearFault(); clear_diag();
    CHECK(Safety_State()==SAFE_OFF && Safety_TripCount()==1,"CLR_FAULT khong duoc xoa trip_count (=%u)",Safety_TripCount());
    power_up(); trip_overcurrent(); Safety_ClearFault(); clear_diag();
    CHECK(Safety_TripCount()==2,"trip 2 count=%u",Safety_TripCount());
    power_up(); trip_overcurrent();
    CHECK(Safety_State()==SAFE_LOCKOUT,"trip 3 -> LOCKOUT (state=%d)",Safety_State());
    CHECK(Safety_Code()==FCODE_OVERCUR,"LOCKOUT phai giu ma loi (=%d)",Safety_Code());
    uint8_t mask = Safety_TripMask();
    Safety_ClearFault(); run(5);
    CHECK(Safety_State()==SAFE_LOCKOUT && Safety_Code()==FCODE_OVERCUR && Safety_TripMask()==mask,"CLR_FAULT trong LOCKOUT phai bi bo qua");
    CHECK(!Safety_RequestPower(true),"LOCKOUT tu choi bat");
    run(1000); CHECK(!pwr_pin && Safety_State()==SAFE_LOCKOUT,"LOCKOUT giu tat nguon");

    /* --- C2: lan trip thu 3 qua duong nhanh (ISR): ma loi phai duoc phan loai trong LOCKOUT --- */
    memset(devs,0,sizeof devs); dcm=0;
    Safety_Init(); run(SAFE_BOOT_MS + 5);
    for (int n=1; n<=3; n++) {
        power_up();
        CHECK(Safety_State()==SAFE_ON,"n=%d len ON",n);
        Safety_FastTrip();
        CHECK(!pwr_pin,"n=%d FastTrip cat ngay",n);
        run(2);
        if (n < 3) {
            CHECK(Safety_State()==SAFE_TRIPPED && Safety_Code()==FCODE_NONE,"n=%d TRIPPED chua co ma",n);
            run(SAFE_SETTLE_MS + 5);
            CHECK(Safety_Code()==FCODE_BUS,"n=%d khong co diag -> BUS (=%d)",n,Safety_Code());
            Safety_ClearFault();
        }
    }
    CHECK(Safety_State()==SAFE_LOCKOUT,"fast trip 3 -> LOCKOUT");
    CHECK(Safety_Code()==FCODE_NONE,"truoc settle chua co ma");
    /* CLR_FAULT chen vao trong cua so phan loai: bi bo qua */
    Safety_ClearFault();
    CHECK(Safety_State()==SAFE_LOCKOUT,"CLR_FAULT trong cua so phan loai khong duoc thoat LOCKOUT");
    devs[2].diag_sticky |= INA228_FLAG_BUSOL;             /* diag den muon, sau trip */
    run(SAFE_SETTLE_MS + 5);
    CHECK(Safety_Code()==FCODE_OVERVOLT && (Safety_TripMask() & 0x4u),"LOCKOUT phai phan loai ma loi tu diag (=%d mask=%u)",Safety_Code(),Safety_TripMask());

    /* --- CLR_FAULT chen giua vong khi vua trip lan 3 (TRIPPED, Task chua chay sang LOCKOUT) --- */
    memset(devs,0,sizeof devs);
    Safety_Init(); run(SAFE_BOOT_MS + 5);
    for (int n=1;n<=2;n++){ power_up(); trip_overcurrent(); Safety_ClearFault(); clear_diag(); }
    power_up(); devs[0].diag_sticky |= INA228_FLAG_SHNTOL;
    now_ms++; Safety_Task(now_ms);                         /* ON -> TRIPPED (count=3), chua sang LOCKOUT */
    CHECK(Safety_State()==SAFE_TRIPPED && Safety_TripCount()==3,"vua trip lan 3 (state=%d cnt=%u)",Safety_State(),Safety_TripCount());
    Safety_ClearFault();
    CHECK(Safety_State()==SAFE_LOCKOUT,"CLR_FAULT chen giua khong duoc ve OFF (state=%d)",Safety_State());

    /* --- Chay on dinh >= SAFE_STABLE_MS thi bo dem reset: khong lockout --- */
    memset(devs,0,sizeof devs);
    Safety_Init(); run(SAFE_BOOT_MS + 5);
    for (int n=1;n<=2;n++){ power_up(); trip_overcurrent(); Safety_ClearFault(); clear_diag(); }
    power_up(); run(SAFE_STABLE_MS + 50);
    CHECK(Safety_State()==SAFE_ON && Safety_TripCount()==0,"on dinh phai reset bo dem (cnt=%u)",Safety_TripCount());
    trip_overcurrent();
    CHECK(Safety_State()==SAFE_TRIPPED && Safety_TripCount()==1,"sau on dinh chi tinh la trip 1");


    /* ====== Trip code: so sanh nguong tren STM, latch cho den Clear ====== */
    memset(devs,0,sizeof devs); dcm=0;
    Safety_Init(); run(SAFE_BOOT_MS + 5);
    { uint8_t thr[PSU_SUP_THR_LEN]; memset(thr,0,sizeof thr);
      thr[3*6+0]=0x60; /* V_5V MAX = 0x600000 (k=3) */  PsuTrip_SetThresholds(thr); }
    for (int c=0;c<4;c++) devs[c].raw.vbus_reg24 = 0x500000;
    power_up();
    CHECK(Safety_State()==SAFE_ON && !PsuTrip_Any(), "ON, trong nguong");
    devs[1].raw.vbus_reg24 = 0x610000;                      /* INA2 (5V) vuot MAX */
    run(5);
    CHECK(Safety_State()==SAFE_TRIPPED && Safety_Code()==FCODE_THRESHOLD, "vuot nguong -> TRIPPED, ma THRESHOLD (state=%d code=%d)",Safety_State(),Safety_Code());
    CHECK(PsuTrip_Bytes()[PSU_TRIP_B_SUP_MAX]==(1u<<3) && PsuTrip_Bytes()[PSU_TRIP_B_SUP_MIN]==0, "bit V_5V MAX");
    devs[1].raw.vbus_reg24 = 0x500000; run(2000);
    CHECK(PsuTrip_Bytes()[PSU_TRIP_B_SUP_MAX]==(1u<<3), "trip giu nguyen khi gia tri tro lai binh thuong, cho den Clear");
    Safety_Control(0x01, false);
    CHECK(Safety_State()==SAFE_TRIPPED, "dang TRIPPED: lenh bat bi tu choi");
    Safety_Control(0x01, true);                             /* Clear + bat trong cung khung */
    CHECK(Safety_State()==SAFE_OFF && !PsuTrip_Any(), "Clear xoa trip, ve OFF");
    run(SAFE_OFF_MIN_MS + 50);
    CHECK(Safety_State()==SAFE_OFF && !pwr_pin, "lenh bat cung khung voi Clear bi bo qua");
    Safety_Control(0x01, false);
    run(SAFE_OFF_MIN_MS + 5); run(SAFE_ARM_MS + 5);
    CHECK(Safety_State()==SAFE_ON && pwr_pin, "bat lai o khung sau");
    Safety_Control(0x01, true);                             /* Clear khi dang ON, mask bat: giu nguyen ON */
    run(5);
    CHECK(Safety_State()==SAFE_ON && pwr_pin, "Clear khi dang ON khong tat nguon");
    Safety_Control(0x00, false); run(5);
    CHECK(Safety_State()==SAFE_OFF && !pwr_pin, "mask=0 -> tat");
    Safety_Control(0x0E, false);                            /* bit 1..3 bo qua (chua co cum 2..4) */
    run(SAFE_OFF_MIN_MS + 5);
    CHECK(Safety_State()==SAFE_OFF, "chi bit0 dieu khien PWR_EN");

    /* Duoi MIN chi tinh khi ON: luc OFF/ARMING V ~ 0 khong trip */
    memset(devs,0,sizeof devs); Safety_Init(); run(SAFE_BOOT_MS + 5);
    { uint8_t thr[PSU_SUP_THR_LEN]; memset(thr,0,sizeof thr); thr[1*6+5]=0x40; /* V_DCM MIN = 0x000040? dat 0x004000 */ thr[1*6+4]=0x40; thr[1*6+5]=0; PsuTrip_SetThresholds(thr); }
    for (int c=0;c<4;c++) devs[c].raw.vbus_reg24 = 0;       /* chua co dien ap */
    CHECK(Safety_RequestPower(true),"bat");
    run(SAFE_OFF_MIN_MS + 5); run(SAFE_ARM_MS - 50);
    CHECK(Safety_State()==SAFE_ARMING && !PsuTrip_Any(), "ARMING: V thap khong trip");
    for (int c=0;c<4;c++) devs[c].raw.vbus_reg24 = 0x500000;
    run(100);
    CHECK(Safety_State()==SAFE_ON && !PsuTrip_Any(), "ON binh thuong");
    devs[0].raw.vbus_reg24 = 0x001000; run(5);
    CHECK(Safety_State()==SAFE_TRIPPED && (PsuTrip_Bytes()[PSU_TRIP_B_SUP_MIN] & 0x02) , "ON: V_DCM duoi MIN -> trip");

    /* Co phan cung INA -> bit trip; LOCKOUT giu trip code */
    memset(devs,0,sizeof devs); { uint8_t z[PSU_SUP_THR_LEN]; memset(z,0,sizeof z); PsuTrip_SetThresholds(z); }
    Safety_Init(); run(SAFE_BOOT_MS + 5);
    power_up(); devs[2].diag_sticky |= INA228_FLAG_SHNTOL | INA228_FLAG_BUSOL; run(3);
    CHECK(Safety_Code()==FCODE_OVERCUR && (PsuTrip_Bytes()[PSU_TRIP_B_INA_OC] & 0x04) && (PsuTrip_Bytes()[PSU_TRIP_B_INA_V] & 0x04), "co INA kenh 3 -> TRIP3/TRIP4 bit 2");
    Safety_ClearFault(); clear_diag();
    CHECK(!PsuTrip_Any(), "Clear xoa bit INA");
    power_up(); devs[0].diag_sticky |= INA228_FLAG_SHNTOL; run(3); Safety_ClearFault(); clear_diag();
    power_up(); devs[0].diag_sticky |= INA228_FLAG_SHNTOL; run(3);
    CHECK(Safety_State()==SAFE_LOCKOUT, "lockout");
    uint8_t keep[PSU_TRIP_BYTES]; memcpy(keep, PsuTrip_Bytes(), sizeof keep);
    CHECK(keep[PSU_TRIP_B_MISC] & PSU_TRIP_MISC_LOCKOUT, "LOCKOUT co bit MISC");
    Safety_Control(0x01, true); run(5);
    CHECK(memcmp(keep, PsuTrip_Bytes(), sizeof keep)==0 && Safety_State()==SAFE_LOCKOUT, "Clear khong xoa trip code trong LOCKOUT");

    /* Chan FT cua DCM -> bit TRIP5 cao */
    memset(devs,0,sizeof devs); Safety_Init(); run(SAFE_BOOT_MS + 5); power_up();
    dcm = 0x02; run(3);
    CHECK(Safety_Code()==FCODE_DCM && (PsuTrip_Bytes()[PSU_TRIP_B_TEMP_FT] & 0x20), "FT cua DCM 2 -> bit 5 cua TRIP5");
    dcm = 0;

    /* --- Safety_ForceOff: ghi BSRR reset bit, khong phu thuoc state --- */
    fake_gpiob.BSRR = 0;
    Safety_ForceOff();
    CHECK(fake_gpiob.BSRR == ((uint32_t)PWR_EN_CTRL_Pin << 16),"ForceOff BSRR=%08x",fake_gpiob.BSRR);

    printf("safety tests done, fails=%d\n", fails);
    return fails ? 1 : 0;
}
