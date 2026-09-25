#include <comm.h>
#include <fw_update.h>
#include <safety.h>
#include <string.h>
#include "main.h"

#include "cfg_store.h"
#include "crc16_ccitt.h"

extern UART_HandleTypeDef huart1;   /* RS422 #1, DE = PC6, RE = PC9    */
extern UART_HandleTypeDef huart5;   /* RS422 #2, DE = PB2, RE = PB10   */
extern UART_HandleTypeDef huart4;   /* FT232,  CTS# = PA2, RTS# = PA3  */

/* Nguong "fresh" rieng cua telemetry: chat hon INA228_STALE_MS (500 ms).
 * Voi chu ky 60 ms, 500 ms nghia la mot kenh chet van bao tuoi trong 8 khung.
 * 2.5 chu ky bat duoc trong 2 khung, van du dung sai cho mot lan quet truot. */
#define TLM_FRESH_MS   150u

/* ==========================================================================
 * BUFFER
 * ========================================================================== */

/* Cung section voi buffer cua driver: ngoai DTCM, can 32 de MPU phu duoc. */
#if defined(__GNUC__)
  #define TLM_DMA_ATTR  __attribute__((section(INA228_DMA_SECTION), aligned(32)))
#elif defined(__ICCARM__)
  #define TLM_DMA_ATTR  _Pragma("location=\".dma_buffer\"")
#else
  #define TLM_DMA_ATTR
#endif

/* Ping-pong: dong khung moi trong khi khung cu con dang duoc DMA doc ra. */
static TLM_DMA_ATTR uint8_t s_frame[2][TLM_FRAME_SZ];
static volatile uint8_t     s_build_idx = 0;
static volatile uint8_t     s_send_idx  = 1;

// ACK/ EVENT
static TLM_DMA_ATTR uint8_t s_aux[2][TLM_AUX_FRAME_SZ];
static volatile uint8_t     s_aux_idx;

static tlm_stats_t s_stats;

static volatile uint8_t s_dcm_fault;

static uint32_t s_last_rx_ms;

/* *************************
 * Luồng RX DMA Circular
 * ************************* */
#define TLM_RX_BUF_SZ   512u

static TLM_DMA_ATTR uint8_t s_rxbuf[TLM_PORT_COUNT][TLM_RX_BUF_SZ];

typedef struct {
	uint16_t tail;
	uint8_t  frm[TLM_RX_MAX_SZ];
	uint16_t idx;
	uint16_t len;
} rx_port_t;

static rx_port_t s_rx[TLM_PORT_COUNT];

_Static_assert((TLM_RX_BUF_SZ % 32u) == 0u,
               "TLM_RX_BUF_SZ phai la boi 32 de giu align DMA cho tung cong");

/* ==========================================================================
 * CONG PHAT
 * ========================================================================== */

typedef struct {
    UART_HandleTypeDef *huart;
    GPIO_TypeDef       *de_port;
    uint16_t            de_pin;
    bool                use_de;
    bool                check_rts;
    volatile bool       busy;

    volatile uint32_t   tx_start_ms;	// check timeout/watchdog
} tlm_port_t;

typedef struct {
    volatile bool pending;
    uint8_t  port;
    uint8_t  ch;
    bool     read_only;
    float    sovl_a, bovl_v, buvl_v;
} cfg_req_t;

static cfg_req_t s_cfg_req;

static tlm_port_t s_port[TLM_PORT_COUNT] = {
    [TLM_PORT_RS422_1] = { &huart5, RS422_DE1_GPIO_Port, RS422_DE1_Pin, true,  false, false, 0 },
    [TLM_PORT_RS422_2] = { &huart1, RS422_DE2_GPIO_Port, RS422_DE2_Pin, true,  false, false, 0 },
    [TLM_PORT_FT232  ] = { &huart4, NULL, 0, false, true, false, 0 },
};

/* ==========================================================================
 * ========================================================================== */
static inline void put_be(uint8_t *d, uint64_t v, uint8_t n)
{
    for (uint8_t i = 0; i < n; i++) {
        d[i] = (uint8_t)(v >> (8u * (n - 1u - i)));
    }
}

