#include <pico/stdlib.h>
#include <hardware/clocks.h>
#include <hardware/vreg.h>
#include <FreeRTOS.h>
#include <task.h>

#include "uart.h"
#include "usb.h"
#include "led.h"

static uint32_t clock_khz_default(void) {
    return 133000;
}

static void init(void) {
    led_init();

    sleep_ms(10);
    vreg_set_voltage(VREG_VOLTAGE_1_10);
    sleep_ms(10);
    set_sys_clock_khz(clock_khz_default(), true);

    uart_protocol_init();
    usb_init();

    led_red(false);
}

int main(void) {
    init();

    vTaskStartScheduler();

    while(1) {
        __wfe();
    }

    __builtin_unreachable();
}
