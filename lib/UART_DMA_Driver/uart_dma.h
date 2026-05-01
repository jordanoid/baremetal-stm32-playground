#ifndef UART_DMA_H
#define UART_DMA_H

#include "stm32f4xx.h"
#include <stdint.h>

// Public API Functions
void uart_dma_system_init(void);
void uart_send_string(const char *str);
int16_t uart_read_byte(void);

#endif // UART_DMA_H