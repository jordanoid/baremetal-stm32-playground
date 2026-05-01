#include "stm32f4xx.h"
#include "uart_dma.h"
#include "timer.h"

#define STR_BUFFER_SIZE 2048

int main(void) {
    timer2_led_init();
    uart_dma_system_init();
    
    uart_send_string("Hello from STM32\r\n");
    uart_send_string("This is jordanoid's baremetal sandbox\r\n");

    // Local variables to store the incoming message
    char str_buffer[STR_BUFFER_SIZE];
    uint16_t str_idx = 0;

    while(1) {
        int16_t incoming_byte = uart_read_byte();

        if (incoming_byte != -1) {
            
            str_buffer[str_idx] = (char)incoming_byte;
            str_idx++;

            if (str_idx >= STR_BUFFER_SIZE - 1) str_idx = 0; 

            if (incoming_byte == '\n' || incoming_byte == '\r') {
                
                str_buffer[str_idx] = '\0'; 

                uart_send_string("Echo: ");
                uart_send_string(str_buffer);
                uart_send_string("\r\n");

                // Reset the index to catch the next command
                str_idx = 0; 
            }
        } 
        else {
            // The CPU will wake up instantly when the IDLE line or HT/TC interrupt fires
            __WFI(); 
        }
    }
}