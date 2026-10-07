#include <string.h>
#include <stddef.h>

#include "flash.h"
#include "cfg_store.h"
#include "crc32_sw.h"
#include "flash_guard.h"

/* ==========================================================================
 * 2 BANK LUAN PHIEN (sector 6 / 7).
 * Bank "hop le" = co ban ghi COMMIT hop le. Compaction:
 *   1. xoa bank KIA  2. ghi snapshot  3. ghi COMMIT (cuoi cung).
 * Mat dien truoc buoc 3 -> bank moi khong co COMMIT -> bi bo qua, bank cu
 * van day du. Sau buoc 3 -> bank moi thang (COMMIT.seq cao hon).
 * Bank cu khong bi xoa ngay; no se bi xoa o lan compaction ke tiep.
 * ========================================================================== */

static uint32_t rec_addr(uint8_t bank, uint32_t idx)
{
    return OTA_CFG_BANK_BASE(bank) + idx * CFG_REC_SIZE;
}

static bool rec_read(uint8_t bank, uint32_t idx, cfg_record_t *out)
{
    const uint32_t a = rec_addr(bank, idx);
    FlashGuard_Begin(a, a + CFG_REC_SIZE);
    FlashGuard_CopyWords(out, a, CFG_REC_SIZE);
    return FlashGuard_End();
}

/* O trong that su = TOAN BO 64 byte la 0xFF (khong chi magic) */
static bool rec_is_erased(const cfg_record_t *r)
{
    const uint32_t *w = (const uint32_t *)r;
    for (uint32_t i = 0; i < CFG_REC_SIZE / 4u; i++) {
        if (w[i] != 0xFFFFFFFFu) return false;
    }
    return true;
}

static uint32_t rec_crc(const cfg_record_t *r)
{
    return Crc32_Compute((const uint8_t *)r, (uint32_t)offsetof(cfg_record_t, crc32));
}

static bool rec_is_valid(const cfg_record_t *r)
{
    if (r->magic != CFG_REC_MAGIC)  return false;
    if (r->len > CFG_PAYLOAD_MAX)   return false;
    return r->crc32 == rec_crc(r);
}

/* ==========================================================================
 * CACHE TRONG RAM - dien 1 lan luc CfgStore_Init(), cap nhat song song
 * moi lan ghi thanh cong de khong phai quet lai flash.
 * ========================================================================== */
typedef struct {
    bool     has_limits;
    cfg_limits_payload_t limits;

    bool has_boot[OTA_SLOT_COUNT];
    cfg_boot_payload_t boot[OTA_SLOT_COUNT];
} cfg_cache_t;

static struct {
    uint8_t  bank;             /* bank dang ghi */
    bool     committed;        /* bank dang ghi da co COMMIT? */
    uint32_t next_free_idx;    /* = CFG_REC_COUNT neu bank day */
    uint32_t max_seq;
    cfg_cache_t c;
} s;

static void cache_apply(cfg_cache_t *c, uint8_t type, const void *payload, uint8_t len)
{
    if (type == CFG_REC_TYPE_LIMITS) {
        if (len != sizeof(cfg_limits_payload_t)) return;
        memcpy(&c->limits, payload, sizeof(c->limits));
        c->has_limits = true;
    } else if (type == CFG_REC_TYPE_BOOT) {
        if (len != sizeof(cfg_boot_payload_t)) return;
        cfg_boot_payload_t bp;
        memcpy(&bp, payload, sizeof(bp));
        const ota_slot_t sl = Ota_SlotFromTag((char)bp.slot);
        if (Ota_SlotValid(sl)) {
            c->boot[sl]     = bp;
            c->has_boot[sl] = true;
        }
    }
}

typedef struct {
    bool        has_commit;
    uint32_t    commit_seq;
    uint32_t    max_seq;
    uint32_t    next_free;
    cfg_cache_t c;
} bank_scan_t;

static void scan_bank(uint8_t bank, bank_scan_t *o)
{
    memset(o, 0, sizeof(*o));
    o->next_free = CFG_REC_COUNT;

    for (uint32_t i = 0; i < CFG_REC_COUNT; i++) {
        cfg_record_t r;

        /* Hong ECC (mat dien GIUA luc ghi) -> bo qua, KHONG coi la o trong */
        if (!rec_read(bank, i, &r)) continue;

        if (rec_is_erased(&r)) { o->next_free = i; break; }
        if (!rec_is_valid(&r)) continue;

        if (r.seq > o->max_seq) o->max_seq = r.seq;
        if (r.type == CFG_REC_TYPE_COMMIT) {
            o->has_commit = true;
            if (r.seq > o->commit_seq) o->commit_seq = r.seq;
        }
        cache_apply(&o->c, r.type, r.payload, r.len);
    }
}

