#include <string.h>
#include <stddef.h>

#include "flash.h"
#include "cfg_store.h"
#include "crc16_ccitt.h"

static inline const cfg_record_t *rec_at(uint32_t idx)
{
    return (const cfg_record_t *)(OTA_CFG_BASE + idx * CFG_REC_SIZE);
}

static bool rec_is_valid(const cfg_record_t *r)
{
    if (r->magic != CFG_REC_MAGIC) return false;
    const uint16_t calc = Crc16_Compute((const uint8_t *)r,
                                     (uint32_t)offsetof(cfg_record_t, crc16));
    return calc == r->crc16;
}

/* ==========================================================================
 * CACHE TRONG RAM - dien 1 lan luc CfgStore_Init(), cap nhat song song
 * moi lan ghi thanh cong de khong phai quet lai sector.
 * ========================================================================== */
static struct {
    uint32_t next_free_idx;    // = CFG_REC_COUNT neu sector day
    uint32_t max_seq;

    bool has_limits;
    cfg_limits_payload_t limits;

    bool has_boot[OTA_SLOT_COUNT];
    cfg_boot_payload_t boot[OTA_SLOT_COUNT];
} s;

static bool write_record(uint8_t type, const void *payload, uint8_t payload_len);
static bool compact(void);

static void cache_apply(uint8_t type, const void *payload)
{
    if (type == CFG_REC_TYPE_LIMITS) {
        memcpy(&s.limits, payload, sizeof(s.limits));
        s.has_limits = true;
    } else if (type == CFG_REC_TYPE_BOOT) {
        cfg_boot_payload_t bp;
        memcpy(&bp, payload, sizeof(bp));
        const ota_slot_t sl = Ota_SlotFromTag((char)bp.slot);
        if (Ota_SlotValid(sl)) {
        	s.boot[sl] = bp;
        	s.has_boot[sl] = true;
        }
    }
}

static bool compact(void)
{
	/* chup cache truoc khi xoa */
	const bool had_limits = s.has_limits;
	const cfg_limits_payload_t limits = s.limits;
	bool had_boot[OTA_SLOT_COUNT];
	cfg_boot_payload_t boot[OTA_SLOT_COUNT];
	memcpy(had_boot, s.has_boot, sizeof(had_boot));
	memcpy(boot, s.boot, sizeof(boot));

	if (!Flash_EraseSector(OTA_CFG_SECTOR).ok) return false;

	s.next_free_idx = 0u;                 /* max_seq GIU NGUYEN: seq tang lien tuc */
	s.has_limits = false;
	memset(s.has_boot, 0, sizeof(s.has_boot));

    for (ota_slot_t sl = OTA_SLOT_A; sl < OTA_SLOT_COUNT; sl++) {
        if (had_boot[sl] && !write_record(CFG_REC_TYPE_BOOT, &boot[sl], sizeof(boot[sl])))
            return false;
    }
    if (had_limits && !write_record(CFG_REC_TYPE_LIMITS, &limits, sizeof(limits)))
        return false;
    return true;
}

void CfgStore_Init(void)
{
    memset(&s, 0, sizeof(s));
    s.next_free_idx = CFG_REC_COUNT;

    for (uint32_t i = 0; i < CFG_REC_COUNT; i++) {
        const cfg_record_t *r = rec_at(i);

        if (r->magic == 0xFFFFFFFFu) {
            s.next_free_idx = i;
            break;
        }
        if (!rec_is_valid(r)) continue;

        if (r->seq > s.max_seq) s.max_seq = r->seq;
        cache_apply(r->type, r->payload);
    }
}

static bool write_record(uint8_t type, const void *payload, uint8_t payload_len)
{
    if (s.next_free_idx >= CFG_REC_COUNT) {
        if (!compact()) return false;   /* nen that bai (loi xoa) - khong con cach nao khac */
    }

    cfg_record_t rec;
    memset(&rec, 0xFF, sizeof(rec));    /* dem phan chua dung bang mau xoa */
    rec.magic = CFG_REC_MAGIC;
    rec.seq   = ++s.max_seq;
    rec.type  = type;
    memset(rec.payload, 0, sizeof(rec.payload));
    memcpy(rec.payload, payload, payload_len);
    rec.crc16 = Crc16_Compute((const uint8_t *)&rec,
                           (uint32_t)offsetof(cfg_record_t, crc16));

    uint8_t buf[CFG_REC_SIZE] __attribute__((aligned(32)));
    memset(buf, 0xFF, sizeof(buf));
    memcpy(buf, &rec, sizeof(rec));

    const uint32_t addr = OTA_CFG_BASE + s.next_free_idx * CFG_REC_SIZE;

    flash_op_result_t r = Flash_ProgramWords(addr, buf, CFG_REC_SIZE);
    if (!r.ok) return false;

    s.next_free_idx++;
    cache_apply(type, payload);

    return true;
}

/* ==========================================================================
 * API
 * ========================================================================== */
bool CfgStore_ReadBoot(ota_slot_t sl, cfg_boot_payload_t *out)
{
    if (!Ota_SlotValid(sl) || !s.has_boot[sl]) return false;
    *out = s.boot[sl];
    return true;
}

bool CfgStore_WriteBoot(ota_slot_t sl, uint32_t boot_count, uint8_t confirmed)
{
    if (!Ota_SlotValid(sl)) return false;

    cfg_boot_payload_t bp;
    memset(&bp, 0, sizeof(bp));           /* padding xac dinh */
    bp.slot       = (uint8_t)g_ota_slots[sl].tag;
    bp.boot_count = boot_count;
    bp.confirmed  = confirmed;
    return write_record(CFG_REC_TYPE_BOOT, &bp, (uint8_t)sizeof(bp));
}

bool CfgStore_ReadLimits(cfg_limits_payload_t *out)
{
    if (!s.has_limits) return false;
    *out = s.limits;
    return true;
}

bool CfgStore_WriteLimits(const cfg_limits_payload_t *in)
{
    return write_record(CFG_REC_TYPE_LIMITS, in, (uint8_t)sizeof(*in));
}

uint32_t CfgStore_AllocInstallSeq(void)
{
    const uint8_t dummy = 0xFFu;

    if (!write_record(CFG_REC_TYPE_INSTALL, &dummy, 1u)) {
        return 0u;
    }
    return s.max_seq;
}
