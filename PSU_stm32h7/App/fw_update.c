#include <comm.h>
#include <fw_update.h>
#include <safety.h>
#include <string.h>
#include "ota_image.h"
#include "crc32_sw.h"
#include "cfg_store.h"
#include "flash.h"
#include "cfg_record.h"

extern uint32_t g_pfnVectors;

#define FWU_COMMIT_DELAY_MS   50u

#define FWU_RECEIVE_TIMEOUT_MS    5000u   /* qua han khong co FW_DATA moi */
#define FWU_VERIFIED_TIMEOUT_MS  15000u   /* qua han khong nhan FW_COMMIT */

/* ==========================================================================
 * TRANG THAI NOI BO
 * ========================================================================== */
static struct {
    fwu_state_t state;

    uint32_t declared_size;
    uint32_t declared_crc32;
    uint32_t version;

    uint32_t running_crc32;    /* dang chay, XOR bit-dao chuan CRC32       */
    uint32_t bytes_received;
    int32_t  last_seq;         /* -1 = chua nhan chunk nao trong phien nay */

    uint8_t  erase_idx;        /* so sector DA xoa xong trong phien nay    */
    uint8_t  erase_total;

    bool     commit_pending;
    uint32_t commit_at_ms;

    bool     erase_failed;      /* co loi xoa chua duoc bao cao ve PC */
    uint32_t erase_err_code;    /* HAL_FLASH_GetError() luc that bai   */

    uint32_t last_activity_ms;
    bool     timed_out;
} s;

static inline ota_slot_t my_slot(void) {
	return Ota_SlotRunning();
}

static inline ota_slot_t target_slot(void) {
	return Ota_Other(Ota_SlotRunning());
}

static inline uint32_t target_base(void) {
	return g_ota_slots[target_slot()].base;
}

/* ==========================================================================
 * API
 * ========================================================================== */

void FwUpdate_Init(void)
{
    s.state           = FWU_IDLE;
    s.commit_pending  = false;
}

fwu_state_t FwUpdate_State(void) { return s.state; }

uint32_t FwUpdate_RunningVersion(void)
{
	if (my_slot() == OTA_SLOT_NONE) {
		return 0u;
	}

	const ota_image_header_t *h = Ota_Header(my_slot());

	if(h->magic != OTA_IMG_MAGIC) {
		return 0u;
	}
	return h->version;
}

bool FwUpdate_TakeEraseError(uint32_t *out_err)
{
    if (!s.erase_failed) return false;

    if (out_err) *out_err = s.erase_err_code;
    s.erase_failed = false;
    return true;
}

uint8_t FwUpdate_Begin(uint32_t total_size, uint32_t crc32_total,
                       uint32_t version, uint32_t *out_info)
{
    if (my_slot() == OTA_SLOT_NONE) {     /* link sai dia chi -> KHONG xoa gi */
        if (out_info) *out_info = SCB->VTOR;
        return TLM_FW_ACK_WRONG_SLOT;
    }

    if (Safety_State() != SAFE_OFF) {
        return TLM_FW_ACK_REFUSED;
    }

    if ((uint32_t)&g_pfnVectors != Ota_CodeBase(my_slot())) {
        if (out_info) *out_info = (uint32_t)&g_pfnVectors;
        return TLM_FW_ACK_WRONG_SLOT;
    }

    if (s.state != FWU_IDLE) {
        return TLM_FW_ACK_BUSY;
    }
    if (total_size == 0u || total_size > OTA_IMG_MAX_SIZE) {
        if (out_info) *out_info = OTA_IMG_MAX_SIZE;
        return TLM_FW_ACK_BAD_SIZE;
    }

    s.declared_size  = total_size;
    s.declared_crc32 = crc32_total;
    s.version        = version;
    s.running_crc32  = CRC32_INIT;
    s.bytes_received = 0u;
    s.last_seq       = -1;

    s.erase_idx   = 0u;
    s.erase_total = (uint8_t)OTA_SLOT_SECTORS;
    s.erase_failed   = false;   /* vut loi ton dong tu phien truoc */
    s.erase_err_code = 0u;

    s.state = FWU_ERASING;   /* FwUpdate_Task() se xoa dan tung sector */

    s.last_activity_ms = HAL_GetTick();

    return TLM_FW_ACK_OK;
}

