#include "rcc.h"

void rcc_system_init(void) {
    // Enable HSE
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)); // Wait for HSE to be ready

    // Enable power controller clock
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS; // Set Voltage Scaling to Scale 1 mode

    // Configure flash latency (3WS for 96MHz) and enable prefetch, instruction cache, and data cache
    FLASH->ACR = FLASH_ACR_LATENCY_3WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    // Configure Dividers: AHB=1, APB1=2, APB2=1
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2); // Clear prescaler bits
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;   // AHB Prescaler = 1 96 MHz HCLK)
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;  // APB1 Prescaler = 2 (48 MHz PCLK1)
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;  // APB2 Prescaler = 1 (96 MHz PCLK2)

    // Set HSE as Source
    RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLSRC | RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLP | RCC_PLLCFGR_PLLQ); // Clear PLL Source, M, N, P, Q bits
    RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSE; // HSE as PLL source
    RCC->PLLCFGR |= (25U << RCC_PLLCFGR_PLLM_Pos);   // PLLM = 25
    RCC->PLLCFGR |= (192U << RCC_PLLCFGR_PLLN_Pos); // PLLN = 192
    RCC->PLLCFGR |= (0U << RCC_PLLCFGR_PLLP_Pos);   // PLLP = 2 (00b)
    RCC->PLLCFGR |= (4U << RCC_PLLCFGR_PLLQ_Pos);   // PLLQ = 4

    // Enable PLL
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)); // Wait for PLL to be ready

    RCC->CFGR &= ~RCC_CFGR_SW; // Clear SW bits
    RCC->CFGR |= RCC_CFGR_SW_PLL; // Select PLL as system clock
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL); // Wait for PLL to be used as system clock  
}