/* Doc float32 big-endian tu buffer */
static float get_be_f32(const uint8_t *b)
{
    union { uint32_t u; float f; } cv;
    cv.u = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] <<  8) | ((uint32_t)b[3]);
    return cv.f;
}

/* Doc uint32/uint16 big-endian - dung cho khung FW_* (size, crc32,
 * version, seq, chunk_len). */
static uint32_t get_be_u32(const uint8_t *b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] <<  8) | ((uint32_t)b[3]);
}

static uint16_t get_be_u16(const uint8_t *b)
{
    return (uint16_t)(((uint16_t)b[0] << 8) | (uint16_t)b[1]);
}
/* ==========================================================================
 * DOC CHAN LOI FT TU DCM
 * Binh thuong pull-up = 1, co loi = 0 -> dao lai de bit 1 nghia la CO LOI.
 * ========================================================================== */
void dcm_poll(void)                      /* goi moi 10 ms */			// check lỗi chân FT, để tạm trong comm
{
    static uint8_t hist[3];
    static uint8_t idx;
    uint8_t raw = 0;

    if (HAL_GPIO_ReadPin(FT_SEC1_GPIO_Port, FT_SEC1_Pin) == GPIO_PIN_RESET) raw |= 0x1u;
    if (HAL_GPIO_ReadPin(FT_SEC2_GPIO_Port, FT_SEC2_Pin) == GPIO_PIN_RESET) raw |= 0x2u;
    if (HAL_GPIO_ReadPin(FT_SEC3_GPIO_Port, FT_SEC3_Pin) == GPIO_PIN_RESET) raw |= 0x4u;
    if (HAL_GPIO_ReadPin(FT_SEC4_GPIO_Port, FT_SEC4_Pin) == GPIO_PIN_RESET) raw |= 0x8u;

    hist[idx] = raw;
    idx = (uint8_t)((idx + 1u) % 3u);

    /* bo phieu da so tren tung bit */
    s_dcm_fault = (uint8_t)((hist[0] & hist[1]) | (hist[1] & hist[2]) | (hist[0] & hist[2]));
}
/* ==========================================================================
 * DONG KHUNG
 * ========================================================================== */
static void tlm_dispatch(void);
static bool cfg_send_ack(uint8_t port, uint8_t ch, uint8_t status,
                         const ina228_dev_t *d);
static bool fw_send_ack(int port, uint8_t ack_cmd, uint8_t status,
                        uint32_t info);
static bool fw_send_info(int port);

static void tlm_build(uint32_t now_ms)
{
    uint8_t  *f     = s_frame[s_build_idx];
    uint8_t   fresh = 0;
    uint16_t  crc;

    for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
        ina228_dev_t *d = ina228_get_dev(i);
        uint8_t      *b = &f[TLM_PAYLOAD_OFF + i * TLM_CH_BLOCK_SZ];

        if (d == NULL) { memset(b, 0, TLM_CH_BLOCK_SZ); s_stats.ch_stale++; continue; }

        /* DIAG luon gui, ke ca khi so do luong da cu */
        put_be(&b[TLM_OFF_DIAG], (uint64_t)(d->diag_sticky), 2);

        if ((uint32_t)(now_ms - d->meas.timestamp_ms) >= TLM_FRESH_MS) {
            memset(b, 0, TLM_OFF_DIAG);       /* chi xoa 10 byte do luong */
            s_stats.ch_stale++;
            continue;
        }
        fresh |= (uint8_t)(1u << i);

        put_be(&b[TLM_OFF_CURRENT], (uint64_t)(uint32_t)d->raw.current_raw,	4);
        put_be(&b[TLM_OFF_VBUS],    (uint64_t)d->raw.vbus_raw,	4);
        put_be(&b[TLM_OFF_DIETEMP], (uint64_t)(uint16_t)d->raw.dietemp_raw, 2);
    }

    f[0] = TLM_HDR1;
    f[1] = TLM_HDR2;
    f[2] = TLM_CMD_TELEMETRY;
    f[3] = (uint8_t)((fresh & TLM_ST_FRESH_Msk) | ((s_dcm_fault & 0x0F) << TLM_ST_DCM_FAULT_Pos));

    crc = Crc16_Compute(&f[TLM_CRC_START], TLM_CRC_LEN);

    put_be(&f[TLM_CRC_OFF], crc, 2);

    f[TLM_FRAME_SZ - 2] = TLM_TAIL1;
    f[TLM_FRAME_SZ - 1] = TLM_TAIL2;

    s_stats.frames_built++;

    s_send_idx  = s_build_idx;
    s_build_idx ^= 1u;

    tlm_dispatch();
}

