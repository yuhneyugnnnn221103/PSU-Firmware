/**
 ******************************************************************************
 * @file    telemetry.c
 * @brief   Dong khung + fan-out 3 UART. Khong cham I2C.
 *
 * LUONG:
 *  TLM_OnTick() -> ina228_bus_start(true)
 *                                        | driver chay DMA ~2.6 ms
 *   main loop  -> TLM_Poll()   -> thay BUS_DONE
 *                                 -> ina228_bus_collect(NULL)   (nha bus)
 *                                 -> dong khung tu dev->raw
 *                                 -> phat DMA ra 3 cong
 *
 * TAI SAO LUON full = true:
 *   Khung can POWER va DIETEMP moi chu ky. s_seq_fast khong co chung, se
 *   khien hai truong nay cu toi INA228_FULL_SCAN_EVERY chu ky (~1 s).
 *   Chi phi them ~1 ms tren chu ky 60 ms -> khong dang de danh doi.
 ******************************************************************************
 */

#include <string.h>
#include "comm.h"
#include "main.h"

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

static tlm_stats_t s_stats;

static volatile uint8_t s_dcm_fault;

/* Luồng RX DMA Circular */
#define TLM_RX_BUF_SZ   256u

static TLM_DMA_ATTR uint8_t s_rxbuf[TLM_PORT_COUNT][TLM_RX_BUF_SZ];
static uint16_t s_rx_tail[TLM_PORT_COUNT];    /* vi tri da doc toi */

/* May trang thai tach khung, moi cong mot bo */
static uint8_t s_rx_frm[TLM_PORT_COUNT][TLM_RX_FRAME_SZ];
static uint8_t s_rx_idx[TLM_PORT_COUNT];

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

static tlm_port_t s_port[TLM_PORT_COUNT] = {
    [TLM_PORT_RS422_1] = { &huart5, RS422_DE1_GPIO_Port, RS422_DE1_Pin, true,  false, false, 0 },
    [TLM_PORT_RS422_2] = { &huart1, RS422_DE2_GPIO_Port, RS422_DE2_Pin, true,  false, false, 0 },
    [TLM_PORT_FT232  ] = { &huart4, NULL,  0,          false, true,  false, 0 },
};

/* ==========================================================================
 * CRC16-CCITT-FALSE  (poly 0x1021, init 0xFFFF, khong reflect, khong xorout)
 * 86 byte ~ 700 vong lap. Muon nhanh hon thi dung khoi CRC phan cung cua H7.
 * ========================================================================== */
//static uint16_t tlm_crc16(const uint8_t *p, uint32_t n)
//{
//    uint16_t crc = 0xFFFFu;
//    while (n--) {
//        crc ^= (uint16_t)(*p++) << 8;
//        for (uint8_t i = 0; i < 8; i++) {
//            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
//                                  : (uint16_t)(crc << 1);
//        }
//    }
//    return crc;
//}

/* ==========================================================================
 * ========================================================================== */

static inline void put_be(uint8_t *d, uint64_t v, uint8_t n)
{
    for (uint8_t i = 0; i < n; i++) {
        d[i] = (uint8_t)(v >> (8u * (n - 1u - i)));
    }
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

static void tlm_build(uint32_t now_ms)
{
    uint8_t  *f     = s_frame[s_build_idx];
    uint8_t   fresh = 0;
    uint16_t  crc;

    for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
        ina228_dev_t *d = ina228_get_dev(i);
        uint8_t      *b = &f[TLM_PAYLOAD_OFF + i * TLM_CH_BLOCK_SZ];

        /* Nguong rieng cua telemetry, khong dung ina228_is_fresh() vi
         * INA228_STALE_MS (500 ms) qua long cho chu ky 60 ms. */
        if (d == NULL ||
            (uint32_t)(now_ms - d->meas.timestamp_ms) >= TLM_FRESH_MS) {
            memset(b, 0, TLM_CH_BLOCK_SZ);
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
    f[3] = (uint8_t)((fresh & TLM_ST_FRESH_Msk) |
                     ((s_dcm_fault & 0x0F) << TLM_ST_DCM_FAULT_Pos));

//    crc = tlm_crc16(&f[TLM_CRC_START], TLM_CRC_LEN);
    crc = 0;
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

        /* Cong con ban -> bo khung nay, KHONG xep hang. Du lieu giam sat cu
         * khong con gia tri; khung moi den sau 60 ms. */
        if (p->busy) {
            s_stats.tx_skipped[i]++;
            continue;
        }

        /* PA2/PA3 la GPIO thuan, khong phai flow control phan cung.
         * Doc RTS# cua FT232 truoc khi phat. */
        if (p->check_rts &&
            HAL_GPIO_ReadPin(FT232_RTS_GPIO_Port, FT232_RTS_Pin) == GPIO_PIN_SET) {
            s_stats.tx_skipped[i]++;
            continue;
        }

        tlm_tx_start(p, buf);
        s_stats.tx_sent[i]++;
    }
}

/* HAL goi ham nay tu ngat USART TC. */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < TLM_PORT_COUNT; i++) {
        if (s_port[i].huart == huart) {
            tlm_port_release(&s_port[i]);
            return;
        }
    }
}

