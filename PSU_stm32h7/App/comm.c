#include <comm.h>
#include <fw_update.h>
#include <ina228_driver.h>
#include <safety.h>
#include <string.h>
#include "main.h"

#include "ota_proto.h"
#include "psu_cfg.h"
#include "psu_monitor.h"
#include "psu_trip.h"

extern UART_HandleTypeDef huart1;   /* RS422 #2, DE = PC6, RE = PC9    */
extern UART_HandleTypeDef huart5;   /* RS422 #1, DE = PB2, RE = PB10   */
extern UART_HandleTypeDef huart4;   /* FT232,  CTS# = PA2, RTS# = PA3  */

/* ==========================================================================
 * BUFFER
 * ========================================================================== */

/* Cung section voi buffer cua driver: ngoai DTCM, can 32 de MPU phu duoc. */
#if defined(__GNUC__)
  #define COMM_DMA_ATTR  __attribute__((section(INA228_DMA_SECTION), aligned(32)))
#elif defined(__ICCARM__)
  #define COMM_DMA_ATTR  _Pragma("location=\".dma_buffer\"")
#else
  #define COMM_DMA_ATTR
#endif

/* RX: DMA vong, luy thua cua 2, du chua > 2 khung lon nhat (915 B) */
#define COMM_RX_BUF_SZ      2048u
#define COMM_RX_IDLE_MS     200u        /* im lang chung nay giua khung -> bo khung do */
#define COMM_TX_BUF_SZ      ((PSU_LEN_MAX + 31u) & ~31u)

static COMM_DMA_ATTR uint8_t s_rxbuf[COMM_PORT_COUNT][COMM_RX_BUF_SZ];
static COMM_DMA_ATTR uint8_t s_txbuf[COMM_PORT_COUNT][COMM_TX_BUF_SZ];

static uint8_t   s_rxfrm[COMM_PORT_COUNT][PSU_LEN_MAX];
static psu_rx_t  s_rx[COMM_PORT_COUNT];
static uint16_t  s_tail[COMM_PORT_COUNT];
static uint32_t  s_rx_byte_ms[COMM_PORT_COUNT];

static volatile uint8_t s_dcm_fault;
static uint32_t s_last_rx_ms;

static struct {
    uint32_t rx_frames, rx_badcrc, rx_badaddr, rx_overrun, rx_uart_err;
    uint32_t tx_sent, tx_skipped, tx_error, tx_stuck;
} s_stats;

typedef struct {
    UART_HandleTypeDef *huart;
    GPIO_TypeDef       *de_port;
    uint16_t            de_pin;
    bool                use_de;
    bool                check_rts;
    volatile bool       busy;
    uint32_t            tx_start_ms;
    uint32_t            tx_timeout_ms;
} comm_port_t;

static comm_port_t s_port[COMM_PORT_COUNT] = {
    [COMM_PORT_RS422_1] = { &huart5, RS422_DE1_GPIO_Port, RS422_DE1_Pin, true,  false, false, 0, 0 },
    [COMM_PORT_RS422_2] = { &huart1, RS422_DE2_GPIO_Port, RS422_DE2_Pin, true,  false, false, 0, 0 },
    [COMM_PORT_FT232  ] = { &huart4, NULL, 0, false, true, false, 0, 0 },
};

/* ACK tre (OTA: xoa xong / loi xoa): thu lai moi vong cho den khi gui duoc */
static struct {
    bool     pending;
    uint8_t  port;
    uint16_t cmd;
    uint8_t  status;
    uint32_t info;
} s_late;
static uint8_t s_erase_port;