/* ==========================================================================
 * PHAT
 * ========================================================================== */

static void tlm_port_release(tlm_port_t *p)
{
    if (p->use_de) {
        HAL_GPIO_WritePin(p->de_port, p->de_pin, GPIO_PIN_RESET);
    }
    p->busy = false;
}

static void tlm_tx_start(tlm_port_t *p, const uint8_t *buf)
{
    if (p->use_de) {
        HAL_GPIO_WritePin(p->de_port, p->de_pin, GPIO_PIN_SET);
        for (volatile int i = 0; i < 200; i++) { __NOP(); }
    }

    p->busy = true;
    p->tx_start_ms = HAL_GetTick();

    if (HAL_UART_Transmit_DMA(p->huart, (uint8_t *)buf, TLM_FRAME_SZ) != HAL_OK) {
        tlm_port_release(p);
    }
}

static void tlm_dispatch(void)
{
    const uint8_t *buf = s_frame[s_send_idx];

    for (uint8_t i = 0; i < TLM_PORT_COUNT; i++) {
        tlm_port_t *p = &s_port[i];

        if (p->busy) {
            s_stats.tx_skipped[i]++;
            continue;
        }

        /* Doc RTS# cua FT232 truoc khi phat. */
        if (p->check_rts &&
            HAL_GPIO_ReadPin(FT232_RTS_GPIO_Port, FT232_RTS_Pin) == GPIO_PIN_SET) {
            s_stats.tx_skipped[i]++;
            continue;
        }

        tlm_tx_start(p, buf);
        s_stats.tx_sent[i]++;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < TLM_PORT_COUNT; i++) {
        if (s_port[i].huart == huart) {
            tlm_port_release(&s_port[i]);
            return;
        }
    }
}

static void rx_rearm(uint8_t i)
{
    tlm_port_t *p = &s_port[i];
    s_rx[i].tail = 0;
    s_rx[i].idx  = 0;
    __HAL_UART_CLEAR_OREFLAG(p->huart);
    if (HAL_UART_Receive_DMA(p->huart, s_rxbuf[i], TLM_RX_BUF_SZ) != HAL_OK)
        s_stats.rx_uart_err[i]++;
}

/* HAL goi tu ngat UART hoac tu ngat DMA khi co loi. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < TLM_PORT_COUNT; i++) {
        if (s_port[i].huart != huart) continue;
        uint32_t err = huart->ErrorCode;

        if (err & (HAL_UART_ERROR_ORE | HAL_UART_ERROR_FE |
                   HAL_UART_ERROR_NE  | HAL_UART_ERROR_PE)) {
            s_stats.rx_uart_err[i]++;
        }
        if (err & HAL_UART_ERROR_DMA) {
            /* Chi abort TX neu dung la TX dang chay */
            if (s_port[i].busy) {
                s_stats.tx_error[i]++;
                (void)HAL_UART_AbortTransmit_IT(huart);
                tlm_port_release(&s_port[i]);
            }
        }
        huart->ErrorCode = HAL_UART_ERROR_NONE;

        /* Neu HAL da ha RX xuong -> dung len lai */
        if (huart->RxState != HAL_UART_STATE_BUSY_RX) rx_rearm(i);
        return;
    }
}

/* ==========================================================================
 * CONG THU
 * ========================================================================== */
void Comm_RxStart(void)
{
    for (uint8_t i = 0; i < TLM_PORT_COUNT; i++) {
        s_rx[i].tail = 0;
        s_rx[i].idx  = 0;
        memset(s_rxbuf[i], 0, TLM_RX_BUF_SZ);

        /* Circular: HAL tu dat CIRC trong hdma, khong bao gio phai restart */
        if (HAL_UART_Receive_DMA(s_port[i].huart, s_rxbuf[i], TLM_RX_BUF_SZ) != HAL_OK) {
            /* Khong Error_Handler: mat mot cong RX khong phai loi chi mang,
             * telemetry van phat binh thuong. */
            s_stats.rx_uart_err[i]++;
        }
    }
}

