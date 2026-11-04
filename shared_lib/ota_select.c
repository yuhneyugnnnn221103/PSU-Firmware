#include "ota_select.h"
#include "cfg_store.h"

bool Ota_SlotBootable(ota_slot_t sl)
{
    cfg_boot_payload_t bp;

    if (CfgStore_ReadBoot(sl, &bp)) {
        if (bp.boot_count == CFG_BOOT_COUNT_FORCE_FAIL)                 return false;  /* app yeu cau vao recovery */
        if (!bp.confirmed && bp.boot_count >= OTA_MAX_BOOT_ATTEMPTS)    return false;  /* het luot */
    }
    return Ota_ImageValid(sl) && Ota_VectorOk(sl);
}

ota_slot_t Ota_SelectSlot(void (*tick)(void))
{
    const bool ok = Ota_SlotBootable(OTA_SLOT_APP);
    if (tick) tick();
    return ok ? OTA_SLOT_APP : OTA_SLOT_NONE;
}

bool Ota_CommitTrial(ota_slot_t sl)
{
    cfg_boot_payload_t bp;
    const bool has_rec = CfgStore_ReadBoot(sl, &bp);

    if (has_rec && bp.confirmed) return true;        /* anh da confirm: khong dem */

    const uint32_t count = has_rec ? bp.boot_count : 0u;
    return CfgStore_WriteBoot(sl, count + 1u, 0u);
}