/* HAL goi tu ngat UART hoac tu ngat DMA khi co loi.
 * Voi cau hinh hien tai (chi TX DMA, khong co RX dang chay) nguyen nhan
 * thuc te la DMA transfer/FIFO error. Neu sau nay bat RX thi phai loc
 * them cac co ORE/FE/NE de khong nham loi RX thanh loi TX. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	for (uint8_t i = 0; i < TLM_PORT_COUNT; i++) {
	        if (s_port[i].huart != huart) continue;

	        uint32_t err = huart->ErrorCode;

	        /* --- Loi RX: KHONG duoc dung TX --- */
	        if (err & (HAL_UART_ERROR_ORE | HAL_UART_ERROR_FE |
	                   HAL_UART_ERROR_NE  | HAL_UART_ERROR_PE)) {
	            s_stats.rx_uart_err[i]++;
	            __HAL_UART_CLEAR_OREFLAG(huart);
	            __HAL_UART_CLEAR_FEFLAG(huart);
	            __HAL_UART_CLEAR_NEFLAG(huart);
	            huart->ErrorCode &= ~(HAL_UART_ERROR_ORE | HAL_UART_ERROR_FE |
	                                  HAL_UART_ERROR_NE  | HAL_UART_ERROR_PE);
	            /* DMA circular van chay - KHONG restart. */
	        }

	        /* --- Loi DMA: moi phai dung TX --- */
	        if (huart->ErrorCode & HAL_UART_ERROR_DMA) {
	            s_stats.tx_error[i]++;
	            (void)HAL_UART_AbortTransmit_IT(huart);
	            tlm_port_release(&s_port[i]);
	        }

	        huart->ErrorCode = HAL_UART_ERROR_NONE;
	        return;
	    }
}

/* ==========================================================================
 * CONG THU
 * ========================================================================== */
void Comm_RxStart(void)
{
    for (uint8_t i = 0; i < TLM_PORT_COUNT; i++) {
        s_rx_tail[i] = 0;
        s_rx_idx[i]  = 0;
        memset(s_rxbuf[i], 0, TLM_RX_BUF_SZ);

        /* Circular: HAL tu dat CIRC trong hdma, khong bao gio phai restart */
        if (HAL_UART_Receive_DMA(s_port[i].huart,
                                 s_rxbuf[i], TLM_RX_BUF_SZ) != HAL_OK) {
            /* Khong Error_Handler: mat mot cong RX khong phai loi chi mang,
             * telemetry van phat binh thuong. */
            s_stats.rx_uart_err[i]++;
        }
    }
}

/* Vi tri DMA dang ghi toi. NDTR dem NGUOC ve 0. */
static inline uint16_t rx_head(uint8_t p)
{
    return (uint16_t)(TLM_RX_BUF_SZ -
                      __HAL_DMA_GET_COUNTER(s_port[p].huart->hdmarx));
}

