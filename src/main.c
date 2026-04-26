#include "stm32f4xx.h"

#define BUF_SIZE 64
#define TX_RING_SIZE 256

uint16_t prescaler = 16000;
uint16_t auto_reload = 500;
uint8_t rx_buffer[BUF_SIZE];
uint8_t tx_ring[TX_RING_SIZE];

volatile uint16_t tx_head = 0;
volatile uint16_t tx_tail = 0;
volatile uint16_t old_pos = 0;

void sys_init(void) {
    // Enable Clock for Timer 2 (Note: TIM2 is on APB1 bus)
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    
    // Enable GPIO C for internal LED and GPIO A for USART1
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIOAEN;

    // Enable USART1 Clock (Note: USART1 is on APB2 bus)
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    GPIOC->MODER &= ~(3U << (13 * 2)); //Clear mode for PC13
    GPIOC->MODER |= (1U << (13 * 2));  //Set GPIO Mode for PC13 (LED) to Output (Alternative: use |= GPIO_MODER_MODE13_0)

    GPIOA->MODER &= ~(3U << (9 * 2));  // Clear mode for PA9 (Alternative: use &= ~(GPIO_MODER_MODE9))
    GPIOA->MODER &= ~(3U << (10 * 2)); // Clear mode for PA10
    GPIOA->MODER |= (2U << (9 * 2));   // Set mode for PA9 to Alternate Function (Alternative: use |= GPIO_MODER_MODE9_1)
    GPIOA->MODER |= (2U << (10 * 2));  // Set mode for PA10 to Alternate Function

    // AFR[1] is for pins 8-15 
    GPIOA->AFR[1] |= (7U << ((9 - 8) * 4));  // Set AF7 (USART1) for PA9 (Alternative: use |= (7U << GPIO_AFRH_AFSEL9))
    GPIOA->AFR[1] |= (7U << ((10 - 8) * 4)); // Set AF7 (USART1) for PA10

    // Set the Prescaler
    // Clock is 16MHz, we want 1kHz timer frequency
    // 16,000,000 / 16,000 = 1,000
    TIM2->PSC = prescaler - 1; 
    TIM2->ARR = auto_reload - 1; // ARR is zero-based, so we subtract 1

    // Enable the Interrupt in the Timer
    TIM2->DIER |= TIM_DIER_UIE;
    
    USART1->BRR = 0x008B; // Assuming APB2 clock is 16MHz, set baud rate to 115200

    USART1->CR1 &= ~USART_CR1_OVER8; // Oversampling by 16
    USART1->CR1 &= ~USART_CR1_M; // 8 data bits
    USART1->CR1 &= ~USART_CR1_PCE; // Parity none
    // USART1->CR1 |= (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE); // Enable USART, Transmitter and Receiver, and RXNE Interrupt
    USART1->CR1 |= (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE); // Enable USART, Transmitter and Receiver (without RXNE, DMA is used)

    NVIC_SetPriority(TIM2_IRQn, 0); // Set a priority for the timer interrupt
    NVIC_SetPriority(USART1_IRQn, 1); // Set a lower priority for USART1 interrupt

    // Enable the Interrupt in the ARM NVIC
    NVIC_EnableIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(USART1_IRQn); // Also enable USART1 interrupt

    // Reset counter and Start the timer
    TIM2->CNT = 0;
    TIM2->CR1 |= TIM_CR1_CEN;
}

