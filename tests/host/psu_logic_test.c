/* Test logic PSU thuan: so sanh nguong + trip latch, ban tin giam sat, anh cau hinh. */
#include <stdio.h>
#include <string.h>
#include "flash_sim.h"
#include "cfg_store.h"
#include "psu_frame.h"
#include "psu_trip.h"
#include "psu_monitor.h"
#include "psu_cfg.h"
#include <ina228_cfg.h>

static int fails;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } }while(0)

static void put24(uint8_t *d, uint32_t v){ d[0]=v>>16; d[1]=v>>8; d[2]=v; }

/* ---- so sanh nguong ---- */
static uint32_t stamp;
static psu_trip_in_t in_make(int32_t cur, uint32_t vbus)    /* 4 kenh cung gia tri */
{
    psu_trip_in_t in; memset(&in,0,sizeof in); stamp += 60;
    for (int c=0;c<4;c++){ in.cur24[c]=(uint32_t)cur & 0xFFFFFFu; in.vbus24[c]=vbus; in.fresh[c]=true; in.stamp[c]=stamp; }
    return in;
}
static void set_thr(uint8_t k, uint32_t mx, uint32_t mn){ uint8_t t[PSU_SUP_THR_LEN]; memset(t,0,sizeof t); put24(&t[k*6],mx); put24(&t[k*6+3],mn); PsuTrip_SetThresholds(t); }

static void test_trip(void)
{
    PsuTrip_Init();
    /* nguong V_5V (k=3, kenh INA2): MAX 0x600000, MIN 0x400000; khong nguong khac */
    set_thr(3, 0x600000, 0x400000);
    psu_trip_in_t in = in_make(0, 0x500000);
    CHECK(!PsuTrip_Evaluate(true,&in) && !PsuTrip_Any(), "trong nguong khong trip");

    in = in_make(0, 0x610000);
    CHECK(!PsuTrip_Evaluate(true,&in), "mau vuot dau tien chua latch (N=%u)", PSU_TRIP_CONFIRM_N);
    CHECK(!PsuTrip_Any(), "chua latch");
    in = in_make(0, 0x500000);   /* quay ve trong nguong -> reset chuoi */
    PsuTrip_Evaluate(true,&in);
    in = in_make(0, 0x610000); PsuTrip_Evaluate(true,&in);
    CHECK(!PsuTrip_Any(), "chuoi bi ngat thi dem lai");
    in = in_make(0, 0x610000);
    CHECK(PsuTrip_Evaluate(true,&in), "N mau lien tiep -> latch");
    CHECK(PsuTrip_Bytes()[PSU_TRIP_B_SUP_MAX]==(1u<<3) && PsuTrip_Bytes()[PSU_TRIP_B_SUP_MIN]==0, "bit MAX cua V_5V (k=3)");

    /* giu nguyen cho den Clear, ke ca khi gia tri ve binh thuong */
    in = in_make(0, 0x500000); PsuTrip_Evaluate(true,&in); PsuTrip_Evaluate(true,&in);
    CHECK(PsuTrip_Bytes()[PSU_TRIP_B_SUP_MAX]==(1u<<3), "trip giu nguyen khi gia tri tro lai binh thuong");
    PsuTrip_Clear();
    CHECK(!PsuTrip_Any(), "Clear xoa trip");

    /* MIN */
    in = in_make(0, 0x300000); PsuTrip_Evaluate(true,&in);
    in = in_make(0, 0x300000);
    CHECK(PsuTrip_Evaluate(true,&in) && PsuTrip_Bytes()[PSU_TRIP_B_SUP_MIN]==(1u<<3), "duoi MIN -> bit MIN");
    PsuTrip_Clear();

    /* khong armed: khong so sanh va reset chuoi (V ~ 0 luc OFF/ARMING khong bi tinh la duoi MIN) */
    in = in_make(0, 0); PsuTrip_Evaluate(false,&in); in = in_make(0, 0);
    CHECK(!PsuTrip_Evaluate(false,&in) && !PsuTrip_Any(), "khong armed khong trip");
    in = in_make(0, 0x300000); PsuTrip_Evaluate(false,&in);          /* mau 1 luc chua armed */
    in = in_make(0, 0x300000); CHECK(!PsuTrip_Evaluate(true,&in), "vao armed phai dem N mau moi (khong tinh mau cu)");

    /* cung mot mau (stamp khong doi) khong duoc dem 2 lan */
    PsuTrip_Clear();
    in = in_make(0, 0x700000);
    PsuTrip_Evaluate(true,&in);
    CHECK(!PsuTrip_Evaluate(true,&in), "mau khong doi (stamp) khong tinh them");

    /* kenh khong tuoi: khong so sanh */
    PsuTrip_Clear();
    in = in_make(0, 0x700000); in.fresh[1] = false; PsuTrip_Evaluate(true,&in); in = in_make(0,0x700000); in.fresh[1]=false;
    CHECK(!PsuTrip_Evaluate(true,&in), "kenh khong tuoi khong trip");

    /* dong co dau: I_DCM (k=0) MAX = +0x100000, MIN = -0x100000 (0xF00000) */
    PsuTrip_Init(); set_thr(0, 0x100000, 0xF00000);
    in = in_make(0x080000, 0); PsuTrip_Evaluate(true,&in); in = in_make(0x080000, 0);
    CHECK(!PsuTrip_Evaluate(true,&in), "dong trong [-,+] khong trip");
    in = in_make(-0x180000, 0); PsuTrip_Evaluate(true,&in); in = in_make(-0x180000, 0);
    CHECK(PsuTrip_Evaluate(true,&in) && PsuTrip_Bytes()[PSU_TRIP_B_SUP_MIN]==1u, "dong am duoi MIN co dau -> trip MIN");
    PsuTrip_Clear();
    in = in_make(0x180000, 0); PsuTrip_Evaluate(true,&in); in = in_make(0x180000, 0);
    CHECK(PsuTrip_Evaluate(true,&in) && PsuTrip_Bytes()[PSU_TRIP_B_SUP_MAX]==1u, "dong duong tren MAX -> trip MAX");

    /* nguong 0 = tat */
    PsuTrip_Init(); set_thr(0, 0, 0);
    in = in_make(0x7FFFF0, 0xFFFFF0); PsuTrip_Evaluate(true,&in); in = in_make(0x7FFFF0, 0xFFFFF0);
    CHECK(!PsuTrip_Evaluate(true,&in) && !PsuTrip_Any(), "nguong 0 = tat");

    /* nguon trip khac + giu nguyen */
    PsuTrip_Init();
    CHECK(PsuTrip_Set(PSU_TRIP_B_INA_OC, 0x04), "bit moi");
    CHECK(!PsuTrip_Set(PSU_TRIP_B_INA_OC, 0x04), "bit da co");
    CHECK(PsuTrip_Bytes()[PSU_TRIP_B_INA_OC]==0x04 && PsuTrip_Any(), "set bit");
    CHECK(!PsuTrip_Set(PSU_TRIP_BYTES, 1), "byte ngoai pham vi");
    PsuTrip_Clear(); CHECK(!PsuTrip_Any(), "clear");
}

