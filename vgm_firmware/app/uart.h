#pragma once

#include <stdint.h>
#include <stdbool.h>

void uart_protocol_init(void);
void keyboard_uart_send_key(uint8_t modifiers, uint8_t usage);
bool keyboard_uart_is_ready(void);