static uint32_t be32(const uint8_t *b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

static uint16_t be16(const uint8_t *b)
{
    return (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
}

/* ==========================================================================
 * DOC CHAN LOI FT TU DCM
 * Binh thuong pull-up = 1, co loi = 0 -> dao lai de bit 1 nghia la CO LOI.
 * ========================================================================== */
void dcm_poll(void)                      /* goi moi 10 ms */
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
 * PHAT (DMA). Moi cong co buffer phat rieng; cong ban -> bo qua (PC hoi lai).
 * ========================================================================== */
static void port_release(comm_port_t *p)
{
    if (p->use_de) {
        HAL_GPIO_WritePin(p->de_port, p->de_pin, GPIO_PIN_RESET);
    }
    p->busy = false;
}

/* Tra ve buffer phat cua cong, hoac NULL neu cong dang ban / FT232 chua san sang (RTS#) */
static uint8_t *tx_acquire(uint8_t port)
{
    comm_port_t *p = &s_port[port];

    if (p->busy) { s_stats.tx_skipped++; return NULL; }
    if (p->check_rts && HAL_GPIO_ReadPin(FT232_RTS_GPIO_Port, FT232_RTS_Pin) == GPIO_PIN_SET) {
        s_stats.tx_skipped++;
        return NULL;
    }
    return s_txbuf[port];
}

static bool tx_start(uint8_t port, uint16_t len)
{
    comm_port_t *p = &s_port[port];

    if (p->use_de) {
        HAL_GPIO_WritePin(p->de_port, p->de_pin, GPIO_PIN_SET);
        for (volatile int i = 0; i < 200; i++) { __NOP(); }
    }

    p->busy = true;
    p->tx_start_ms = HAL_GetTick();
    /* thoi gian truyen ly thuyet (10 bit/byte) + 30 ms du phong */
    p->tx_timeout_ms = ((uint32_t)len * 10u * 1000u) / COMM_BAUD + 30u;

    if (HAL_UART_Transmit_DMA(p->huart, s_txbuf[port], len) != HAL_OK) {
        port_release(p);
        s_stats.tx_error++;
        return false;
    }
    s_stats.tx_sent++;
    return true;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < COMM_PORT_COUNT; i++) {
        if (s_port[i].huart == huart) {
            port_release(&s_port[i]);
            return;
        }
    }
}

void Comm_TxWatchdog(uint32_t now_ms)
{
    for (uint8_t i = 0; i < COMM_PORT_COUNT; i++) {
        comm_port_t *p = &s_port[i];

        if (!p->busy) continue;
        if ((uint32_t)(now_ms - p->tx_start_ms) < p->tx_timeout_ms) continue;

        s_stats.tx_stuck++;
        (void)HAL_UART_AbortTransmit_IT(p->huart);
        p->huart->ErrorCode = HAL_UART_ERROR_NONE;
        port_release(p);
    }
}

/* ==========================================================================
 * TRA LOI
 * ========================================================================== */
static bool send_ack(uint8_t port, uint16_t cmd, uint8_t status, uint32_t info)
{
    uint8_t *f = tx_acquire(port);
    if (f == NULL) return false;

    Psu_FrameBegin(f, PSU_LEN_FW_ACK, PSU_CMD_FW_ACK);
    f[OTAP_ACK_OFF_CMD]    = OTAP_ACKCMD(cmd);
    f[OTAP_ACK_OFF_STATUS] = status;
    f[OTAP_ACK_OFF_INFO + 0] = (uint8_t)(info >> 24);
    f[OTAP_ACK_OFF_INFO + 1] = (uint8_t)(info >> 16);
    f[OTAP_ACK_OFF_INFO + 2] = (uint8_t)(info >> 8);
    f[OTAP_ACK_OFF_INFO + 3] = (uint8_t)info;
    f[OTAP_ACK_OFF_SLOT]   = (uint8_t)FwUpdate_RunningSlot();
    f[OTAP_ACK_OFF_STATE]  = 0u;
    Psu_FrameSeal(f, PSU_LEN_FW_ACK);
    return tx_start(port, PSU_LEN_FW_ACK);
}

static bool send_info(uint8_t port)
{
    uint8_t *f = tx_acquire(port);
    if (f == NULL) return false;

    const uint32_t ver = FwUpdate_RunningVersion();
    Psu_FrameBegin(f, PSU_LEN_FW_ACK, PSU_CMD_FW_ACK);
    f[OTAP_ACK_OFF_CMD]    = OTAP_ACKCMD(PSU_CMD_FW_INFO);
    f[OTAP_ACK_OFF_STATUS] = OTAP_ST_OK;
    f[OTAP_ACK_OFF_INFO + 0] = (uint8_t)(ver >> 24);
    f[OTAP_ACK_OFF_INFO + 1] = (uint8_t)(ver >> 16);
    f[OTAP_ACK_OFF_INFO + 2] = (uint8_t)(ver >> 8);
    f[OTAP_ACK_OFF_INFO + 3] = (uint8_t)ver;
    f[OTAP_ACK_OFF_SLOT]   = (uint8_t)FwUpdate_RunningSlot();
    f[OTAP_ACK_OFF_STATE]  = FwUpdate_RunningState();
    Psu_FrameSeal(f, PSU_LEN_FW_ACK);
    return tx_start(port, PSU_LEN_FW_ACK);
}

static bool send_monitor(uint8_t port)
{
    uint8_t *f = tx_acquire(port);
    if (f == NULL) return false;

    const uint32_t now = HAL_GetTick();
    psu_mon_t m;
    memset(&m, 0, sizeof(m));

    for (uint8_t i = 0; i < INA228_CH_COUNT; i++) {
        const ina228_dev_t *d = ina228_get_dev(i);
        if (d == NULL) continue;
        m.cur24[i]  = d->raw.current_reg24;
        m.vbus24[i] = d->raw.vbus_reg24;
        if (ina228_is_fresh(d, now)) m.fresh_mask |= (uint8_t)(1u << i);
    }
    m.safe_state  = (uint8_t)Safety_State();
    m.trip_count  = Safety_TripCount();
    m.pwr_on      = Safety_PowerIsOn();
    m.slot        = FwUpdate_RunningSlot();
    m.version     = FwUpdate_RunningVersion();
    m.trip        = PsuTrip_Bytes();

    PsuMon_Build(f, &m);
    return tx_start(port, PSU_LEN_MONITOR);
}

static bool send_cfg_reply(uint8_t port)
{
    uint8_t *f = tx_acquire(port);
    if (f == NULL) return false;

    Psu_FrameBegin(f, PSU_LEN_CFG_REPLY, PSU_CMD_CFG_REPLY);      /* Mask (byte 5..6) khong dung = 0 */
    memcpy(&f[7], PsuCfg_Blob(), PSU_CFG_LEN);
    Psu_FrameSeal(f, PSU_LEN_CFG_REPLY);
    return tx_start(port, PSU_LEN_CFG_REPLY);
}

/* ==========================================================================
 * THUC THI LENH (khung da dung header/tail/CRC va dia chi)
 * ========================================================================== */
static void rx_exec(uint8_t p, const uint8_t *f)
{
    uint32_t info = 0u;
    uint8_t  st;

    switch (Psu_Cmd(f)) {

    case PSU_CMD_CONTROL:           /* khong ACK */
        Safety_Control(f[5], f[6] == 0x01u);
        break;

    case PSU_CMD_MON_REQ:
        (void)send_monitor(p);
        break;

    case PSU_CMD_CFG_QUERY:
        (void)send_cfg_reply(p);
        break;

    case PSU_CMD_CFG_WRITE:         /* khong ACK; PC doc lai de kiem tra */
        /* Doi thanh ghi INA/nguong khi nguon dang bat se doi thang do va ngat bao ve -> chi khi SAFE_OFF.
         * Mask (byte 5..6) bo qua: luon ghi de toan bo. */
        if (Safety_State() == SAFE_OFF && !FwUpdate_IsBusy()) {
            (void)PsuCfg_Store(&f[7]);
        }
        break;

    case PSU_CMD_FW_INFO:
        (void)send_info(p);
        break;

    case PSU_CMD_FW_ERASE:
        st = FwUpdate_Erase(&info);
        if (st != OTAP_ST_OK) {
            (void)send_ack(p, PSU_CMD_FW_ERASE, st, info);
        } else {
            s_erase_port = p;       /* ACK gui sau khi xoa xong (Comm_FwPoll) */
        }
        break;

    case PSU_CMD_FW_BEGIN:
        st = FwUpdate_Begin(be32(&f[OTAP_BEGIN_OFF_SIZE]), be32(&f[OTAP_BEGIN_OFF_CRC32]),
                            be32(&f[OTAP_BEGIN_OFF_VERSION]), &info);
        (void)send_ack(p, PSU_CMD_FW_BEGIN, st, info);
        break;

    case PSU_CMD_FW_DATA:
        st = FwUpdate_Data(be16(&f[OTAP_DATA_OFF_SEQ]), be16(&f[OTAP_DATA_OFF_LEN]),
                           &f[OTAP_DATA_OFF_PAYLOAD], &info);
        (void)send_ack(p, PSU_CMD_FW_DATA, st, info);
        break;

    case PSU_CMD_FW_END:
        st = FwUpdate_End(&info);
        (void)send_ack(p, PSU_CMD_FW_END, st, info);
        break;

    case PSU_CMD_FW_COMMIT:
        if (FwUpdate_CanCommit()) {
            (void)send_ack(p, PSU_CMD_FW_COMMIT, OTAP_ST_OK, 0u);
            FwUpdate_Commit();
        } else {
            (void)send_ack(p, PSU_CMD_FW_COMMIT, OTAP_ST_NO_COMMIT, 0u);
        }
        break;

    case PSU_CMD_FW_ROLLBACK:
        st = FwUpdate_RequestRollBack();
        (void)send_ack(p, PSU_CMD_FW_ROLLBACK, st, 0u);       /* OK -> reset ~50 ms sau */
        break;

    default:
        break;
    }
}

static void on_frame(uint8_t p, const uint8_t *f, uint16_t n)
{
    if (!Psu_FrameValid(f, n))      { s_stats.rx_badcrc++;  return; }
    if (Psu_Addr(f) != PSU_ADDR)    { s_stats.rx_badaddr++; return; }

    s_stats.rx_frames++;
    s_last_rx_ms = HAL_GetTick();
    rx_exec(p, f);
}

/* ==========================================================================
 * NHAN (DMA vong)
 * ========================================================================== */
static void rx_rearm(uint8_t i)
{
    comm_port_t *p = &s_port[i];
    s_tail[i] = 0u;
    Psu_RxReset(&s_rx[i]);
    __HAL_UART_CLEAR_OREFLAG(p->huart);
    if (HAL_UART_Receive_DMA(p->huart, s_rxbuf[i], COMM_RX_BUF_SZ) != HAL_OK)
        s_stats.rx_uart_err++;
}

/* HAL goi tu ngat UART hoac tu ngat DMA khi co loi. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < COMM_PORT_COUNT; i++) {
        if (s_port[i].huart != huart) continue;
        uint32_t err = huart->ErrorCode;

        if (err & (HAL_UART_ERROR_ORE | HAL_UART_ERROR_FE |
                   HAL_UART_ERROR_NE  | HAL_UART_ERROR_PE)) {
            s_stats.rx_uart_err++;
        }
        if (err & HAL_UART_ERROR_DMA) {
            /* Chi abort TX neu dung la TX dang chay */
            if (s_port[i].busy) {
                s_stats.tx_error++;
                (void)HAL_UART_AbortTransmit_IT(huart);
                port_release(&s_port[i]);
            }
        }
        huart->ErrorCode = HAL_UART_ERROR_NONE;

        /* Neu HAL da ha RX xuong -> dung len lai */
        if (huart->RxState != HAL_UART_STATE_BUSY_RX) rx_rearm(i);
        return;
    }
}