/* Do dai khung theo CMD, quyet dinh ngay khi doc duoc byte thu 3 */
static uint16_t rx_frame_len(uint8_t cmd)
{
    switch (cmd) {
    case TLM_CMD_PWR_CTRL:  return TLM_RX_FRAME_SZ;
    case TLM_CMD_SET_LIMIT: return TLM_CFG_FRAME_SZ;
    case TLM_CMD_CLR_FAULT: return TLM_RX_FRAME_SZ;
    case TLM_CMD_GET_LIMIT: return TLM_RX_FRAME_SZ;

    case TLM_CMD_FW_BEGIN:  return TLM_FW_BEGIN_SZ;
    case TLM_CMD_FW_DATA:   return TLM_FW_DATA_FRAME_MAX;
    case TLM_CMD_FW_END:    return TLM_FW_CTRL_SZ;
    case TLM_CMD_FW_COMMIT: return TLM_FW_CTRL_SZ;
    case TLM_CMD_FW_INFO:   return TLM_FW_CTRL_SZ;
    case TLM_CMD_FW_ROLLBACK: return TLM_FW_CTRL_SZ;
    default:                return 0u;                   /* CMD la -> bo */
    }
}

/* Vi tri DMA dang ghi toi. NDTR dem NGUOC ve 0. */
static inline uint16_t rx_head(uint8_t p)
{
	if (s_port[p].huart->hdmarx == NULL) return 0;

    return (uint16_t)(TLM_RX_BUF_SZ - __HAL_DMA_GET_COUNTER(s_port[p].huart->hdmarx));
}

/* Xu ly mot khung da xac thuc header + tailer */
static void rx_exec(uint8_t p, const uint8_t *f, uint16_t n)
{
    uint16_t crc_off = (uint16_t)(n - 4u);
    uint16_t crc_rx  = (uint16_t)(((uint16_t)f[crc_off] << 8) | f[crc_off + 1u]);
    uint16_t crc_ok  = Crc16_Compute(&f[TLM_RX_CRC_START], (uint32_t)(crc_off - TLM_RX_CRC_START));

    if (crc_rx != crc_ok) {
        s_stats.rx_badcrc[p]++;
        return;
    }

    s_stats.rx_frames[p]++;
    s_last_rx_ms = HAL_GetTick();

    switch (f[TLM_RX_OFF_CMD]) {

    case TLM_CMD_PWR_CTRL:
        if (!Safety_RequestPower(f[TLM_RX_OFF_CTRL] != 0u)) {
            /* Bi tu choi vi dang TRIPPED/LOCKOUT. */
            (void)cfg_send_ack(p, 0xFFu, TLM_ACK_REFUSED, NULL);
        }
        break;

    case TLM_CMD_SET_LIMIT:
        s_cfg_req.port      = p;
        s_cfg_req.ch        = f[TLM_CFG_OFF_CH];
        s_cfg_req.read_only = false;
        s_cfg_req.sovl_a    = get_be_f32(&f[TLM_CFG_OFF_SOVL]);
        s_cfg_req.bovl_v    = get_be_f32(&f[TLM_CFG_OFF_BOVL]);
        s_cfg_req.buvl_v    = get_be_f32(&f[TLM_CFG_OFF_BUVL]);
        s_cfg_req.pending   = true;      /* ghi cuoi cung: barrier tu nhien */
        break;

    case TLM_CMD_GET_LIMIT:
        s_cfg_req.port      = p;
        s_cfg_req.ch        = f[TLM_RX_OFF_CTRL];
        s_cfg_req.read_only = true;
        s_cfg_req.pending   = true;
        break;

    case TLM_CMD_CLR_FAULT:
        (void)f[TLM_RX_OFF_CTRL];   /* mask: giu lai trong khung cho tuong thich, khong dung */
        Safety_ClearFault();
        break;

    case TLM_CMD_FW_BEGIN: {
        uint32_t info = 0u;
        const uint32_t total  = get_be_u32(&f[TLM_FW_BEGIN_OFF_SIZE]);
        const uint32_t crc32t = get_be_u32(&f[TLM_FW_BEGIN_OFF_CRC32]);
        const uint32_t ver    = get_be_u32(&f[TLM_FW_BEGIN_OFF_VERSION]);
        const uint8_t  st     = FwUpdate_Begin(total, crc32t, ver, &info);
        (void)fw_send_ack(p, TLM_CMD_FW_BEGIN, st, info);
        break;
    }

    case TLM_CMD_FW_DATA: {
        uint32_t info = 0u;
        const uint16_t seq = (uint16_t)get_be_u16(&f[TLM_FW_DATA_OFF_SEQ]);
        const uint16_t len = (uint16_t)get_be_u16(&f[TLM_FW_DATA_OFF_LEN]);
        const uint8_t  st  = FwUpdate_Data(seq, len,
                                           &f[TLM_FW_DATA_OFF_PAYLOAD], &info);
        (void)fw_send_ack(p, TLM_CMD_FW_DATA, st, info);
        break;
    }

    case TLM_CMD_FW_END: {
        uint32_t info = 0u;
        const uint8_t st = FwUpdate_End(&info);
        (void)fw_send_ack(p, TLM_CMD_FW_END, st, info);
        break;
    }

    case TLM_CMD_FW_COMMIT: {
        if (FwUpdate_CanCommit()) {
            (void)fw_send_ack(p, TLM_CMD_FW_COMMIT, TLM_FW_ACK_OK, 0u);
            FwUpdate_Commit();
        } else {
            (void)fw_send_ack(p, TLM_CMD_FW_COMMIT, TLM_FW_ACK_NO_COMMIT_PENDING, 0u);
        }
        break;
    }

    case TLM_CMD_FW_INFO:
        (void)fw_send_info(p);
        break;

    case TLM_CMD_FW_ROLLBACK:
    	(void) FwUpdate_RequestRollBack();
    	break;

    default:
        s_stats.rx_badframe[p]++;
        break;
    }
}