uint8_t FwUpdate_Data(uint16_t seq, uint16_t chunk_len,
                      const uint8_t *data, uint32_t *out_info)
{
    if (out_info) *out_info = 0u;

    if (s.state != FWU_RECEIVING) {
        return TLM_FW_ACK_BUSY;    /* van dang xoa, hoac chua FW_BEGIN */
    }
    if ((int32_t)seq != s.last_seq + 1) {
        /* Sai thu tu - bao PC seq dung tiep theo can gui la last_seq+1 */
        if (out_info) *out_info = (uint32_t)(s.last_seq < 0 ? 0 : (uint32_t)s.last_seq);
        return TLM_FW_ACK_BAD_SEQ;
    }

    const uint32_t off = (uint32_t)seq * TLM_FW_CHUNK_MAX;

    if (chunk_len == 0u || chunk_len > TLM_FW_CHUNK_MAX || (off + chunk_len) > s.declared_size) {
    	return TLM_FW_ACK_BAD_SIZE;
    }

    if ((chunk_len < TLM_FW_CHUNK_MAX) && (off + chunk_len) != s.declared_size) {
    	return TLM_FW_ACK_BAD_SIZE;
    }

	if (seq == 0u) {
		if (chunk_len < 8u) {
			return TLM_FW_ACK_WRONG_SLOT;   /* chunk dau qua ngan, khong doc noi vector */
		}
		const uint32_t reset_handler =
			((uint32_t)data[4])        | ((uint32_t)data[5] << 8) |
			((uint32_t)data[6] << 16)  | ((uint32_t)data[7] << 24);

		const uint32_t lo = target_base() + OTA_HEADER_SIZE;
		const uint32_t hi = target_base() + OTA_SLOT_SIZE;

		if (reset_handler < lo || reset_handler >= hi) {
			if (out_info) *out_info = reset_handler;   /* PC hien de chan doan */
			s.state = FWU_IDLE;   /* huy phien luon, khong nhan tiep */
			return TLM_FW_ACK_WRONG_SLOT;
		}
	}

	const uint32_t write_addr = target_base() + OTA_HEADER_SIZE + off;
	if ((write_addr + TLM_FW_CHUNK_MAX) > (target_base() + OTA_SLOT_SIZE))
		return TLM_FW_ACK_BAD_SIZE;

	static uint8_t chunk_buf[TLM_FW_CHUNK_MAX] __attribute__((aligned(32)));
	memcpy(chunk_buf, data, chunk_len);
	memset(chunk_buf + chunk_len, 0xFF, TLM_FW_CHUNK_MAX - chunk_len);

	flash_op_result_t r = Flash_ProgramWords(write_addr, chunk_buf, TLM_FW_CHUNK_MAX);

    if (!r.ok) {
        s.state = FWU_IDLE;
        return TLM_FW_ACK_CRC_FAIL;
    }

    /* CRC32 CHI phu chunk_len byte THAT (bo qua phan dem neu la chunk cuoi) */
    s.running_crc32   = Crc32_Update(s.running_crc32, data, chunk_len);
    s.bytes_received  += chunk_len;
    s.last_seq         = (int32_t)seq;

    if (out_info) *out_info = (uint32_t)s.last_seq;

    s.last_activity_ms = HAL_GetTick();

    return TLM_FW_ACK_OK;
}

static uint32_t hdr_max_install_seq(void)
{
    uint32_t m = 0u;
    for (ota_slot_t s = OTA_SLOT_A; s < OTA_SLOT_COUNT; s++) {
    	const ota_image_header_t *h = Ota_Header(s);
    	if (h->magic == OTA_IMG_MAGIC && h->install_seq >m) {
    		m = h->install_seq;
    	}
    }
    return m;
}

