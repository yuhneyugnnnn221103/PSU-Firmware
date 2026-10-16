/* Test may trang thai safety (safety.c that, phan cung gia). */
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "ina228_driver.h"
#include "safety.h"
#include "comm.h"
#include "led.h"
#include "fw_update.h"

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

static void run(uint32_t ms){ for(uint32_t i=0;i<ms;i++){ now_ms++; for(int c=0;c<4;c++) devs[c].last_ok_ms = now_ms; Safety_Task(now_ms);} }
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
    for (int c=0;c<4;c++) devs[c].last_ok_ms = 0;
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

    /* --- Safety_ForceOff: ghi BSRR reset bit, khong phu thuoc state --- */
    fake_gpiob.BSRR = 0;
    Safety_ForceOff();
    CHECK(fake_gpiob.BSRR == ((uint32_t)PWR_EN_CTRL_Pin << 16),"ForceOff BSRR=%08x",fake_gpiob.BSRR);

    printf("safety tests done, fails=%d\n", fails);
    return fails ? 1 : 0;
}
