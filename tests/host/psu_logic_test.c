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

/* ---- so sanh nguong (cum 1: 3 DCM x {dong, nhiet do}) ---- */
static uint32_t stamp;
static psu_trip_in_t in_make(int32_t cur, int16_t temp)    /* 3 DCM cung gia tri */
{
    psu_trip_in_t in; memset(&in,0,sizeof in); stamp += 60;
    for (int n=0;n<3;n++){ in.cur24[n]=(uint32_t)cur & 0xFFFFFFu; in.temp[n]=temp; in.fresh[n]=true; in.stamp[n]=stamp; }
    return in;
}
/* dat 1 nguong cua cum 1: k = 0 I1, 1 T1, 2 I2, 3 T2, 4 I3, 5 T3 */
static void set_thr(uint8_t k, uint32_t mx, uint32_t mn)
{
    uint8_t cfg[4*PSU_TRIP_CLUSTER_STRIDE]; memset(cfg,0,sizeof cfg);
    put24(&cfg[k*6],mx); put24(&cfg[k*6+3],mn); PsuTrip_SetThresholds(cfg);
}

static void test_trip(void)
{
    PsuTrip_Init();
    /* nguong I_OUT_ADC2 (k=2): MAX 0x600000, MIN 0x400000 */
    set_thr(2, 0x600000, 0x400000);
    psu_trip_in_t in = in_make(0x500000, 0);
    CHECK(!PsuTrip_Evaluate(0,true,&in) && !PsuTrip_Any(), "trong nguong khong trip");

    in = in_make(0x610000, 0);
    CHECK(!PsuTrip_Evaluate(0,true,&in), "mau vuot dau tien chua latch (N=%u)", PSU_TRIP_CONFIRM_N);
    CHECK(!PsuTrip_Any(), "chua latch");
    in = in_make(0x500000, 0); PsuTrip_Evaluate(0,true,&in);   /* quay ve trong nguong -> reset chuoi */
    in = in_make(0x610000, 0); PsuTrip_Evaluate(0,true,&in);
    CHECK(!PsuTrip_Any(), "chuoi bi ngat thi dem lai");
    in = in_make(0x610000, 0);
    CHECK(PsuTrip_Evaluate(0,true,&in), "N mau lien tiep -> latch");
    CHECK(PsuTrip_Bytes()[PSU_TRIP_B_SW_MAX]==(1u<<1) && PsuTrip_Bytes()[PSU_TRIP_B_SW_MIN]==0, "bit MAX cua I_OUT_ADC2 (bit 1)");

    /* giu nguyen cho den Clear, ke ca khi gia tri ve binh thuong */
    in = in_make(0x500000, 0); PsuTrip_Evaluate(0,true,&in); PsuTrip_Evaluate(0,true,&in);
    CHECK(PsuTrip_Bytes()[PSU_TRIP_B_SW_MAX]==(1u<<1), "trip giu nguyen khi gia tri tro lai binh thuong");
    PsuTrip_Clear();
    CHECK(!PsuTrip_Any(), "Clear xoa trip");

    /* MIN */
    in = in_make(0x300000, 0); PsuTrip_Evaluate(0,true,&in);
    in = in_make(0x300000, 0);
    CHECK(PsuTrip_Evaluate(0,true,&in) && PsuTrip_Bytes()[PSU_TRIP_B_SW_MIN]==(1u<<1), "duoi MIN -> bit MIN");
    PsuTrip_Clear();

    /* khong armed: khong so sanh va reset chuoi (gia tri ~ 0 luc OFF/ARMING khong bi tinh la duoi MIN) */
    in = in_make(0, 0); PsuTrip_Evaluate(0,false,&in); in = in_make(0, 0);
    CHECK(!PsuTrip_Evaluate(0,false,&in) && !PsuTrip_Any(), "khong armed khong trip");
    in = in_make(0x300000, 0); PsuTrip_Evaluate(0,false,&in);          /* mau 1 luc chua armed */
    in = in_make(0x300000, 0); CHECK(!PsuTrip_Evaluate(0,true,&in), "vao armed phai dem N mau moi (khong tinh mau cu)");

    /* cung mot mau (stamp khong doi) khong duoc dem 2 lan */
    PsuTrip_Clear();
    in = in_make(0x700000, 0);
    PsuTrip_Evaluate(0,true,&in);
    CHECK(!PsuTrip_Evaluate(0,true,&in), "mau khong doi (stamp) khong tinh them");

    /* DCM khong tuoi: khong so sanh */
    PsuTrip_Clear();
    in = in_make(0x700000, 0); in.fresh[1] = false; PsuTrip_Evaluate(0,true,&in); in = in_make(0x700000,0); in.fresh[1]=false;
    CHECK(!PsuTrip_Evaluate(0,true,&in), "DCM khong tuoi khong trip");

    /* dong co dau: I_OUT_ADC1 (k=0) MAX = +0x100000, MIN = -0x100000 (0xF00000) */
    PsuTrip_Init(); set_thr(0, 0x100000, 0xF00000);
    in = in_make(0x080000, 0); PsuTrip_Evaluate(0,true,&in); in = in_make(0x080000, 0);
    CHECK(!PsuTrip_Evaluate(0,true,&in), "dong trong [-,+] khong trip");
    in = in_make(-0x180000, 0); PsuTrip_Evaluate(0,true,&in); in = in_make(-0x180000, 0);
    CHECK(PsuTrip_Evaluate(0,true,&in) && PsuTrip_Bytes()[PSU_TRIP_B_SW_MIN]==1u, "dong am duoi MIN co dau -> trip MIN");
    PsuTrip_Clear();
    in = in_make(0x180000, 0); PsuTrip_Evaluate(0,true,&in); in = in_make(0x180000, 0);
    CHECK(PsuTrip_Evaluate(0,true,&in) && PsuTrip_Bytes()[PSU_TRIP_B_SW_MAX]==1u, "dong duong tren MAX -> trip MAX");

    /* nhiet do co dau 16 bit: TEMP_ADC3 (k=5): MAX 0x000C80 (100 C), MIN 0x00FF38 (-10 C) */
    PsuTrip_Init(); set_thr(5, 0x000C80, 0x00FF38);
    in = in_make(0, 0x0400); PsuTrip_Evaluate(0,true,&in); in = in_make(0, 0x0400);
    CHECK(!PsuTrip_Evaluate(0,true,&in), "nhiet do trong nguong");
    in = in_make(0, 0x0D00); PsuTrip_Evaluate(0,true,&in); in = in_make(0, 0x0D00);
    CHECK(PsuTrip_Evaluate(0,true,&in) && PsuTrip_Bytes()[PSU_TRIP_B_SW_MAX]==(1u<<(3+2)), "TEMP_ADC3 vuot MAX -> bit 5");
    PsuTrip_Clear();
    in = in_make(0, (int16_t)0xFE00); PsuTrip_Evaluate(0,true,&in); in = in_make(0, (int16_t)0xFE00);
    CHECK(PsuTrip_Evaluate(0,true,&in) && PsuTrip_Bytes()[PSU_TRIP_B_SW_MIN]==(1u<<(3+2)), "TEMP_ADC3 duoi MIN (am) -> bit 5");

    /* nguong 0 = tat */
    PsuTrip_Init(); set_thr(0, 0, 0);
    in = in_make(0x7FFFF0, 0x7FFF); PsuTrip_Evaluate(0,true,&in); in = in_make(0x7FFFF0, 0x7FFF);
    CHECK(!PsuTrip_Evaluate(0,true,&in) && !PsuTrip_Any(), "nguong 0 = tat");

    /* cum 2..4: chua co bang bit -> khong trip */
    PsuTrip_Init(); { uint8_t cfg[4*PSU_TRIP_CLUSTER_STRIDE]; memset(cfg,0,sizeof cfg); put24(&cfg[PSU_TRIP_CLUSTER_STRIDE],0x000001); PsuTrip_SetThresholds(cfg); }
    in = in_make(0x7FFFF0, 0); PsuTrip_Evaluate(1,true,&in); in = in_make(0x7FFFF0, 0);
    CHECK(!PsuTrip_Evaluate(1,true,&in) && !PsuTrip_Any(), "cum 2 chua gan bit trip");

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
    for (int n=0;n<3;n++){ m.cur24[n]=0x112233u+n; m.temp[n]=(int16_t)(0x0A00+n); }
    m.temp[2] = (int16_t)0xFF38;                       /* -200 (am) */
    m.fresh_mask=0x05;                                 /* DCM1 va DCM3 tuoi, DCM2 mat du lieu */
    m.safe_state[0]=3; m.pwr_on[0]=true;  m.trip_count[0]=2;
    m.safe_state[1]=1; m.pwr_on[1]=false; m.trip_count[1]=0;
    m.safe_state[3]=4; m.pwr_on[3]=false; m.trip_count[3]=7;
    m.slot='B'; m.version=0xAABBCCDD; m.trip=trip;

    uint8_t f[PSU_LEN_MONITOR]; PsuMon_Build(f,&m);
    CHECK(f[0]==0xAB && f[1]==0xCD && f[2]==0x81 && f[3]==0x81 && f[4]==PSU_ADDR, "header 81 81 + dia chi");
    CHECK(Psu_FrameValid(f,PSU_LEN_MONITOR), "CRC/duoi hop le");
    /* Cum 1 (tai lieu, 1-based): I_OUT_ADC1 6-8, TEMP_ADC1 9-11, I_OUT_ADC2 12-14, TEMP_ADC2 15-17, I_OUT_ADC3 18-20, TEMP_ADC3 21-23 */
    CHECK(f[5]==0x11 && f[6]==0x22 && f[7]==0x33 && f[8]==0x00 && f[9]==0x0A && f[10]==0x00, "DCM1: I_OUT_ADC1, TEMP_ADC1 (byte 6..11)");
    CHECK(f[11]==0 && f[12]==0 && f[13]==0 && f[14]==0 && f[15]==0 && f[16]==0, "DCM2 khong tuoi -> 0");
    CHECK(f[17]==0x11 && f[18]==0x22 && f[19]==0x35 && f[20]==0x00 && f[21]==0xFF && f[22]==0x38, "DCM3: I_OUT_ADC3, TEMP_ADC3 am (byte 18..23)");
    int zero=1; for (int i=23;i<221;i++) if (f[i]) zero=0; CHECK(zero, "phan con lai cua cum 1, cum 2..4 va Supply = 0 (byte 24..221)");
    zero=1; for (int i=221;i<228;i++) if (f[i]) zero=0; CHECK(zero, "RTC = 0");
    CHECK(memcmp(&f[228], trip, 10)==0, "10 trip byte o 229..238");
    /* vung du phong 24 byte (byte 239..262): 4 x {safe_state, PWR_EN, trip count}, slot, version */
    CHECK(f[238]==3 && f[239]==1 && f[240]==2, "cum 1: ON, PWR_EN, trip count 2");
    CHECK(f[241]==1 && f[242]==0 && f[243]==0, "cum 2: OFF");
    CHECK(f[244]==0 && f[245]==0 && f[246]==0, "cum 3: BOOT(0) mac dinh trong mau");
    CHECK(f[247]==4 && f[248]==0 && f[249]==7, "cum 4: TRIPPED, trip count 7");
    CHECK(f[250]=='B' && f[251]==0xAA && f[252]==0xBB && f[253]==0xCC && f[254]==0xDD, "slot, version BE");
    zero=1; for (int i=255;i<262;i++) if (f[i]) zero=0; CHECK(zero, "phan con lai cua vung du phong = 0");
    CHECK(f[262]*256+f[263] == Psu_Crc16(f,PSU_LEN_MONITOR) && f[264]==0xE1 && f[265]==0xE2, "CRC + duoi o cuoi");
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

    /* nguong cum 1 trong cau hinh duoc dua vao so sanh: I_OUT_ADC1 MAX (offset 0) */
    uint8_t w2[PSU_CFG_LEN]; PsuCfg_BuildDefault(w2);
    put24(&w2[0], 0x000100);
    CHECK(PsuCfg_Store(w2), "luu cau hinh 2");
    psu_trip_in_t in = in_make(0x000200, 0); PsuTrip_Evaluate(0,true,&in); in = in_make(0x000200,0);
    CHECK(PsuTrip_Evaluate(0,true,&in) && PsuTrip_Bytes()[PSU_TRIP_B_SW_MAX]==1, "nguong luu trong cau hinh duoc dung de so sanh");
}

int main(void)
{
    test_trip();
    test_monitor();
    test_cfg();
    printf("psu_logic tests done, fails=%d\n", fails);
    return fails ? 1 : 0;
}
