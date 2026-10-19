#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include "flash_sim.h"
#include "cfg_store.h"
#include "flash.h"
#include "flash_guard.h"

static int fails;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } }while(0)

#define BLOB_LEN 904u
static void mkblob(uint8_t *b, uint16_t v){ for (unsigned i=0;i<BLOB_LEN;i++) b[i]=(uint8_t)(v*31u+i*7u+(i>>8)); }
static bool blob_is(uint16_t v){ uint8_t want[BLOB_LEN], got[CFG_BLOB_MAX]; uint16_t n=0; mkblob(want,v);
    return CfgStore_ReadBlob(CFG_BLOB_PSU,got,sizeof got,&n) && n==BLOB_LEN && memcmp(want,got,BLOB_LEN)==0; }
static bool put_blob(uint16_t v){ uint8_t b[BLOB_LEN]; mkblob(b,v); return CfgStore_WriteBlob(CFG_BLOB_PSU,b,BLOB_LEN); }

int main(void)
{
    memset(sim,0xFF,sizeof sim);
    CfgStore_Init();
    CHECK(CfgStore_NeedsErase(), "fresh device must need erase/commit");
    CHECK(CfgStore_WriteBoot(OTA_SLOT_A,2,0), "writeboot A");
    CHECK(CfgStore_WriteBoot(OTA_SLOT_B,0,1), "writeboot B");
    CHECK(put_blob(100),"blob");
    CfgStore_Init();
    cfg_boot_payload_t bp;
    CHECK(CfgStore_ReadBoot(OTA_SLOT_A,&bp)&&bp.boot_count==2&&!bp.confirmed,"reload A");
    CHECK(CfgStore_ReadBoot(OTA_SLOT_B,&bp)&&bp.confirmed,"reload B");
    CHECK(blob_is(100),"reload blob");

    /* Day bank: ghi limits cho toi khi sap day */
    uint16_t v=100;
    for (int i=0;i<75;i++){ v++; CHECK(put_blob(v),"fill %d",i); }      /* ~75 x 26 ban ghi: sap day bank */
    while (!CfgStore_NeedsErase()) { v++; CHECK(put_blob(v),"fill tiep"); }
    uint32_t seqbefore = CfgStore_AllocInstallSeq();
    CHECK(seqbefore>0,"seq");
    static uint8_t base[sizeof sim]; memcpy(base,sim,sizeof sim);

    /* Quet moi diem mat dien trong luc ghi 20 limits (di qua compaction) */
    long total=0;
    for (int pass=0; pass<2; pass++) {
        for (long k=0; ; k++) {
            memcpy(sim,base,sizeof sim); ops=0; fail_at=k;
            CfgStore_Init();
            volatile uint16_t vv=v, last_ok=v; bool crashed=false;
            if (setjmp(sim_jb)==0) {
                for (int i=0;i<4;i++){ vv++; if(put_blob(vv)) last_ok=vv; else break; }
            } else crashed=true;
            fail_at=-1;
            if (!crashed) { total=k; break; }
            /* reboot */
            CfgStore_Init();
            CHECK(CfgStore_ReadBoot(OTA_SLOT_A,&bp)&&bp.boot_count==2,"k=%ld boot A lost",k);
            CHECK(CfgStore_ReadBoot(OTA_SLOT_B,&bp)&&bp.confirmed,"k=%ld boot B lost",k);
            CHECK(blob_is(last_ok) || blob_is((uint16_t)(last_ok+1)),"k=%ld blob mat/hong/cu (last_ok=%u)",k,last_ok);
            uint32_t s2=CfgStore_AllocInstallSeq(); CHECK(s2>seqbefore,"k=%ld seq regressed %u<=%u",k,s2,seqbefore);
            /* sau reboot van ghi duoc */
            CHECK(put_blob(7) && blob_is(7),"k=%ld write after reboot",k);
            
        }
        break;
    }
    /* quet that su: lap k cho den khi khong crash */

    /* ---- Quet mat dien tren may MOI (khoi tao + 3 lan ghi dau) ---- */
    for (long k=0;;k++) {
        memset(sim,0xFF,sizeof sim); ops=0; fail_at=k; CfgStore_Init();
        volatile int done=0; bool crashed=false;
        if (setjmp(sim_jb)==0) {
            if(CfgStore_WriteBoot(OTA_SLOT_A,1,0)) done=1;
            if(CfgStore_WriteBoot(OTA_SLOT_A,2,1)) done=2;
            if(put_blob(55)) done=3;
        } else crashed=true;
        fail_at=-1;
        if(!crashed){ printf("fresh sweep ok, %ld crash points\n",k); break; }
        CfgStore_Init();
        bool has=CfgStore_ReadBoot(OTA_SLOT_A,&bp);
        if (done>=1) CHECK(has,"fresh k=%ld boot lost (done=%d)",k,done);
        if (has) CHECK(bp.boot_count==1||bp.boot_count==2,"fresh k=%ld bad boot_count",k);
        if (done>=2) CHECK(bp.boot_count==2&&bp.confirmed,"fresh k=%ld confirm lost",k);
        { uint8_t gb[CFG_BLOB_MAX]; uint16_t gl; bool hb = CfgStore_ReadBlob(CFG_BLOB_PSU,gb,sizeof gb,&gl);
          if (done>=3) CHECK(blob_is(55),"fresh k=%ld blob mat sau khi da ghi xong",k);
          if (hb) CHECK(blob_is(55),"fresh k=%ld blob doc duoc nhung khong dung",k); }
        CHECK(CfgStore_WriteBoot(OTA_SLOT_B,0,0),"fresh k=%ld write after reboot",k);
        CfgStore_Init();
        CHECK(CfgStore_ReadBoot(OTA_SLOT_B,&bp),"fresh k=%ld B not persisted",k);
    }
    printf("sweep crash points done, ops until completion=%ld, fails=%d\n", total, fails);
    return fails?1:0;
}
