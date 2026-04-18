#include "stm32f4xx.h"

uint16_t prescaler = 16000;
uint16_t auto_reload = 250;

void timer2_init_interrupt(void) {
    // 1. Enable Clock for Timer 2 (Note: TIM2 is on APB1 bus)
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    // 2. Set the Prescaler
    // Clock is 16MHz, we want 1kHz timer frequency
    // 16,000,000 / 16,000 = 1,000
    TIM2->PSC = prescaler - 1; 
    TIM2->ARR = auto_reload - 1; // ARR is zero-based, so we subtract 1
    
    // 3. Enable the Interrupt in the Timer
    TIM2->DIER |= TIM_DIER_UIE; 

    // 4. Enable the Interrupt in the ARM NVIC
    NVIC_EnableIRQ(TIM2_IRQn);    

    // 5. Reset counter and Start the timer
    TIM2->CNT = 0;
    TIM2->CR1 |= TIM_CR1_CEN;
}

int main(void) {
    // Enable GPIOC clock and set PC13 as output (from Phase 1)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER &= ~(3U << (13 * 2));
    GPIOC->MODER |= (1U << (13 * 2));

    timer2_init_interrupt();

    while(1) {
        
    }
}

void TIM2_IRQHandler(void) {
    // Double-check that the UIF flag is actually the reason we are here
    if (TIM2->SR & TIM_SR_UIF) {
        // IMPORTANT: Clear the flag immediately
        TIM2->SR &= ~TIM_SR_UIF;

        // Toggle the LED
        GPIOC->ODR ^= (1 << 13);
    }
}