/* ---- ban tin giam sat ---- */
static void test_monitor(void)
{
    psu_mon_t m; memset(&m,0,sizeof m);
    uint8_t trip[PSU_TRIP_BYTES]; for (int i=0;i<10;i++) trip[i]=(uint8_t)(0xA0+i);
    for (int c=0;c<4;c++){ m.cur24[c]=0x112233u+c; m.vbus24[c]=0x445566u+c; }
    m.fresh_mask=0x0D; m.safe_state=3; m.trip_count=2; m.pwr_on=true; m.slot='B'; m.version=0xAABBCCDD; m.trip=trip;

    uint8_t f[PSU_LEN_MONITOR]; PsuMon_Build(f,&m);
    CHECK(f[0]==0xAB && f[1]==0xCD && f[2]==0x81 && f[3]==0x81 && f[4]==PSU_ADDR, "header 81 81 + dia chi");
    CHECK(Psu_FrameValid(f,PSU_LEN_MONITOR), "CRC/duoi hop le");
    /* vi tri theo tai lieu (1-based): Supply 198..221, RTC 222..228, Trip 229..238, Du phong 239..262, CRC 263-264 */
    CHECK(f[197]==0x11 && f[198]==0x22 && f[199]==0x33 && f[200]==0x44 && f[201]==0x55 && f[202]==0x66, "INA1 -> I_DCM, V_DCM (byte 198..203)");
    CHECK(f[203]==0 && f[208]==0, "INA2 khong tuoi -> 0");
    CHECK(f[209]==0x11 && f[210]==0x22 && f[211]==0x35 && f[212]==0x44 && f[214]==0x68, "INA3 -> I_3V3_1 (byte 210..)");
    CHECK(f[215]==0x11 && f[216]==0x22 && f[217]==0x36 && f[218]==0x44 && f[219]==0x55 && f[220]==0x69, "INA4 tuoi -> I_3V3_2, V_3V3_2 (byte 216..221)");
    int zero=1; for (int i=5;i<197;i++) if (f[i]) zero=0; CHECK(zero, "4 cum DCM = 0 (byte 6..197)");
    zero=1; for (int i=221;i<228;i++) if (f[i]) zero=0; CHECK(zero, "RTC = 0");
    CHECK(memcmp(&f[228], trip, 10)==0, "10 trip byte o 229..238");
    /* vung du phong 24 byte (byte 239..262): chi giu safe_state, PWR_EN, trip count, slot, version */
    CHECK(f[238]==3 && f[239]==1 && f[240]==2 && f[241]=='B', "du phong: safe_state, PWR_EN, trip count, slot");
    CHECK(f[242]==0xAA && f[243]==0xBB && f[244]==0xCC && f[245]==0xDD, "du phong: version BE");
    zero=1; for (int i=246;i<262;i++) if (f[i]) zero=0; CHECK(zero, "phan con lai cua vung du phong = 0");
    CHECK(f[262]*256+f[263] == Psu_Crc16(f,PSU_LEN_MONITOR) && f[264]==0xE1 && f[265]==0xE2, "CRC + duoi o cuoi");

    m.pwr_on=false; m.safe_state=1; PsuMon_Build(f,&m);
    CHECK(f[238]==1 && f[239]==0, "PWR_EN tat, safe_state OFF");
}