void dma2_init(void){
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN; // Enable clock for DMA2

    USART1->CR3 |= USART_CR3_DMAT | USART_CR3_DMAR; // Enable DMA for USART1 transmit and receive

    // DMA2 Stream 7 Channel 4 -> USART1_TX
    DMA2_Stream7->PAR = (uint32_t)&USART1->DR; // Peripheral address (USART1 data register)
    DMA2_Stream7->M0AR = (uint32_t)tx_ring; // Memory address (transmit buffer)
    DMA2_Stream7->CR = (4U << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_TCIE; // Channel 4

    // DMA2 Stream 2 Channel 4 -> USART1_RX
    DMA2_Stream2->PAR = (uint32_t)&USART1->DR; // Peripheral address (USART1 data register)
    DMA2_Stream2->M0AR = (uint32_t)rx_buffer; // Memory address (receive buffer)
    DMA2_Stream2->NDTR = BUF_SIZE; // Buffer size
    DMA2_Stream2->CR = (4U << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC | DMA_SxCR_CIRC | DMA_SxCR_HTIE | DMA_SxCR_TCIE;// Channel 4

    DMA2_Stream2->CR |= DMA_SxCR_EN; // Enable DMA for USART1 receive

    NVIC_SetPriority(DMA2_Stream7_IRQn, 2); // Lower priority than RX
    NVIC_SetPriority(DMA2_Stream2_IRQn, 1);
    NVIC_EnableIRQ(DMA2_Stream7_IRQn);
    NVIC_EnableIRQ(DMA2_Stream2_IRQn);
}

void dma2_transmit(uint32_t mem_addr, uint16_t size){
    DMA2_Stream7->CR &= ~DMA_SxCR_EN; // Disable DMA stream before configuring
    while(DMA2_Stream7->CR & DMA_SxCR_EN); // Wait until the stream is fully disabled

    // Clear flags for Stream 7 in High Interrupt Register
    DMA2->HIFCR = (0x3DU << 22);

    DMA2_Stream7->M0AR = mem_addr;
    DMA2_Stream7->NDTR = size; // Set the number of data items to transmit
    DMA2_Stream7->CR |= DMA_SxCR_EN; // Enable DMA for USART1 transmit
}

void flush_tx_ring(void) {
    // Is the TX DMA currently busy sending something? If yes, back off and wait.
    if (DMA2_Stream7->CR & DMA_SxCR_EN) return;

    // Is the Ring Buffer empty? If yes, nothing to do.
    if (tx_head == tx_tail) return;

    uint16_t send_len = 0;

    // Calculate contiguous length for the DMA
    if (tx_head > tx_tail) {
        // Linear data: Head is ahead of Tail
        send_len = tx_head - tx_tail;
    } else {
        // Wrapped data: Head wrapped around behind Tail.
        // Send only up to the physical end of the array.
        send_len = TX_RING_SIZE - tx_tail;
    }

    // Fire the DMA using the exact address of the Tail
    uint32_t start_address = (uint32_t)&tx_ring[tx_tail];
    dma2_transmit(start_address, send_len);

    // Advance the Tail by the amount we just sent
    tx_tail = (tx_tail + send_len) % TX_RING_SIZE;
}

void process_rx_data(void) {
    // Where is the DMA right now?
    uint16_t curr_pos = BUF_SIZE - DMA2_Stream2->NDTR;
    
    // If no new data, exit
    if (curr_pos == old_pos) return; 

    // Calculate length (handling circular wrap-around)
    uint16_t rx_len = 0;
    if (curr_pos > old_pos) {
        rx_len = curr_pos - old_pos;
    } else {
        rx_len = (BUF_SIZE - old_pos) + curr_pos; 
    }

    // Copy the new chunk to tx_buffer
    for (uint16_t i = 0; i < rx_len; i++) {
        uint16_t index = (old_pos + i) % BUF_SIZE;

        // Calculate where the head wants to go next
        uint16_t next_head = (tx_head + 1) % TX_RING_SIZE;

        // If the next step hits the tail, the buffer is full! Stop copying.
        if (next_head == tx_tail) {
            break; // Drop the incoming bytes to protect the existing queue
        }
        
        tx_ring[tx_head] = rx_buffer[index];
        tx_head = next_head; // Advance Head
    }

    // Save the position and Transmit
    old_pos = curr_pos;
}

int main(void) {
    sys_init();
    dma2_init();

    char *message = "Hello from STM32\r\n";

    int idx = 0;
    while(message[idx] != '\0') {
        tx_ring[tx_head] = message[idx];
        tx_head = (tx_head + 1) % TX_RING_SIZE;
        idx++;
    }

    while(1) {
        // Continuously check if there is data to send
        flush_tx_ring();
        __WFI(); // Wait For Interrupt to save power while idle
    }
}

void TIM2_IRQHandler(void) {
    // Double-check that the UIF flag is actually the reason we are here
    if (TIM2->SR & TIM_SR_UIF) {
        // Clear the flag immediately
        TIM2->SR &= ~TIM_SR_UIF;

        // Toggle the LED
        GPIOC->ODR ^= (1 << 13);
    }
}

// USART1 Interrupt Handler (if using RXNE interrupt instead of DMA)
// void USART1_IRQHandler(void) {
//     if (USART1->SR & USART_SR_RXNE) {
//         char data = (char)USART1->DR;
        
//         // Simple write back
//         while (!(USART1->SR & USART_SR_TXE));
//         USART1->DR = data;
//     }
// }

// USART1 Interrupt Handler for IDLE Line Detection (using DMA)
void USART1_IRQHandler(void) {
    if (USART1->SR & USART_SR_IDLE) {
        
        // Clear IDLE flag (Read SR, then DR)
        volatile uint32_t tmp = USART1->SR;
        tmp = USART1->DR;
        (void)tmp; 

        // Extract whatever is left seamlessly
        process_rx_data(); 
    }
}

void DMA2_Stream2_IRQHandler(void) {
    // Check Half-Transfer Flag
    if (DMA2->LISR & DMA_LISR_HTIF2) {
        DMA2->LIFCR = DMA_LIFCR_CHTIF2; // Clear flag
        process_rx_data();              // Extract indices 0 to 31
    }
    
    // Check Transfer Complete Flag
    if (DMA2->LISR & DMA_LISR_TCIF2) {
        DMA2->LIFCR = DMA_LIFCR_CTCIF2; // Clear flag
        process_rx_data();              // Extract indices 32 to 63
    }
}

void DMA2_Stream7_IRQHandler(void) {
    if (DMA2->HISR & DMA_HISR_TCIF7) {
        DMA2->HIFCR = DMA_HIFCR_CTCIF7; // Clear flag
    }
}