/* Vi tri DMA dang ghi toi. NDTR dem NGUOC ve 0. */
static inline uint16_t rx_head(uint8_t p)
{
    if (s_port[p].huart->hdmarx == NULL) return 0;
    return (uint16_t)(COMM_RX_BUF_SZ - __HAL_DMA_GET_COUNTER(s_port[p].huart->hdmarx));
}

void Comm_RxPoll(void)
{
    const uint32_t now = HAL_GetTick();

    for (uint8_t p = 0; p < COMM_PORT_COUNT; p++) {
        const uint16_t head = rx_head(p);
        uint16_t tail = s_tail[p];

        const uint16_t avail = (uint16_t)((head - tail) & (COMM_RX_BUF_SZ - 1u));
        if (avail > (COMM_RX_BUF_SZ - PSU_LEN_MAX)) s_stats.rx_overrun++;

        if (tail == head) {
            /* im lang giua khung: bo khung do de khung ke tiep tach duoc */
            if (s_rx[p].idx != 0u && (uint32_t)(now - s_rx_byte_ms[p]) >= COMM_RX_IDLE_MS) {
                Psu_RxReset(&s_rx[p]);
            }
            continue;
        }

        s_rx_byte_ms[p] = now;
        while (tail != head) {
            const uint16_t n = Psu_RxFeed(&s_rx[p], s_rxbuf[p][tail]);
            tail = (uint16_t)((tail + 1u) & (COMM_RX_BUF_SZ - 1u));
            if (n != 0u) on_frame(p, s_rx[p].buf, n);
        }
        s_tail[p] = tail;
    }
}