/* Nap tung byte vao may trang thai. Tu dong bat lai khi lech khung. */
static void rx_feed(uint8_t p, uint8_t c)
{
    uint16_t k = s_rx[p].idx;

    switch (k) {
    case 0:  /* cho HDR1 */
        if (c == TLM_HDR1) { s_rx[p].frm[0] = c; s_rx[p].idx = 1; }
        return;

    case 1:  /* cho HDR2 */
        if (c == TLM_HDR2) { s_rx[p].frm[1] = c; s_rx[p].idx = 2; }
        /* "AB AB CD" van phai bat duoc -> giu idx = 1 neu lai la HDR1 */
        else if (c != TLM_HDR1) { s_rx[p].idx = 0; }
        return;

    case 2: /* cho CMD */
    	s_rx[p].len = rx_frame_len(c);
		if (s_rx[p].len == 0u) {             /* CMD khong biet -> vut, bat lai */
			s_stats.rx_badframe[p]++;
			s_rx[p].idx = 0;
			return;
		}
		s_rx[p].frm[2] = c;
		s_rx[p].idx    = 3;
		return;

    default:
    	if (k >= TLM_RX_MAX_SZ) {
    		s_rx[p].idx = 0;
    		s_stats.rx_badframe[p]++;
    		return;
    	}
        s_rx[p].frm[k] = c;
        s_rx[p].idx    = (uint16_t)(k + 1u);
        break;
    }

    uint16_t n = s_rx[p].len;
    if(s_rx[p].idx < n) return;

    s_rx[p].idx = 0;
    if (s_rx[p].frm[n - 2u] == TLM_TAIL1 && s_rx[p].frm[n - 1u] == TLM_TAIL2) {
        rx_exec(p, s_rx[p].frm, n);
    } else {
        s_stats.rx_badframe[p]++;
    }
}

void Comm_RxPoll(void)
{
    for (uint8_t p = 0; p < TLM_PORT_COUNT; p++) {
        uint16_t head = rx_head(p);
        uint16_t tail = s_rx[p].tail;

        uint16_t avail = (uint16_t)((head - tail) & (TLM_RX_BUF_SZ - 1u));
        if (avail > (TLM_RX_BUF_SZ - TLM_RX_FRAME_SZ)) {
            s_stats.rx_overrun[p]++;
        }

        while (tail != head) {
            rx_feed(p, s_rxbuf[p][tail]);
            tail = (uint16_t)((tail + 1u) & (TLM_RX_BUF_SZ - 1u));
            s_stats.rx_bytes[p]++;
        }
        s_rx[p].tail = tail;
    }
}

