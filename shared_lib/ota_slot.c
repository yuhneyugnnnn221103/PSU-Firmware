#include "ota_image.h"
#include "crc32_sw.h"
#include "flash_guard.h"

const ota_slot_desc_t g_ota_slots[OTA_SLOT_COUNT] = {
		[OTA_SLOT_A] = { OTA_SLOTA_BASE, OTA_SLOTA_SECTOR, 'A' },
		[OTA_SLOT_B] = { OTA_SLOTB_BASE, OTA_SLOTB_SECTOR, 'B' },
};

extern uint32_t g_pfnVectors;            /* startup_stm32h725xx.s */

ota_slot_t Ota_SlotRunning(void)
{
	const uint32_t vt = (uint32_t)&g_pfnVectors;
	for (ota_slot_t s = OTA_SLOT_A; s < OTA_SLOT_COUNT; s++) {
		if (vt == Ota_CodeBase(s)) {
			return s;
		}
	}

	return OTA_SLOT_NONE;
}

ota_slot_t Ota_SlotFromTag(char tag)
{
	for (ota_slot_t s = OTA_SLOT_A; s < OTA_SLOT_COUNT; s++) {
		if (g_ota_slots[s].tag == tag) {
			return s;
		}
	}

	return OTA_SLOT_NONE;
}

bool Ota_ImageValid(ota_slot_t s)
{
    if (!Ota_SlotValid(s)) return false;

    const uint32_t lo = g_ota_slots[s].base;
    const uint32_t hi = lo + OTA_SLOT_SIZE;

    /* 5 word dau header: magic, version, size, crc32, install_seq */
    uint32_t hw[5];
    FlashGuard_Begin(lo, hi);
    FlashGuard_CopyWords(hw, lo, sizeof(hw));
    if (!FlashGuard_End()) return false;

    const uint32_t magic = hw[0], size = hw[2], crc_hdr = hw[3];
    if (magic != OTA_IMG_MAGIC || size == 0u || size > OTA_IMG_MAX_SIZE) return false;

    uint32_t       buf[64];                               /* 256 byte / khoi */
    uint32_t       crc  = CRC32_INIT;
    const uint32_t code = Ota_CodeBase(s);

    for (uint32_t off = 0; off < size; off += sizeof(buf)) {
        const uint32_t n = ((size - off) < sizeof(buf)) ? (size - off) : sizeof(buf);

        FlashGuard_Begin(lo, hi);
        FlashGuard_CopyWords(buf, code + off, (n + 3u) & ~3u);
        if (!FlashGuard_End()) return false;              /* image hong ECC */

        crc = Crc32_Update(crc, (const uint8_t *)buf, n);
    }
    return Crc32_Finalize(crc) == crc_hdr;
}

/* Vung RAM hop le cho MSP ban dau (STM32H725): DTCM 128 KB, AXI SRAM 320 KB */
#define RAM_DTCM_START      0x20000000u
#define RAM_DTCM_END        0x20020000u
#define RAM_AXI_START       0x24000000u
#define RAM_AXI_END         0x24050000u

bool Ota_VectorOk(ota_slot_t s)
{
    if (!Ota_SlotValid(s)) return false;

    const uint32_t base = Ota_CodeBase(s);
    const uint32_t hi   = g_ota_slots[s].base + OTA_SLOT_SIZE;

    uint32_t w[2];
    FlashGuard_Begin(base, hi);
    FlashGuard_CopyWords(w, base, sizeof(w));
    if (!FlashGuard_End()) return false;

    const uint32_t msp = w[0], entry = w[1];

    const bool msp_ok =
        ((msp & 3u) == 0u) &&
        (((msp > RAM_DTCM_START) && (msp <= RAM_DTCM_END)) ||
         ((msp > RAM_AXI_START)  && (msp <= RAM_AXI_END)));

    const bool entry_ok =
        ((entry & 1u) != 0u) &&                     /* bit Thumb */
        ((entry & ~1u) >= base) && ((entry & ~1u) < hi);

    return msp_ok && entry_ok;
}

bool Ota_ReadHeader(ota_slot_t s, ota_image_header_t *out)
{
    if (!Ota_SlotValid(s)) return false;

    const uint32_t lo = g_ota_slots[s].base;
    FlashGuard_Begin(lo, lo + OTA_SLOT_SIZE);
    FlashGuard_CopyWords(out, lo, sizeof(*out));
    return FlashGuard_End();
}
