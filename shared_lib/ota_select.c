#include "ota_select.h"
#include "cfg_store.h"

static bool slot_bootable(ota_slot_t sl)
{
    cfg_boot_payload_t bp;

    if (CfgStore_ReadBoot(sl, &bp)) {
        if (bp.boot_count == CFG_BOOT_COUNT_FORCE_FAIL)                 return false;  /* PC rollback */
        if (!bp.confirmed && bp.boot_count >= OTA_MAX_BOOT_ATTEMPTS)    return false;  /* het luot */
    }
    return Ota_ImageValid(sl) && Ota_VectorOk(sl);
}

/* true neu anh b moi hon anh a */
static bool newer(ota_slot_t b, ota_slot_t a)
{
    ota_image_header_t ha, hb;
    if (!Ota_ReadHeader(a, &ha)) return true;
    if (!Ota_ReadHeader(b, &hb)) return false;

    if (hb.install_seq != ha.install_seq) return hb.install_seq > ha.install_seq;
    return hb.version > ha.version;
}

ota_slot_t Ota_SelectSlot(void (*tick)(void))
{
    ota_slot_t best = OTA_SLOT_NONE;

    for (ota_slot_t sl = OTA_SLOT_A; sl < OTA_SLOT_COUNT; sl++) {
        const bool ok = slot_bootable(sl);
        if (tick) tick();
        if (!ok) continue;
        if (best == OTA_SLOT_NONE || newer(sl, best)) best = sl;
    }
    return best;
}