/* ==========================================================================
 * API
 * ========================================================================== */

void Comm_Init(void)
{
    memset(s_frame, 0, sizeof(s_frame));
    memset(&s_stats, 0, sizeof(s_stats));
    s_build_idx = 0;
    s_send_idx  = 1;

    for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
        ina228_dev_t *d = ina228_get_dev(i);
        if (d != NULL) d->meas.timestamp_ms = HAL_GetTick() - TLM_FRESH_MS;
    }

    /* RE cua LTM2881 tich cuc muc thap. RS422 la full-duplex nen bo thu
     * khong bao gio can tat -> giu bat vinh vien. */
    HAL_GPIO_WritePin(RS422_RE2_GPIO_Port, RS422_RE2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RS422_RE1_GPIO_Port, RS422_RE1_Pin, GPIO_PIN_RESET);

    /* DE tat luc nghi */
    HAL_GPIO_WritePin(RS422_DE2_GPIO_Port, RS422_DE2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RS422_DE1_GPIO_Port, RS422_DE1_Pin, GPIO_PIN_RESET);

    /* PA2 = CTS# gui toi FT232, giu LOW = STM luon san sang nhan */
    HAL_GPIO_WritePin(FT232_CTS_GPIO_Port, FT232_CTS_Pin, GPIO_PIN_RESET);

    Comm_RxStart();
}

void Comm_OnTick(void)
{
	if (FwUpdate_IsBusy()) return;

    if (ina228_bus_start() == INA228_ERR_BUSY) {
        s_stats.scan_busy++;
    }
}

void Comm_Poll(uint32_t now_ms)
{
	if (FwUpdate_IsBusy()) return;

    if (ina228_bus_state() != INA228_BUS_DONE) return;

    ina228_bus_collect(NULL);

    tlm_build(now_ms);
}

void Comm_TxWatchdog(uint32_t now_ms)
{
    for (uint8_t i = 0; i < TLM_PORT_COUNT; i++) {
        tlm_port_t *p = &s_port[i];

        if (!p->busy) continue;
        if ((uint32_t)(now_ms - p->tx_start_ms) < TLM_TX_TIMEOUT_MS) continue;

        s_stats.tx_stuck[i]++;
        (void)HAL_UART_AbortTransmit_IT(p->huart);
        p->huart->ErrorCode = HAL_UART_ERROR_NONE;
        tlm_port_release(p);
    }
}

/* ==========================================================================
 * ACK
 * ========================================================================== */
static uint8_t *aux_acquire(void)
{
    for (uint8_t i = 0; i < TLM_PORT_COUNT; i++) {
        if (s_port[i].busy) return NULL;
    }
    uint8_t *f = s_aux[s_aux_idx];
    s_aux_idx ^= 1u;
    return f;
}

static void aux_seal(uint8_t *f)
{
    put_be(&f[TLM_AUX_FRAME_SZ - 4u],
           Crc16_Compute(&f[TLM_CRC_START], TLM_AUX_FRAME_SZ - 4u - TLM_CRC_START), 2);
    f[TLM_AUX_FRAME_SZ - 2] = TLM_TAIL1;
    f[TLM_AUX_FRAME_SZ - 1] = TLM_TAIL2;
}

static uint8_t tlm_send_aux(const uint8_t *buf, uint8_t len, int port)
{
    uint8_t sent = 0;
    for (uint8_t i = 0; i < TLM_PORT_COUNT; i++) {
        if (port >= 0 && (uint8_t)port != i) continue;

        tlm_port_t *p = &s_port[i];
		if (p->busy) { s_stats.tx_skipped[i]++; continue; }
		if (p->check_rts &&
			HAL_GPIO_ReadPin(FT232_RTS_GPIO_Port, FT232_RTS_Pin) == GPIO_PIN_SET) {
			s_stats.tx_skipped[i]++; continue;
		}

		if (p->use_de) {
			HAL_GPIO_WritePin(p->de_port, p->de_pin, GPIO_PIN_SET);
			for (volatile int k = 0; k < 200; k++) { __NOP(); }
		}
		p->busy        = true;
		p->tx_start_ms = HAL_GetTick();

        if (HAL_UART_Transmit_DMA(p->huart, (uint8_t *)buf, len) != HAL_OK) {
            tlm_port_release(p);
        } else {
            s_stats.tx_sent[i]++;
            sent++;
        }
    }
    return sent;
}