/* ---- anh cau hinh ---- */
static void test_cfg(void)
{
    memset(sim,0xFF,sizeof sim); CfgStore_Init(); PsuTrip_Init();
    PsuCfg_Load();
    CHECK(!PsuCfg_IsStored(), "chua luu -> mac dinh");
    uint16_t r[PSU_INA_REGS]; PsuCfg_InaRegs(0,r);
    CHECK(r[INAR_CONFIG]==INA228_CONFIG_VALUE && r[INAR_ADC_CONFIG]==INA228_ADC_CONFIG_VALUE && r[INAR_SHUNT_CAL]==INA228_SHUNT_CAL_VALUE, "INA mac dinh");
    CHECK(r[INAR_SOVL]==INA228_SOVL_DISABLED && r[INAR_BUVL]==INA228_BUVL_DISABLED && r[INAR_SUVL]==INA228_SUVL_DISABLED, "nguong cung INA mac dinh = tat");
    const uint8_t *b = PsuCfg_Blob();
    CHECK(b[PSU_CFG_OFF_INA]==0x00 && b[PSU_CFG_OFF_INA+1]==0x10, "blob mac dinh co CONFIG INA1 = 0x0010");

    uint8_t w[PSU_CFG_LEN]; for (unsigned i=0;i<sizeof w;i++) w[i]=(uint8_t)(i*13+5);
    /* INA2 hop le (SHUNT_CAL != 0), INA3 SHUNT_CAL = 0 (-> mac dinh); CONFIG co bit RST */
    uint8_t *ina2 = &w[PSU_CFG_OFF_INA + 18]; ina2[0]=0x80; ina2[1]=0x10; ina2[4]=0x20; ina2[5]=0x00;
    uint8_t *ina3 = &w[PSU_CFG_OFF_INA + 36]; ina3[4]=0; ina3[5]=0;
    CHECK(PsuCfg_Store(w), "luu cau hinh");
    CHECK(PsuCfg_IsStored() && PsuCfg_ConsumeDirty() && !PsuCfg_ConsumeDirty(), "dirty 1 lan");
    CHECK(memcmp(PsuCfg_Blob(), w, PSU_CFG_LEN)==0, "tra lai dung nhu da ghi (de PC doc lai so sanh)");

    PsuCfg_InaRegs(1,r);
    CHECK(r[INAR_SHUNT_CAL]==0x2000 && (r[INAR_CONFIG]&0xC000)==0, "INA2 dung gia tri PC, bo bit RST/RSTACC");
    PsuCfg_InaRegs(2,r);
    CHECK(r[INAR_SHUNT_CAL]==INA228_SHUNT_CAL_VALUE && r[INAR_ADC_CONFIG]==INA228_ADC_CONFIG_VALUE, "SHUNT_CAL=0 -> mac dinh cho ca INA");

    /* khoi dong lai: doc tu Flash */
    CfgStore_Init(); PsuCfg_Load();
    CHECK(PsuCfg_IsStored() && memcmp(PsuCfg_Blob(), w, PSU_CFG_LEN)==0, "nap lai tu Flash sau reset");
    CHECK(!PsuCfg_ConsumeDirty(), "nap luc khoi dong khong dirty");

    /* nguong Supply duoc dua vao so sanh */
    uint8_t w2[PSU_CFG_LEN]; memset(w2,0,sizeof w2);
    put24(&w2[PSU_CFG_OFF_SUPPLY + 0], 0x000100);          /* I_DCM MAX */
    PsuCfg_BuildDefault(w2 + 0);  /* ghi de INA mac dinh vao w2 */
    put24(&w2[PSU_CFG_OFF_SUPPLY + 0], 0x000100);
    CHECK(PsuCfg_Store(w2), "luu cau hinh 2");
    psu_trip_in_t in = in_make(0x000200, 0); PsuTrip_Evaluate(true,&in); in = in_make(0x000200,0);
    CHECK(PsuTrip_Evaluate(true,&in) && PsuTrip_Bytes()[PSU_TRIP_B_SUP_MAX]==1, "nguong luu trong cau hinh duoc dung de so sanh");
}

int main(void)
{
    test_trip();
    test_monitor();
    test_cfg();
    printf("psu_logic tests done, fails=%d\n", fails);
    return fails ? 1 : 0;
}
