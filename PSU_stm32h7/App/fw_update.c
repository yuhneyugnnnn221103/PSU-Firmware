#include <fw_update.h>
#include <safety.h>
#include "main.h"
#include "ota_image.h"
#include "cfg_store.h"
#include "cfg_record.h"
#include "ota_proto.h"

#define FWU_RESET_DELAY_MS   50u     /* cho ACK cua ENTER_REC gui xong truoc khi reset */

static struct {
    bool     reset_pending;
    uint32_t reset_at_ms;
} s;

static inline ota_slot_t my_slot(void)
{
    return Ota_SlotRunning();
}

void FwUpdate_Init(void)
{
    s.reset_pending = false;
}

uint32_t FwUpdate_RunningVersion(void)
{
    if (my_slot() == OTA_SLOT_NONE) return 0u;

    const ota_image_header_t *h = Ota_Header(my_slot());
    if (h->magic != OTA_IMG_MAGIC) return 0u;
    return h->version;
}

uint8_t FwUpdate_RunningState(void)
{
    cfg_boot_payload_t bp;
    if (my_slot() == OTA_SLOT_NONE || !CfgStore_ReadBoot(my_slot(), &bp)) return 0u;

    if (bp.boot_count >= OTAP_STATE_COUNT_MAX) bp.boot_count = OTAP_STATE_COUNT_MAX;   /* gom ca FORCE_FAIL */
    return (uint8_t)((bp.confirmed ? OTAP_STATE_CONFIRMED : 0u) | (uint8_t)bp.boot_count);
}

void FwUpdate_VersionStr(char out[32])
{
    (void)Ota_ReadVersionStr(my_slot(), out);
}

char FwUpdate_RunningSlot(void)
{
    const ota_slot_t sl = my_slot();
    return Ota_SlotValid(sl) ? g_ota_slots[sl].tag : '?';
}

uint8_t FwUpdate_RequestEnterRecovery(void)
{
    if (!Safety_AllOff())            return OTAP_ST_REFUSED;     /* ghi cfg co the xoa sector -> chan CPU */
    if (s.reset_pending)             return OTAP_ST_OK;          /* nhan lan 2 trong luc cho reset */
    if (my_slot() == OTA_SLOT_NONE)  return OTAP_ST_WRONG_SLOT;

    /* Danh dau app "khong khoi dong" -> bootloader o lai recovery cho den khi nap xong (END ghi lai boot record). */
    if (!CfgStore_WriteBoot(my_slot(), CFG_BOOT_COUNT_FORCE_FAIL, 0u)) return OTAP_ST_PROG_FAIL;

    s.reset_pending = true;
    s.reset_at_ms   = HAL_GetTick() + FWU_RESET_DELAY_MS;
    return OTAP_ST_OK;
}

void FwUpdate_Task(uint32_t now_ms)
{
    if (s.reset_pending && (int32_t)(now_ms - s.reset_at_ms) >= 0) {
        NVIC_SystemReset();
    }
}
