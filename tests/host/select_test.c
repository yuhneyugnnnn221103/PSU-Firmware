/* Test chon slot: slot het luot ma chua confirm KHONG duoc chon lai. */
#include <stdio.h>
#include <string.h>
#include "flash_sim.h"
#include "ota_select.h"
#include "cfg_store.h"
#include "crc32_sw.h"

static int fails;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } }while(0)

static uint8_t *simp(uint32_t a){ return sim + (a - 0x08000000u); }

/* Dung anh hop le truc tiep trong flash gia */
static void make_image(ota_slot_t s, uint32_t seq, uint32_t ver)
{
    uint8_t code[600]; for (unsigned i=0;i<sizeof code;i++) code[i]=(uint8_t)(i*5+1);
    uint32_t msp=0x24050000u, rh=Ota_CodeBase(s)+0x101u;
    memcpy(code,&msp,4); memcpy(code+4,&rh,4);
    ota_image_header_t h; memset(&h,0xFF,sizeof h);
    h.magic=OTA_IMG_MAGIC; h.version=ver; h.size=sizeof code; h.crc32=Crc32_Compute(code,sizeof code); h.install_seq=seq;
    memcpy(simp(g_ota_slots[s].base),&h,sizeof h);
    memcpy(simp(Ota_CodeBase(s)),code,sizeof code);
}
static void reset_all(void){ memset(sim,0xFF,sizeof sim); CfgStore_Init(); }
static void rec(ota_slot_t s, uint32_t count, uint8_t conf){ CHECK(CfgStore_WriteBoot(s,count,conf),"writeboot"); }

int main(void)
{
    /* 1. Khong co record: chon anh moi hon theo install_seq, roi version */
    reset_all(); make_image(OTA_SLOT_A,5,1); make_image(OTA_SLOT_B,6,1);
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_B,"seq cao hon thang");
    reset_all(); make_image(OTA_SLOT_A,7,1); make_image(OTA_SLOT_B,7,2);
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_B,"hoa seq -> version cao hon");

    /* 2. Trial: count 0..2 chua confirm van chon duoc; count 3 thi loai */
    reset_all(); make_image(OTA_SLOT_A,5,1);
    for (uint32_t c=0;c<OTA_MAX_BOOT_ATTEMPTS;c++){ rec(OTA_SLOT_A,c,0); CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_A,"count=%u phai chon duoc",c); }
    rec(OTA_SLOT_A,OTA_MAX_BOOT_ATTEMPTS,0);
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_NONE,"het luot + chi co 1 anh valid -> KHONG chon lai (recovery)");

    /* 3. A het luot, B valid -> B */
    reset_all(); make_image(OTA_SLOT_A,9,1); make_image(OTA_SLOT_B,5,1);
    rec(OTA_SLOT_A,OTA_MAX_BOOT_ATTEMPTS,0);
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_B,"A het luot -> B (du A moi hon)");

    /* 4. Ca 2 het luot -> NONE */
    rec(OTA_SLOT_B,OTA_MAX_BOOT_ATTEMPTS+4,0);
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_NONE,"ca 2 het luot -> NONE");

    /* 5. Da confirm: count lon van chon duoc */
    reset_all(); make_image(OTA_SLOT_A,5,1);
    rec(OTA_SLOT_A,50,1);
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_A,"da confirm khong bi loai");

    /* 6. Rollback (FORCE_FAIL): loai, ke ca khi slot kia khong hop le */
    reset_all(); make_image(OTA_SLOT_A,5,1);
    rec(OTA_SLOT_A,CFG_BOOT_COUNT_FORCE_FAIL,0);
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_NONE,"rollback + khong con anh khac -> NONE");
    make_image(OTA_SLOT_B,1,1);
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_B,"rollback A -> B");

    /* 7. Nap lai (WriteBoot 0,0) dua slot het luot tro lai */
    reset_all(); make_image(OTA_SLOT_A,5,1); rec(OTA_SLOT_A,OTA_MAX_BOOT_ATTEMPTS,0);
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_NONE,"truoc khi nap lai");
    rec(OTA_SLOT_A,0,0);
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_A,"nap lai -> chon duoc");

    /* 8. Anh hong CRC -> loai du record dep; trang thai khong lam chet slot kia */
    reset_all(); make_image(OTA_SLOT_A,5,1); make_image(OTA_SLOT_B,4,1);
    simp(Ota_CodeBase(OTA_SLOT_A))[100]^=1;
    CHECK(Ota_SelectSlot(NULL)==OTA_SLOT_B,"A hong CRC -> B");

    /* 9. Qua cac lan reboot mo phong: chuoi boot khong confirm */
    reset_all(); make_image(OTA_SLOT_A,5,1);
    int boots=0; for(;;){ ota_slot_t s=Ota_SelectSlot(NULL); if(s==OTA_SLOT_NONE)break; cfg_boot_payload_t bp; uint32_t c=CfgStore_ReadBoot(s,&bp)?bp.boot_count:0; CfgStore_WriteBoot(s,c+1,0); boots++; if(boots>10)break; }
    CHECK(boots==(int)OTA_MAX_BOOT_ATTEMPTS,"so lan boot truoc khi bi loai = %d (mong doi %u)",boots,OTA_MAX_BOOT_ATTEMPTS);

    printf("select tests done, fails=%d\n",fails);
    return fails?1:0;
}
