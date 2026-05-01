#include "uart_dma.h"

#define BUF_SIZE      2048   // Hardware RX DMA buffer
#define TX_RING_SIZE  2048  // Software TX Queue
#define APP_RX_SIZE   2048  // Software RX Queue

static uint8_t rx_buffer[BUF_SIZE];
static uint8_t tx_ring[TX_RING_SIZE];
static uint8_t app_rx_buffer[APP_RX_SIZE];

static volatile uint16_t tx_head = 0;
static volatile uint16_t tx_tail = 0;

static volatile uint16_t app_rx_head = 0;
static volatile uint16_t app_rx_tail = 0;

static volatile uint16_t old_pos = 0;

static void start_tx_dma_chunk(void);
static void process_rx_data(void);


void uart_dma_system_init(void) {
    // Enable clocks for GPIOA, DMA2, and USART1
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2EN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    // Configure PA9 (TX) and PA10 (RX) for USART1
    GPIOA->MODER &= ~((3U << (9 * 2)) | (3U << (10 * 2)));
    GPIOA->MODER |= (2U << (9 * 2)) | (2U << (10 * 2));
    GPIOA->AFR[1] |= (7U << ((9 - 8) * 4)) | (7U << ((10 - 8) * 4));

    // Configure USART1: 115200 baud, 8N1, Enable USART, TX, RX, and IDLE interrupt
    USART1->BRR = 0x008B; 
    USART1->CR1 &= ~(USART_CR1_OVER8 | USART_CR1_M | USART_CR1_PCE);
    USART1->CR1 |= (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE);
    USART1->CR3 |= USART_CR3_DMAT | USART_CR3_DMAR;

    // Configure DMA for USART1 TX
    DMA2_Stream7->PAR = (uint32_t)&USART1->DR;
    DMA2_Stream7->M0AR = (uint32_t)tx_ring; 
    DMA2_Stream7->CR = (4U << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_TCIE;

    // Configure DMA for USART1 RX
    DMA2_Stream2->PAR = (uint32_t)&USART1->DR;
    DMA2_Stream2->M0AR = (uint32_t)rx_buffer;
    DMA2_Stream2->NDTR = BUF_SIZE;
    DMA2_Stream2->CR = (4U << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC | DMA_SxCR_CIRC | DMA_SxCR_HTIE | DMA_SxCR_TCIE;
    DMA2_Stream2->CR |= DMA_SxCR_EN;

    NVIC_SetPriority(USART1_IRQn, 1);
    NVIC_SetPriority(DMA2_Stream2_IRQn, 1); 
    NVIC_SetPriority(DMA2_Stream7_IRQn, 2); // Lower priority than RX
    
    NVIC_EnableIRQ(USART1_IRQn);
    NVIC_EnableIRQ(DMA2_Stream2_IRQn);
    NVIC_EnableIRQ(DMA2_Stream7_IRQn);
}


void uart_send_string(const char *str) {
    int idx = 0;
    
    __disable_irq(); 
    
    while(str[idx] != '\0') {
        tx_ring[tx_head] = str[idx];
        tx_head = (tx_head + 1) % TX_RING_SIZE;
        idx++;
    }
    
    if (!(DMA2_Stream7->CR & DMA_SxCR_EN)) {
        start_tx_dma_chunk(); 
    }
    
    __enable_irq(); 
}

int16_t uart_read_byte(void) {
    if (app_rx_head == app_rx_tail) {
        return -1; 
    }

    __disable_irq();

    uint8_t data = app_rx_buffer[app_rx_tail];
    app_rx_tail = (app_rx_tail + 1) % APP_RX_SIZE;

    __enable_irq();

    return data;
}

static void start_tx_dma_chunk(void) {
    if (tx_head == tx_tail) return; 

    uint16_t send_len = 0;
    
    if (tx_head > tx_tail) {
        send_len = tx_head - tx_tail;
    } else {
        send_len = TX_RING_SIZE - tx_tail; 
    }

    uint32_t start_address = (uint32_t)&tx_ring[tx_tail];

    DMA2_Stream7->CR &= ~DMA_SxCR_EN;
    while(DMA2_Stream7->CR & DMA_SxCR_EN); 
    DMA2->HIFCR = (0x3DU << 22);           
    
    DMA2_Stream7->M0AR = start_address;
    DMA2_Stream7->NDTR = send_len;
    DMA2_Stream7->CR |= DMA_SxCR_EN;       

    tx_tail = (tx_tail + send_len) % TX_RING_SIZE;
}

static void process_rx_data(void) {
    uint16_t curr_pos = BUF_SIZE - DMA2_Stream2->NDTR;
    if (curr_pos == old_pos) return;

    uint16_t rx_len = (curr_pos > old_pos) ? (curr_pos - old_pos) : ((BUF_SIZE - old_pos) + curr_pos);

    for (uint16_t i = 0; i < rx_len; i++) {
        uint16_t index = (old_pos + i) % BUF_SIZE;
        uint8_t incoming_byte = rx_buffer[index];
        uint16_t next_app_head = (app_rx_head + 1) % APP_RX_SIZE;
        
        if (next_app_head != app_rx_tail) { 
            app_rx_buffer[app_rx_head] = incoming_byte;
            app_rx_head = next_app_head; 
        }
    }
    
    old_pos = curr_pos;
}

void USART1_IRQHandler(void) {
    if (USART1->SR & USART_SR_IDLE) {
        volatile uint32_t tmp = USART1->SR;
        tmp = USART1->DR;
        (void)tmp;
        process_rx_data();
    }
}

void DMA2_Stream2_IRQHandler(void) {
    if (DMA2->LISR & DMA_LISR_HTIF2) {
        DMA2->LIFCR = DMA_LIFCR_CHTIF2;
        process_rx_data();
    }
    if (DMA2->LISR & DMA_LISR_TCIF2) {
        DMA2->LIFCR = DMA_LIFCR_CTCIF2;
        process_rx_data();
    }
}

void DMA2_Stream7_IRQHandler(void) {
    if (DMA2->HISR & DMA_HISR_TCIF7) {
        DMA2->HIFCR = DMA_HIFCR_CTCIF7; 

        if (tx_head != tx_tail) {
            start_tx_dma_chunk(); 
        }
    }
}