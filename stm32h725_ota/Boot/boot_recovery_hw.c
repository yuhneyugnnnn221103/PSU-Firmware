#include "main.h"
#include "boot_recovery.h"
#include "iwdg_hw.h"

/* ==========================================================================
 * UART polling o muc thanh ghi (bootloader khong co HAL UART).
 * Cung pin / baud voi app (xem PSU_stm32h7: main.c, stm32h7xx_hal_msp.c).
 *   port 0: UART5  (RX PB12, TX PB13, AF14)  DE1 = PB2,  RE1 = PB10
 *   port 1: USART1 (TX PB14, RX PB15, AF4)   DE2 = PC6,  RE2 = PC9
 *   port 2: UART4  (TX PA0,  RX PA1,  AF8)   CTS# = PA2 (giu LOW)
 * Kernel clock mac dinh sau reset = PCLK (khong cau hinh RCC them).
 * ========================================================================== */

#define REC_BAUD   115200u

typedef struct {
    USART_TypeDef *u;
    GPIO_TypeDef  *de_port;
    uint16_t       de_pin;
    bool           apb2;     /* USART1 chay tu PCLK2 */
} rec_port_t;

static const rec_port_t s_ports[REC_PORTS] = {
    { UART5,  GPIOB, GPIO_PIN_2, false },
    { USART1, GPIOC, GPIO_PIN_6, true  },
    { UART4,  NULL,  0u,         false },
};

static void gpio_af(GPIO_TypeDef *port, uint32_t pins, uint32_t af)
{
    GPIO_InitTypeDef g = {0};
    g.Pin       = pins;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = af;
    HAL_GPIO_Init(port, &g);
}

static void gpio_out_low(GPIO_TypeDef *port, uint32_t pins)
{
    GPIO_InitTypeDef g = {0};
    HAL_GPIO_WritePin(port, (uint16_t)pins, GPIO_PIN_RESET);
    g.Pin   = pins;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &g);
}

void RecHw_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_UART4_CLK_ENABLE();
    __HAL_RCC_UART5_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    gpio_af(GPIOB, GPIO_PIN_12 | GPIO_PIN_13, GPIO_AF14_UART5);
    gpio_af(GPIOB, GPIO_PIN_14 | GPIO_PIN_15, GPIO_AF4_USART1);
    gpio_af(GPIOA, GPIO_PIN_0  | GPIO_PIN_1,  GPIO_AF8_UART4);

    /* DE tat, RE (tich cuc thap) luon bat, CTS# cua FT232 = LOW */
    gpio_out_low(GPIOB, GPIO_PIN_2 | GPIO_PIN_10);
    gpio_out_low(GPIOC, GPIO_PIN_6 | GPIO_PIN_9);
    gpio_out_low(GPIOA, GPIO_PIN_2);

    const uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    const uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();

    for (uint8_t i = 0; i < REC_PORTS; i++) {
        USART_TypeDef *u = s_ports[i].u;
        const uint32_t clk = s_ports[i].apb2 ? pclk2 : pclk1;

        u->CR1 = 0u;                       /* 8N1, oversampling 16, FIFO tat */
        u->CR2 = 0u;
        u->CR3 = 0u;
        u->PRESC = 0u;
        u->BRR = (clk + REC_BAUD / 2u) / REC_BAUD;
        u->ICR = 0xFFFFFFFFu;
        u->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;
    }
}

bool RecHw_RxByte(uint8_t port, uint8_t *byte)
{
    USART_TypeDef *u = s_ports[port].u;

    if (u->ISR & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE)) {
        u->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
    }
    if (u->ISR & USART_ISR_RXNE_RXFNE) {
        *byte = (uint8_t)u->RDR;
        return true;
    }
    return false;
}

static void tiny_delay(void)
{
    for (volatile int i = 0; i < 200; i++) { __NOP(); }
}

void RecHw_Tx(uint8_t port, const uint8_t *buf, uint16_t len)
{
    const rec_port_t *p = &s_ports[port];

    if (p->de_port != NULL) {
        HAL_GPIO_WritePin(p->de_port, p->de_pin, GPIO_PIN_SET);
        tiny_delay();
    }
    for (uint16_t i = 0; i < len; i++) {
        while ((p->u->ISR & USART_ISR_TXE_TXFNF) == 0u) { }
        p->u->TDR = buf[i];
    }
    while ((p->u->ISR & USART_ISR_TC) == 0u) { }
    if (p->de_port != NULL) {
        tiny_delay();
        HAL_GPIO_WritePin(p->de_port, p->de_pin, GPIO_PIN_RESET);
    }
}

void RecHw_ResetSystem(void)
{
    HAL_Delay(20);        /* TX da xong (TC) - them chut cho host nhan ACK */
    NVIC_SystemReset();
}

/* ==========================================================================
 * VONG CHINH CUU HO
 * ========================================================================== */
#define REC_LED_PERIOD_MS    50u      /* nhay nhanh: bao hieu che do cuu ho */
#define REC_RESYNC_MS        500u     /* im lang chung nay -> bo khung do */

void Boot_RecoveryRun(void)
{
    uint32_t t_led = HAL_GetTick();
    uint32_t t_rx[REC_PORTS];

    RecHw_Init();
    for (uint8_t i = 0; i < REC_PORTS; i++) t_rx[i] = t_led;

    while (1) {
        IWDG_Refresh();
        uint32_t now = HAL_GetTick();

        if ((uint32_t)(now - t_led) >= REC_LED_PERIOD_MS) {
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            t_led = now;
        }

        for (uint8_t p = 0; p < REC_PORTS; p++) {
            uint8_t c;
            bool got = false;

            while (RecHw_RxByte(p, &c)) {
                got = true;
                Boot_RecoveryFeed(p, c);
            }
            if (got) {
                t_rx[p] = HAL_GetTick();
            } else if ((uint32_t)(now - t_rx[p]) >= REC_RESYNC_MS) {
                Boot_RecoveryResync(p);
            }
        }
    }
}