void CfgStore_Init(void)
{
    bank_scan_t sc[OTA_CFG_BANKS];

    memset(&s, 0, sizeof(s));
    s.next_free_idx = CFG_REC_COUNT;

    for (uint8_t b = 0; b < OTA_CFG_BANKS; b++) scan_bank(b, &sc[b]);

    if (!sc[0].has_commit && !sc[1].has_commit) {
        /* May moi hoac ca 2 bank hong: dung bank 0, se xoa + COMMIT khi ghi lan dau */
        s.bank = 0u;
        return;
    }

    uint8_t pick;
    if (sc[0].has_commit && sc[1].has_commit) pick = (sc[1].commit_seq > sc[0].commit_seq) ? 1u : 0u;
    else                                      pick = sc[1].has_commit ? 1u : 0u;

    s.bank          = pick;
    s.committed     = true;
    s.next_free_idx = sc[pick].next_free;
    s.max_seq       = sc[pick].max_seq;
    s.c             = sc[pick].c;

    /* seq khong bao gio lui, ke ca khi bank con lai co seq cao hon */
    if (sc[pick ^ 1u].max_seq > s.max_seq) s.max_seq = sc[pick ^ 1u].max_seq;
}

/* Ghi 1 ban ghi vao (bank, idx), doc lai so sanh. Khong dung cache. */
static bool prog_record(uint8_t bank, uint32_t idx, uint32_t seq,
                        uint8_t type, const void *payload, uint8_t len)
{
    cfg_record_t rec __attribute__((aligned(32)));
    memset(&rec, 0, sizeof(rec));
    rec.magic = CFG_REC_MAGIC;
    rec.seq   = seq;
    rec.type  = type;
    rec.len   = len;
    memcpy(rec.payload, payload, len);
    rec.crc32 = rec_crc(&rec);

    const flash_op_result_t r =
        Flash_ProgramWords(rec_addr(bank, idx), (const uint8_t *)&rec, CFG_REC_SIZE);

    cfg_record_t chk;
    return r.ok && rec_read(bank, idx, &chk) && (memcmp(&chk, &rec, sizeof(rec)) == 0);
}

/* Them ban ghi vao cuoi bank dang ghi. Thu toi da 2 o: o dau loi thi bo qua
 * (H7 khong cho ghi de flash-word da lap trinh -> o loi KHONG dung lai). */
static bool append(uint8_t type, const void *payload, uint8_t len)
{
    for (uint8_t attempt = 0; attempt < 2u; attempt++) {
        if (s.next_free_idx >= CFG_REC_COUNT) return false;

        const uint32_t idx = s.next_free_idx++;
        const uint32_t seq = ++s.max_seq;

        if (prog_record(s.bank, idx, seq, type, payload, len)) return true;
    }
    return false;
}

static bool compact(void)
{
    const uint8_t old_bank = s.bank;
    const uint8_t new_bank = (uint8_t)(old_bank ^ 1u);

    if (!Flash_EraseSector(OTA_CFG_SECTOR(new_bank)).ok) return false;

    /* Chuyen sang bank moi; neu that bai, tra lai bank cu (cache khong bi dong vao) */
    const uint32_t save_free = s.next_free_idx;
    const uint32_t save_seq  = s.max_seq;
    const bool     save_com  = s.committed;

    s.bank = new_bank;
    s.next_free_idx = 0u;
    s.committed = false;

    bool ok = true;
    for (ota_slot_t sl = OTA_SLOT_A; ok && sl < OTA_SLOT_COUNT; sl++) {
        if (s.c.has_boot[sl]) ok = append(CFG_REC_TYPE_BOOT, &s.c.boot[sl], sizeof(s.c.boot[sl]));
    }
    if (ok && s.c.has_limits) ok = append(CFG_REC_TYPE_LIMITS, &s.c.limits, sizeof(s.c.limits));

    const uint8_t dummy = 0xFFu;
    if (ok) ok = append(CFG_REC_TYPE_COMMIT, &dummy, 1u);   /* CUOI CUNG */

    if (!ok) {
        s.bank = old_bank;
        s.next_free_idx = save_free;
        s.max_seq = save_seq;     /* seq da dung o bank hong khong con y nghia */
        s.committed = save_com;
        return false;
    }

    s.committed = true;
    return true;
}

/* Bank chua co COMMIT (may moi): dam bao bank trong roi ghi COMMIT dau tien */
static bool ensure_committed(void)
{
    if (s.committed) return true;

    cfg_record_t r;
    if (!rec_read(s.bank, 0u, &r) || !rec_is_erased(&r)) {
        if (!Flash_EraseSector(OTA_CFG_SECTOR(s.bank)).ok) return false;
    }
    s.next_free_idx = 0u;

    const uint8_t dummy = 0xFFu;
    if (!append(CFG_REC_TYPE_COMMIT, &dummy, 1u)) return false;
    s.committed = true;
    return true;
}

static bool write_record(uint8_t type, const void *payload, uint8_t len)
{
    if (len > CFG_PAYLOAD_MAX) return false;

    if (!ensure_committed()) return false;

    if (s.next_free_idx >= CFG_REC_COUNT && !compact()) return false;

    if (!append(type, payload, len)) {
        /* 2 o lien tiep loi: thu chuyen bank mot lan */
        if (!compact() || !append(type, payload, len)) return false;
    }
    cache_apply(&s.c, type, payload, len);
    return true;
}

/* ==========================================================================
 * API
 * ========================================================================== */
bool CfgStore_NeedsErase(void)
{
    return !s.committed || (s.next_free_idx >= CFG_REC_COUNT);
}

bool CfgStore_ReadBoot(ota_slot_t sl, cfg_boot_payload_t *out)
{
    if (!Ota_SlotValid(sl) || !s.c.has_boot[sl]) return false;
    *out = s.c.boot[sl];
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
    if (!s.c.has_limits) return false;
    *out = s.c.limits;
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