static bool cfg_send_ack(uint8_t port, uint8_t ch, uint8_t status,
                         const ina228_dev_t *d)
{
    uint8_t *f = aux_acquire();
    if (f == NULL) return false;

    memset(f, 0, TLM_AUX_FRAME_SZ);
    f[0] = TLM_HDR1;
    f[1] = TLM_HDR2;
    f[2] = TLM_CMD_LIMIT_ACK;
    f[TLM_ACK_OFF_CH]     = ch;
    f[TLM_ACK_OFF_STATUS] = status;

    if (d != NULL) {
        put_be(&f[TLM_ACK_OFF_SOVL], d->limits.sovl, 2);
        put_be(&f[TLM_ACK_OFF_BOVL], d->limits.bovl, 2);
        put_be(&f[TLM_ACK_OFF_BUVL], d->limits.buvl, 2);
    }

    aux_seal(f);
    return (tlm_send_aux(f, TLM_AUX_FRAME_SZ, (int)port) > 0u);
}

/* FW_ACK dung offset RIENG (ADDR o offset 3) khac voi cfg_send_ack o tren,
 * nhung tai su dung nguyen ven aux_acquire()/aux_seal()/tlm_send_aux() -
 * ca 3 chi quan tam kich thuoc buffer (TLM_AUX_FRAME_SZ), khong quan tam
 * bo cuc ben trong. */
static bool fw_send_ack(int port, uint8_t ack_cmd, uint8_t status,
                        uint32_t info)
{
    uint8_t *f = aux_acquire();
    if (f == NULL) return false;

    memset(f, 0, TLM_AUX_FRAME_SZ);
    f[0] = TLM_HDR1;
    f[1] = TLM_HDR2;
    f[2] = TLM_CMD_FW_ACK;
    f[TLM_FW_OFF_ADDR]     = TLM_BOARD_ADDR;
    f[TLM_FW_ACK_OFF_CMD]  = ack_cmd;
    f[TLM_FW_ACK_OFF_STATUS] = status;
    put_be(&f[TLM_FW_ACK_OFF_INFO], info, 4);

    aux_seal(f);
    return (tlm_send_aux(f, TLM_AUX_FRAME_SZ, port) > 0u);
}

static bool fw_send_info(int port)
{
    uint8_t *f = aux_acquire();
    if (f == NULL) return false;

    memset(f, 0, TLM_AUX_FRAME_SZ);
    f[0] = TLM_HDR1;
    f[1] = TLM_HDR2;
    f[2] = TLM_CMD_FW_ACK;
    f[TLM_FW_OFF_ADDR]       = TLM_BOARD_ADDR;
    f[TLM_FW_ACK_OFF_CMD]    = TLM_CMD_FW_INFO;
    f[TLM_FW_ACK_OFF_STATUS] = TLM_FW_ACK_OK;
    put_be(&f[TLM_FW_ACK_OFF_INFO], FwUpdate_RunningVersion(), 4);
    f[TLM_FW_ACK_OFF_SLOT]   = (uint8_t)FwUpdate_RunningSlot();

    aux_seal(f);
    return (tlm_send_aux(f, TLM_AUX_FRAME_SZ, port) > 0u);
}

static void cfg_persist_limits(void)
{
    cfg_limits_payload_t p;

    for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
        const ina228_dev_t *d = ina228_get_dev(i);
        p.sovl[i] = d ? d->limits.sovl : 0u;
        p.bovl[i] = d ? d->limits.bovl : 0u;
        p.buvl[i] = d ? d->limits.buvl : 0u;
    }

    (void)CfgStore_WriteLimits(&p);
}

