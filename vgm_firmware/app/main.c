#include <pico/stdlib.h>
#include <hardware/clocks.h>
#include <hardware/vreg.h>
#include <FreeRTOS.h>
#include <task.h>

#include "uart.h"
#include "usb.h"

static void init(void) {
    /*
     * Keep the VGM clock configuration conservative. USB host operation uses
     * the RP2040 native USB controller in the bundled TinyUSB port.
     */
    vreg_set_voltage(VREG_VOLTAGE_1_10);
    sleep_ms(10);
    set_sys_clock_khz(133000, true);

    uart_protocol_init();
    usb_init();
}

int main(void) {
    init();

    vTaskStartScheduler();

    for(;;) {
        tight_loop_contents();
    }
}