/* Xu ly mot khung da xac thuc header + tailer */
static void rx_exec(uint8_t p, const uint8_t *f)
{
    /* CRC: hien tai phia PC gui 0x0000. Bat buoc dung 0 de khi bat CRC
     * that. */
    uint16_t crc_rx = (uint16_t)(((uint16_t)f[TLM_RX_OFF_CRC] << 8) |
                                  f[TLM_RX_OFF_CRC + 1u]);
//  uint16_t crc_ok = tlm_crc16(&f[TLM_RX_CRC_START], TLM_RX_CRC_LEN);
    uint16_t crc_ok = 0u;
    if (crc_rx != crc_ok) {
        s_stats.rx_badcrc[p]++;
        return;
    }

    if (f[TLM_RX_OFF_CMD] != TLM_CMD_PWR_CTRL) {
        s_stats.rx_badframe[p]++;
        return;
    }

    s_stats.rx_frames[p]++;

    if (f[TLM_RX_OFF_CTRL] == 0) {
    	HAL_GPIO_WritePin(PWR_EN_CTRL_GPIO_Port, PWR_EN_CTRL_Pin, GPIO_PIN_RESET);
    }
    else {
    	HAL_GPIO_WritePin(PWR_EN_CTRL_GPIO_Port, PWR_EN_CTRL_Pin, GPIO_PIN_SET);
    }

//    Pwr_SetMask(f[TLM_RX_OFF_MASK] & 0x0Fu);////////
}

/* Nap tung byte vao may trang thai. Tu dong bat lai khi lech khung. */
static void rx_feed(uint8_t p, uint8_t c)
{
    uint8_t k = s_rx_idx[p];

    switch (k) {
    case 0:  /* cho HDR1 */
        if (c == TLM_HDR1) { s_rx_frm[p][0] = c; s_rx_idx[p] = 1; }
        return;

    case 1:  /* cho HDR2 */
        if (c == TLM_HDR2) { s_rx_frm[p][1] = c; s_rx_idx[p] = 2; }
        /* "AB AB CD" van phai bat duoc -> giu idx = 1 neu lai la HDR1 */
        else if (c != TLM_HDR1) { s_rx_idx[p] = 0; }
        return;

    default:
        s_rx_frm[p][k] = c;
        s_rx_idx[p]    = (uint8_t)(k + 1u);
        break;
    }

    if (s_rx_idx[p] < TLM_RX_FRAME_SZ) return;

    s_rx_idx[p] = 0;
    if (s_rx_frm[p][TLM_RX_FRAME_SZ - 2u] == TLM_TAIL1 &&
        s_rx_frm[p][TLM_RX_FRAME_SZ - 1u] == TLM_TAIL2) {
        rx_exec(p, s_rx_frm[p]);
    } else {
        s_stats.rx_badframe[p]++;
    }
}

void Comm_RxPoll(void)
{
    for (uint8_t p = 0; p < TLM_PORT_COUNT; p++) {
        uint16_t head = rx_head(p);
        uint16_t tail = s_rx_tail[p];

        /* Tràn: DMA da vong qua tail. Mat du lieu, nhung khong the lam gi
         * hon ngoai viec dem lai va tiep tuc - may trang thai se tu bat
         * lai o khung ke tiep. */
        uint16_t avail = (uint16_t)((head - tail) & (TLM_RX_BUF_SZ - 1u));
        if (avail > (TLM_RX_BUF_SZ - TLM_RX_FRAME_SZ)) {
            s_stats.rx_overrun[p]++;
        }

        while (tail != head) {
            rx_feed(p, s_rxbuf[p][tail]);
            tail = (uint16_t)((tail + 1u) & (TLM_RX_BUF_SZ - 1u));
            s_stats.rx_bytes[p]++;
        }
        s_rx_tail[p] = tail;
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

    /* Day timestamp ve qua khu: kenh chua tung phan hoi phai bi danh dau
     * "cu" ngay tu khung dau tien, thay vi duoc coi la tuoi trong
     * TLM_FRESH_MS dau sau boot (timestamp_ms khoi tao = 0).
     * An toan: ham nay chay o ngu canh task, truoc chu ky quet dau tien,
     * nen khong vi pham nguyen tac mot-nguoi-ghi cua driver. */
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
    if (ina228_bus_start() == INA228_ERR_BUSY) {
        s_stats.scan_busy++;
    }
}

void Comm_Poll(uint32_t now_ms)
{
    if (ina228_bus_state() != INA228_BUS_DONE) return;

    /* Nha bus ve IDLE. Truyen NULL vi ta doc thang dev->raw de giu nguyen
     * bit thanh ghi, khong dung ban float da quy doi. */
    ina228_bus_collect(NULL);

    tlm_build(now_ms);
}

//const tlm_stats_t *TLM_GetStats(void)
//{
//    return &s_stats;
//}

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


