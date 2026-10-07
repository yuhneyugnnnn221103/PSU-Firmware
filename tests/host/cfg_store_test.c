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

static cfg_limits_payload_t lim(uint16_t v){ cfg_limits_payload_t l; for(int i=0;i<4;i++){l.sovl[i]=v;l.bovl[i]=v+1;l.buvl[i]=v+2;} return l; }

int main(void)
{
    memset(sim,0xFF,sizeof sim);
    CfgStore_Init();
    CHECK(CfgStore_NeedsErase(), "fresh device must need erase/commit");
    CHECK(CfgStore_WriteBoot(OTA_SLOT_A,2,0), "writeboot A");
    CHECK(CfgStore_WriteBoot(OTA_SLOT_B,0,1), "writeboot B");
    cfg_limits_payload_t l1 = lim(100); CHECK(CfgStore_WriteLimits(&l1),"limits");
    CfgStore_Init();
    cfg_boot_payload_t bp; cfg_limits_payload_t lr;
    CHECK(CfgStore_ReadBoot(OTA_SLOT_A,&bp)&&bp.boot_count==2&&!bp.confirmed,"reload A");
    CHECK(CfgStore_ReadBoot(OTA_SLOT_B,&bp)&&bp.confirmed,"reload B");
    CHECK(CfgStore_ReadLimits(&lr)&&lr.sovl[0]==100,"reload limits");

    /* Day bank: ghi limits cho toi khi sap day */
    uint16_t v=100;
    for (int i=0;i<2040;i++){ v++; cfg_limits_payload_t l=lim(v); CHECK(CfgStore_WriteLimits(&l),"fill %d",i); }
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
                for (int i=0;i<20;i++){ vv++; cfg_limits_payload_t l=lim(vv); if(CfgStore_WriteLimits(&l)) last_ok=vv; else break; }
            } else crashed=true;
            fail_at=-1;
            if (!crashed) { total=k; break; }
            /* reboot */
            CfgStore_Init();
            CHECK(CfgStore_ReadBoot(OTA_SLOT_A,&bp)&&bp.boot_count==2,"k=%ld boot A lost",k);
            CHECK(CfgStore_ReadBoot(OTA_SLOT_B,&bp)&&bp.confirmed,"k=%ld boot B lost",k);
            CHECK(CfgStore_ReadLimits(&lr),"k=%ld limits lost",k);
            if (CfgStore_ReadLimits(&lr)) CHECK(lr.sovl[0]==last_ok||lr.sovl[0]==(uint16_t)(last_ok+1),"k=%ld limits stale %u vs %u",k,lr.sovl[0],last_ok);
            uint32_t s2=CfgStore_AllocInstallSeq(); CHECK(s2>seqbefore,"k=%ld seq regressed %u<=%u",k,s2,seqbefore);
            /* sau reboot van ghi duoc */
            cfg_limits_payload_t l=lim(7); CHECK(CfgStore_WriteLimits(&l),"k=%ld write after reboot",k);
            
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
            cfg_limits_payload_t l=lim(55); if(CfgStore_WriteLimits(&l)) done=3;
        } else crashed=true;
        fail_at=-1;
        if(!crashed){ printf("fresh sweep ok, %ld crash points\n",k); break; }
        CfgStore_Init();
        bool has=CfgStore_ReadBoot(OTA_SLOT_A,&bp);
        if (done>=1) CHECK(has,"fresh k=%ld boot lost (done=%d)",k,done);
        if (has) CHECK(bp.boot_count==1||bp.boot_count==2,"fresh k=%ld bad boot_count",k);
        if (done>=2) CHECK(bp.boot_count==2&&bp.confirmed,"fresh k=%ld confirm lost",k);
        CHECK(CfgStore_WriteBoot(OTA_SLOT_B,0,0),"fresh k=%ld write after reboot",k);
        CfgStore_Init();
        CHECK(CfgStore_ReadBoot(OTA_SLOT_B,&bp),"fresh k=%ld B not persisted",k);
    }
    printf("sweep crash points done, ops until completion=%ld, fails=%d\n", total, fails);
    return fails?1:0;
}
