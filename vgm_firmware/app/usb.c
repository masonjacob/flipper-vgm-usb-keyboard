#include "usb.h"
#include "uart.h"

#include <pico/stdlib.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "bsp/board.h"
#include "tusb.h"

static hid_keyboard_report_t previous_report = {
    .modifier = 0,
    .reserved = 0,
    .keycode = {0},
};

static bool report_contains_key(
    const hid_keyboard_report_t* report,
    uint8_t usage) {

    for(uint8_t i = 0; i < 6; ++i) {
        if(report->keycode[i] == usage) return true;
    }

    return false;
}

static void process_keyboard_report(
    const hid_keyboard_report_t* report) {

    for(uint8_t i = 0; i < 6; ++i) {
        const uint8_t usage = report->keycode[i];

        if(usage == 0) continue;

        if(!report_contains_key(&previous_report, usage)) {
            keyboard_uart_send_key(report->modifier, usage);
        }
    }

    previous_report = *report;
}

static void usb_host_task(void* context) {
    (void)context;

    board_init();

    /*
     * Native RP2040 USB host port. The VGM's USB-C connector is documented
     * as usable as a host by custom firmware.
     */
    if(!tuh_init(BOARD_TUH_RHPORT)) {
        vTaskDelete(NULL);
        return;
    }

    while(true) {
        tuh_task();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void tuh_hid_mount_cb(
    uint8_t dev_addr,
    uint8_t instance,
    uint8_t const* desc_report,
    uint16_t desc_len) {

    (void)desc_report;
    (void)desc_len;

    const uint8_t protocol =
        tuh_hid_interface_protocol(dev_addr, instance);

    if(protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        previous_report = (hid_keyboard_report_t){
            .modifier = 0,
            .reserved = 0,
            .keycode = {0},
        };

        if(!tuh_hid_receive_report(dev_addr, instance)) {
            /* Device mounted but first report request failed. */
        }
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    (void)dev_addr;
    (void)instance;

    previous_report = (hid_keyboard_report_t){
        .modifier = 0,
        .reserved = 0,
        .keycode = {0},
    };
}

void tuh_hid_report_received_cb(
    uint8_t dev_addr,
    uint8_t instance,
    uint8_t const* report,
    uint16_t len) {

    const uint8_t protocol =
        tuh_hid_interface_protocol(dev_addr, instance);

    if(protocol == HID_ITF_PROTOCOL_KEYBOARD &&
       len >= sizeof(hid_keyboard_report_t)) {
        process_keyboard_report(
            (const hid_keyboard_report_t*)report);
    }

    if(!tuh_hid_receive_report(dev_addr, instance)) {
        /* Device disconnected or stopped accepting reports. */
    }
}

void usb_init(void) {
    TaskHandle_t handle = NULL;

    const BaseType_t status = xTaskCreate(
        usb_host_task,
        "usb_host",
        4096,
        NULL,
        3,
        &handle);

    configASSERT(status == pdPASS);
    (void)handle;
}
