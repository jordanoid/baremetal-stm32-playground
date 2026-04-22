#include "stm32f4xx.h"

uint16_t prescaler = 16000;
uint16_t auto_reload = 500;

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
    USART1->CR1 |= (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE); // Enable USART, Transmitter and Receiver, and RXNE Interrupt

    NVIC_SetPriority(TIM2_IRQn, 0); // Set a priority for the timer interrupt
    NVIC_SetPriority(USART1_IRQn, 1); // Set a lower priority for USART1 interrupt

    // Enable the Interrupt in the ARM NVIC
    NVIC_EnableIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(USART1_IRQn); // Also enable USART1 interrupt

    // Reset counter and Start the timer
    TIM2->CNT = 0;
    TIM2->CR1 |= TIM_CR1_CEN;
}

int main(void) {
    sys_init();

    while(1) {
        
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

void USART1_IRQHandler(void) {
    if (USART1->SR & USART_SR_RXNE) {
        char data = (char)USART1->DR;
        
        // Simple write back
        while (!(USART1->SR & USART_SR_TXE));
        USART1->DR = data;
    }
}