void Comm_CfgTask(void)
{
	if (FwUpdate_IsBusy()) return;

    if (!s_cfg_req.pending) return;
    if (ina228_bus_state() != INA228_BUS_IDLE) return;

    cfg_req_t req = s_cfg_req;

    ina228_dev_t *d = (req.ch < INA228_CH_COUNT) ? ina228_get_dev(req.ch) : NULL;
    uint8_t st;

    if (d == NULL) {
        st = (req.ch < INA228_CH_COUNT) ? TLM_ACK_NO_DEV : TLM_ACK_PARAM;
    } else if (req.read_only) {
        st = (ina228_read_limits(d) == INA228_OK) ? TLM_ACK_OK : TLM_ACK_I2C_ERR;
    } else {
        st = (ina228_set_limits_f(d, req.sovl_a, req.bovl_v, req.buvl_v)
              == INA228_OK) ? TLM_ACK_OK : TLM_ACK_I2C_ERR;

        if (st == TLM_ACK_OK) {
            cfg_persist_limits();
        }
    }

    if (cfg_send_ack(req.port, req.ch, st, d)) {
        s_cfg_req.pending = false;        /* <-- chi xoa khi ACK da di */
    }
}

/* ==========================================================================
 * SAFETY
 * ========================================================================== */
uint8_t Comm_DcmFault(void) { return s_dcm_fault; }

bool Comm_PcLinkAlive(uint32_t now_ms)
{
    if (s_last_rx_ms == 0u) return false;      /* chua tung nhan lenh nao */
    return ((uint32_t)(now_ms - s_last_rx_ms) < 3000u);
}

/* ==========================================================================
 * KHUNG STATUS 0x03
 * ========================================================================== */
void Comm_StatusTask(uint32_t now_ms)
{
	if (FwUpdate_IsBusy()) return;

    static uint32_t t_last;
    static uint8_t  last_state = 0xFFu;

    uint8_t st_now = (uint8_t)((((uint8_t)Safety_State() & 0x0Fu) << 4)
                             |  ((uint8_t)Safety_Code()  & 0x0Fu));

    bool due     = ((uint32_t)(now_ms - t_last) >= 1000u);
    bool changed = (st_now != last_state);
    if (!due && !changed) return;

    uint8_t *f = aux_acquire();
    if (f == NULL) return;               /* cong dang ban, thu lai vong sau */

    uint8_t fresh = 0, cfgok = 0;
    for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
        ina228_dev_t *d = ina228_get_dev(i);
        if (d == NULL) continue;
        if (ina228_is_fresh(d, now_ms)) fresh |= (uint8_t)(1u << i);
        if (d->cfg_ok)                  cfgok |= (uint8_t)(1u << i);
    }

    memset(f, 0, TLM_AUX_FRAME_SZ);
    f[0] = TLM_HDR1;
    f[1] = TLM_HDR2;
    f[2] = TLM_CMD_STATUS;
    f[TLM_STS_OFF_STATE]    = st_now;
    f[TLM_STS_OFF_TRIPMASK] = Safety_TripMask();
    f[TLM_STS_OFF_DCM]      = s_dcm_fault;
    f[TLM_STS_OFF_FRESH]    = fresh;
    f[TLM_STS_OFF_CFGOK]    = cfgok;
    f[TLM_STS_OFF_PWR]      = Safety_PowerIsOn() ? 1u : 0u;
    f[TLM_STS_OFF_TRIPCNT]  = Safety_TripCount();

    aux_seal(f);

    if (tlm_send_aux(f, TLM_AUX_FRAME_SZ, -1) > 0u) {
        t_last     = now_ms;
        last_state = st_now;
    }
}

/* ==========================================================================
 * OTA - THONG BAO XOA XONG
 */
void Comm_FwPoll(void)
{
    static fwu_state_t s_last = FWU_IDLE;

    const fwu_state_t st = FwUpdate_State();

    if (st == FWU_RECEIVING && s_last == FWU_ERASING) {

        (void)fw_send_ack(-1, TLM_CMD_FW_BEGIN, TLM_FW_ACK_OK, 1u);
    }

    {
        uint32_t err = 0u;
        if (FwUpdate_TakeEraseError(&err)) {
            (void)fw_send_ack(-1, TLM_CMD_FW_BEGIN, TLM_FW_ACK_ERASE_FAIL, err);
        }
    }

    s_last = st;
}
