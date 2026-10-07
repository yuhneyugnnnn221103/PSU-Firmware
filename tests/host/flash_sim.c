#include <string.h>
#include <stdbool.h>
#include "flash_sim.h"
#include "flash.h"
#include "flash_guard.h"

uint32_t g_pfnVectors;
volatile uint32_t g_flash_ecc_fault_count;

uint8_t sim[1024*1024];
long ops, fail_at = -1;
jmp_buf sim_jb;

static void power_fail(void){ longjmp(sim_jb,1); }

long overwrite_errors;   /* so lan co ghi len flash-word chua xoa (H7 khong cho) */

flash_op_result_t Flash_ProgramWords(uint32_t addr, const uint8_t *d, uint32_t len)
{
    flash_op_result_t r = {true,0};
    for (uint32_t w = 0; w < len/32; w++) {
        if (ops++ == fail_at) power_fail();          /* mat dien truoc flash-word nay */
        uint8_t *p = sim + (addr - 0x08000000u) + 32*w;
        for (int i=0;i<32;i++) {
            if (p[i] != 0xFF) { overwrite_errors++; r.ok = false; r.hal_error = 0x20; return r; }
        }
        memcpy(p, d + 32*w, 32);
    }
    return r;
}
flash_op_result_t Flash_EraseSector(uint32_t n)
{
    flash_op_result_t r = {true,0};
    if (ops++ == fail_at) { memset(sim + n*0x20000, 0xFF, 0x10000); power_fail(); } /* xoa do 1 nua */
    memset(sim + n*0x20000, 0xFF, 0x20000);
    return r;
}
void FlashGuard_Begin(uint32_t a,uint32_t b){(void)a;(void)b;}
bool FlashGuard_End(void){return true;}
void FlashGuard_CopyWords(void*dst,uint32_t src,uint32_t len){memcpy(dst,sim+(src-0x08000000u),len);}

