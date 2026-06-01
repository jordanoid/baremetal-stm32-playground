#include "timer.h"

void timer2_led_init(void) {
    uint16_t prescaler = 96000;
    uint16_t auto_reload = 500;

    // Enable Clocks (TIM2, GPIOC)
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;

    // LED Setup (PC13)
    GPIOC->MODER &= ~(3U << (13 * 2)); 
    GPIOC->MODER |= (1U << (13 * 2));  

    // Timer Setup
    TIM2->PSC = prescaler - 1;
    TIM2->ARR = auto_reload - 1;
    TIM2->DIER |= TIM_DIER_UIE;

    // NVIC Setup
    NVIC_SetPriority(TIM2_IRQn, 0);
    NVIC_EnableIRQ(TIM2_IRQn);

    // Start Timer
    TIM2->CNT = 0;
    TIM2->CR1 |= TIM_CR1_CEN;
}

void timer5_adc_init(void) {
    uint16_t prescaler = 96;
    uint16_t auto_reload = 100;

    // Enable Clocks (TIM5)
    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;

    // Timer Setup
    TIM5->PSC = prescaler - 1;
    TIM5->ARR = auto_reload - 1;
    TIM5->DIER |= TIM_DIER_UIE;

    // NVIC Setup
    NVIC_SetPriority(TIM5_IRQn, 0);
    NVIC_EnableIRQ(TIM5_IRQn);

    // Start Timer
    TIM5->CNT = 0;
    TIM5->CR1 |= TIM_CR1_CEN;
}

// Interrupt Handler
void TIM2_IRQHandler(void) {
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR &= ~TIM_SR_UIF;
        GPIOC->ODR ^= (1 << 13); // Toggle LED
    }
}

void TIM5_IRQHandler(void) {
    if (TIM5->SR & TIM_SR_UIF) {
        TIM5->SR &= ~TIM_SR_UIF;
        // Placeholder for ADC read or other periodic tasks
    }
}