uint8_t FwUpdate_End(uint32_t *out_info)
{
    if (out_info) *out_info = 0u;

    if (s.state != FWU_RECEIVING) {
        return TLM_FW_ACK_BUSY;
    }

    const uint32_t final_crc = Crc32_Finalize(s.running_crc32);

    if (s.bytes_received != s.declared_size || final_crc != s.declared_crc32) {
        s.state = FWU_IDLE;
        if (out_info) *out_info = final_crc;
        return TLM_FW_ACK_CRC_FAIL;
    }

    ota_image_header_t hdr;
    hdr.magic       = OTA_IMG_MAGIC;
    hdr.version     = s.version;
    hdr.size        = s.declared_size;
    hdr.crc32       = s.declared_crc32;

    uint32_t seq = CfgStore_AllocInstallSeq();
    if (seq == 0u) {
    	s.state = FWU_IDLE;
    	return TLM_FW_ACK_REFUSED;
    }

    const uint32_t m = hdr_max_install_seq();
    if (seq <= m)
    	seq = m + 1;
    hdr.install_seq = seq;

    memset(hdr.reserved, 0xFF, sizeof(hdr.reserved));

    flash_op_result_t r = Flash_ProgramWords(target_base(), (uint8_t *)&hdr, OTA_HEADER_SIZE);
    if (!r.ok) {
        s.state = FWU_IDLE;
        return TLM_FW_ACK_CRC_FAIL;
    }

	if (!Ota_ImageValid(target_slot())) {
		s.state = FWU_IDLE;
		if (out_info) *out_info = Crc32_Compute((const uint8_t *)Ota_CodeBase(target_slot()), s.declared_size);
		return TLM_FW_ACK_CRC_FAIL;
	}

    s.state = FWU_VERIFIED;

    {
        (void)CfgStore_WriteBoot(target_slot(), 0u, 0u);
    }

    if (out_info) *out_info = 1u;

    s.last_activity_ms = HAL_GetTick();

    return TLM_FW_ACK_OK;
}

bool FwUpdate_CanCommit(void)
{
    return (s.state == FWU_VERIFIED);
}

void FwUpdate_Commit(void)
{
    s.commit_pending = true;
    s.commit_at_ms    = HAL_GetTick() + FWU_COMMIT_DELAY_MS;
}

void FwUpdate_Task(uint32_t now_ms)
{
    if (s.commit_pending) {
        if ((int32_t)(now_ms - s.commit_at_ms) >= 0) {
            NVIC_SystemReset();
        }
        return;
    }

    if (s.state == FWU_RECEIVING || s.state == FWU_VERIFIED) {
        uint32_t limit = (s.state == FWU_RECEIVING)
                        ? FWU_RECEIVE_TIMEOUT_MS : FWU_VERIFIED_TIMEOUT_MS;
        if ((uint32_t)(now_ms - s.last_activity_ms) >= limit) {
            s.state     = FWU_IDLE;
            s.timed_out = true;   /* Comm_FwPoll() se bao ve PC */
        }
    }

    if (s.state != FWU_ERASING) return;

    if (s.erase_idx >= s.erase_total) {
        s.state = FWU_RECEIVING;
        return;
    }

    const uint32_t sector_num = g_ota_slots[target_slot()].sector_first + s.erase_idx;

    flash_op_result_t r = Flash_EraseSector(sector_num);
    if (!r.ok) {
        s.erase_err_code = r.hal_error;
        s.erase_failed   = true;
        s.state          = FWU_IDLE;
        return;
    }

    s.erase_idx++;
}

bool FwUpdate_TakeTimeout(void)
{
    if (!s.timed_out) return false;
    s.timed_out = false;
    return true;
}

bool FwUpdate_RequestRollBack(void)
{
	if (Safety_State() != SAFE_OFF)	return false;
	if (s.state != FWU_IDLE)		return false;

	if (my_slot() == OTA_SLOT_NONE || !Ota_ImageValid(target_slot())) return false;

	if (!CfgStore_WriteBoot(my_slot(), CFG_BOOT_COUNT_FORCE_FAIL, 0u))	return false;

	s.commit_pending = true;
	s.commit_at_ms = HAL_GetTick() + FWU_COMMIT_DELAY_MS;
	return true;
}

char FwUpdate_RunningSlot(void)
{
    const ota_slot_t sl = my_slot();
    return Ota_SlotValid(sl) ? g_ota_slots[sl].tag : '?';
}

