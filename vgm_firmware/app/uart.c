#include "uart.h"
#include "keyboard_protocol.h"

#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/uart.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#define UART_ID            uart0
#define UART_IRQ           UART0_IRQ
#define UART_TX_PIN        0
#define UART_RX_PIN        1
#define UART_BAUD_RATE     460800UL

static volatile bool handshake_done = false;
static SemaphoreHandle_t tx_mutex = NULL;

static void uart_init_pins(void) {
    uart_init(UART_ID, UART_BAUD_RATE);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_fifo_enabled(UART_ID, true);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    gpio_set_pulls(UART_RX_PIN, true, false);
    gpio_set_pulls(UART_TX_PIN, true, false);
}

static bool magic_match_step(
    uint8_t ch,
    const uint8_t magic[KBW_MAGIC_LEN],
    size_t* index) {

    if(ch == magic[*index]) {
        (*index)++;
        if(*index == KBW_MAGIC_LEN) {
            *index = 0;
            return true;
        }
    } else {
        *index = (ch == magic[0]) ? 1u : 0u;
    }

    return false;
}

static void uart_send_bytes(const uint8_t* data, size_t len) {
    if(tx_mutex) {
        xSemaphoreTake(tx_mutex, portMAX_DELAY);
    }

    uart_write_blocking(UART_ID, data, len);
    uart_tx_wait_blocking(UART_ID);

    if(tx_mutex) {
        xSemaphoreGive(tx_mutex);
    }
}

static bool uart_wait_for_start(uint32_t timeout_ms) {
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    size_t magic_index = 0;

    while(absolute_time_diff_us(get_absolute_time(), deadline) > 0) {
        while(uart_is_readable(UART_ID)) {
            const uint8_t ch = uart_getc(UART_ID);

            if(magic_match_step(ch, kbw_host_magic, &magic_index)) {
                uart_send_bytes(kbw_ack_magic, sizeof(kbw_ack_magic));
                return true;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }

    return false;
}

static void uart_task(void* context) {
    (void)context;

    uart_init_pins();

    while(true) {
        handshake_done = uart_wait_for_start(1000);

        if(handshake_done) {
            /*
             * Stay alive and acknowledge repeated handshakes. Keyboard data
             * only travels VGM -> Flipper, so there is nothing else to parse.
             */
            size_t magic_index = 0;

            while(true) {
                while(uart_is_readable(UART_ID)) {
                    const uint8_t ch = uart_getc(UART_ID);

                    if(magic_match_step(
                           ch,
                           kbw_host_magic,
                           &magic_index)) {
                        uart_send_bytes(
                            kbw_ack_magic,
                            sizeof(kbw_ack_magic));
                    }
                }

                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
    }
}

void uart_protocol_init(void) {
    tx_mutex = xSemaphoreCreateMutex();
    configASSERT(tx_mutex != NULL);

    TaskHandle_t handle = NULL;
    const BaseType_t status = xTaskCreate(
        uart_task,
        "uart_task",
        2048,
        NULL,
        2,
        &handle);

    configASSERT(status == pdPASS);
    (void)handle;
}

void keyboard_uart_send_key(uint8_t modifiers, uint8_t usage) {
    if(!handshake_done) return;

    const uint8_t frame[4] = {
        KBW_FRAME_MAGIC,
        KBW_EVENT_KEY,
        modifiers,
        usage,
    };

    uart_send_bytes(frame, sizeof(frame));
}

bool keyboard_uart_is_ready(void) {
    return handshake_done;
}
