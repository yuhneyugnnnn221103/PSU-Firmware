/* Test khoi dong app: app chi khoi dong khi anh hop le, chua bi danh dau vao recovery, chua het luot thu. */
#include <stdio.h>
#include <string.h>
#include "flash_sim.h"
#include "ota_select.h"
#include "cfg_store.h"
#include "crc32_sw.h"

static int fails;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } }while(0)

static uint8_t *simp(uint32_t a){ return sim + (a - 0x08000000u); }
#define APP OTA_SLOT_APP

/* Dung anh hop le truc tiep trong flash gia */
static void make_image(uint32_t seq, uint32_t ver)
{
    uint8_t code[600]; for (unsigned i=0;i<sizeof code;i++) code[i]=(uint8_t)(i*5+1);
    uint32_t msp=0x24050000u, rh=Ota_CodeBase(APP)+0x101u;
    memcpy(code,&msp,4); memcpy(code+4,&rh,4);
    ota_image_header_t h; memset(&h,0xFF,sizeof h);
    h.magic=OTA_IMG_MAGIC; h.version=ver; h.size=sizeof code; h.crc32=Crc32_Compute(code,sizeof code); h.install_seq=seq;
    memcpy(simp(g_ota_slots[APP].base),&h,sizeof h);
    memcpy(simp(Ota_CodeBase(APP)),code,sizeof code);
}
static void reset_all(void){ memset(sim,0xFF,sizeof sim); CfgStore_Init(); }
static void rec(uint32_t count, uint8_t conf){ CHECK(CfgStore_WriteBoot(APP,count,conf),"writeboot"); }

int main(void)
{
    /* 0. Bo cuc: mot app 640 KB, header 1 KB truoc code */
    CHECK(OTA_SLOT_COUNT==1 && OTA_SLOT_SIZE==0xA0000u && OTA_HEADER_SIZE==1024u, "bo cuc 1 app 640 KB");
    CHECK(g_ota_slots[APP].base==0x08020000u && Ota_CodeBase(APP)==0x08020400u && g_ota_slots[APP].tag=='A', "app o 0x08020000, code 0x08020400");
    CHECK(OTA_IMG_MAX_SIZE==0xA0000u-1024u, "kich thuoc anh toi da");

    /* 1. Khong co anh -> recovery. Co anh hop le, chua co record -> khoi dong */
    reset_all();
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_NONE,"flash trong -> NONE (vao recovery)");
    make_image(5,1);
    CHECK(Ota_SelectSlot(NULL)==APP,"anh hop le, chua co record -> chon app");

    /* 2. Trial: count 0..2 chua confirm van chon duoc; count 3 thi loai */
    for (uint32_t c=0;c<OTA_MAX_BOOT_ATTEMPTS;c++){ rec(c,0); CHECK(Ota_SelectSlot(NULL)==APP,"count=%u phai chon duoc",c); }
    rec(OTA_MAX_BOOT_ATTEMPTS,0);
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_NONE,"het luot (anh van hop le) -> KHONG chon lai, vao recovery");
    rec(OTA_MAX_BOOT_ATTEMPTS+4,0);
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_NONE,"van het luot");

    /* 3. Da confirm: count lon van chon duoc */
    reset_all(); make_image(5,1);
    rec(50,1);
    CHECK(Ota_SelectSlot(NULL)==APP,"da confirm khong bi loai");

    /* 4. ENTER_REC (FORCE_FAIL): loai ke ca khi da confirm; nap lai (0,0) dua app tro lai */
    rec(CFG_BOOT_COUNT_FORCE_FAIL,0);
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_NONE,"FORCE_FAIL (yeu cau vao recovery) -> NONE");
    CHECK(!Ota_SlotBootable(APP),"FORCE_FAIL -> khong bootable");
    rec(CFG_BOOT_COUNT_FORCE_FAIL,1);
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_NONE,"FORCE_FAIL + confirmed=1 van bi loai (chi dua vao co FORCE_FAIL)");
    rec(0,0);
    CHECK(Ota_SelectSlot(NULL)==APP,"nap lai (0,0) -> chon duoc");

    /* 5. Anh hong CRC / vector -> loai du record dep */
    reset_all(); make_image(5,1);
    simp(Ota_CodeBase(APP))[100]^=1;
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_NONE,"hong CRC -> NONE");
    reset_all(); make_image(5,1);
    { uint32_t bad=0x20000000u; memcpy(simp(Ota_CodeBase(APP)),&bad,4); }      /* MSP hop le nhung CRC doi -> CRC hong */
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_NONE,"sua vector -> CRC sai -> NONE");

    /* 6. Chuoi boot khong confirm: dung OTA_MAX_BOOT_ATTEMPTS lan */
    reset_all(); make_image(5,1);
    int boots=0; for(;;){ ota_slot_t s=Ota_SelectSlot(NULL); if(s==OTA_SLOT_NONE)break; CHECK(Ota_CommitTrial(s),"commit trial"); boots++; if(boots>10)break; }
    CHECK(boots==(int)OTA_MAX_BOOT_ATTEMPTS,"so lan boot truoc khi bi loai = %d (mong doi %u)",boots,OTA_MAX_BOOT_ATTEMPTS);

    /* 7. Ota_CommitTrial: dem luot cho anh chua confirm, khong dem anh da confirm */
    reset_all(); make_image(5,1);
    CHECK(Ota_CommitTrial(APP),"commit lan 1");
    { cfg_boot_payload_t b; CHECK(CfgStore_ReadBoot(APP,&b)&&b.boot_count==1&&!b.confirmed,"count=1 sau lan 1"); }
    rec(9,1);
    CHECK(Ota_CommitTrial(APP),"commit anh da confirm");
    { cfg_boot_payload_t b; CHECK(CfgStore_ReadBoot(APP,&b)&&b.boot_count==9&&b.confirmed,"anh da confirm khong bi dem"); }

    /* 8. Ghi bo dem loi: tra false (van khoi dong, khong bo thiet bi chet) */
    reset_all(); make_image(5,1);
    force_prog_fail = 1;
    CHECK(!Ota_CommitTrial(APP),"ghi loi -> false");
    force_prog_fail = 0;
    CHECK(Ota_SelectSlot(NULL)==APP,"ghi loi khong lam mat kha nang chon app");

    printf("select tests done, fails=%d\n",fails);
    return fails?1:0;
}