/* ==========================================================================
 * API
 * ========================================================================== */
void Comm_Init(void)
{
    memset(&s_stats, 0, sizeof(s_stats));
    memset(&s_late, 0, sizeof(s_late));

    /* RE cua LTM2881 tich cuc muc thap. RS422 la full-duplex nen bo thu
     * khong bao gio can tat -> giu bat vinh vien. */
    HAL_GPIO_WritePin(RS422_RE2_GPIO_Port, RS422_RE2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RS422_RE1_GPIO_Port, RS422_RE1_Pin, GPIO_PIN_RESET);

    /* DE tat luc nghi */
    HAL_GPIO_WritePin(RS422_DE2_GPIO_Port, RS422_DE2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RS422_DE1_GPIO_Port, RS422_DE1_Pin, GPIO_PIN_RESET);

    /* PA2 = CTS# gui toi FT232, giu LOW = STM luon san sang nhan */
    HAL_GPIO_WritePin(FT232_CTS_GPIO_Port, FT232_CTS_Pin, GPIO_PIN_RESET);

    for (uint8_t i = 0; i < COMM_PORT_COUNT; i++) {
        Psu_RxInit(&s_rx[i], s_rxfrm[i], PSU_LEN_MAX);
        s_tail[i] = 0u;
        memset(s_rxbuf[i], 0, COMM_RX_BUF_SZ);

        /* Circular: HAL tu dat CIRC trong hdma, khong bao gio phai restart */
        if (HAL_UART_Receive_DMA(s_port[i].huart, s_rxbuf[i], COMM_RX_BUF_SZ) != HAL_OK) {
            /* Mat mot cong RX khong phai loi chi mang: cong con lai la du phong. */
            s_stats.rx_uart_err++;
        }
    }
}

uint8_t Comm_DcmFault(void) { return s_dcm_fault; }

bool Comm_PcLinkAlive(uint32_t now_ms)
{
    if (s_last_rx_ms == 0u) return false;      /* chua tung nhan lenh nao */
    return ((uint32_t)(now_ms - s_last_rx_ms) < 3000u);
}

/* ==========================================================================
 * OTA - ACK TRE (xoa xong / loi xoa)
 * ========================================================================== */
void Comm_FwPoll(void)
{
    if (!s_late.pending) {
        uint32_t err = 0u;
        if (FwUpdate_TakeEraseDone()) {
            s_late.pending = true; s_late.port = s_erase_port; s_late.cmd = PSU_CMD_FW_ERASE;
            s_late.status = OTAP_ST_OK; s_late.info = 1u;
        } else if (FwUpdate_TakeEraseError(&err)) {
            s_late.pending = true; s_late.port = s_erase_port; s_late.cmd = PSU_CMD_FW_ERASE;
            s_late.status = OTAP_ST_ERASE_FAIL; s_late.info = err;
        }
    }

    if (s_late.pending && send_ack(s_late.port, s_late.cmd, s_late.status, s_late.info)) {
        s_late.pending = false;
    }
}
