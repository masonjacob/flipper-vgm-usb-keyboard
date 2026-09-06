#include "usb.h"
#include "uart.h"

#include <bsp/board_api.h>
#include <tusb.h>

#include <FreeRTOS.h>
#include <task.h>

static hid_keyboard_report_t previous_report = {0};

static bool report_contains_key(hid_keyboard_report_t const* report, uint8_t usage) {
    for(uint8_t i = 0; i < 6; ++i) {
        if(report->keycode[i] == usage) return true;
    }
    return false;
}

static void process_keyboard_report(hid_keyboard_report_t const* report) {
    for(uint8_t i = 0; i < 6; ++i) {
        uint8_t usage = report->keycode[i];
        if(usage == 0) continue;

        /* Only emit transitions; held keys are intentionally repeated by
         * the physical keyboard at the HID report level. We keep this
         * implementation deterministic and let the editor handle repeats
         * by receiving fresh key presses from the keyboard. */
        if(!report_contains_key(&previous_report, usage)) {
            keyboard_uart_send_key(report->modifier, usage);
        }
    }

    previous_report = *report;
}

static void usb_host_task(void* context) {
    (void)context;

    const tusb_rhport_init_t rh_init = {
        .role = TUSB_ROLE_HOST,
        .speed = TUSB_SPEED_AUTO,
    };

    /*
     * The module's RP2040 USB-C port is documented as supporting host mode.
     * `tinyusb_board` supplies the board-level host controller glue.
     */
    board_init();
    if(!tusb_rhport_init(BOARD_TUH_RHPORT, &rh_init)) {
        vTaskDelete(NULL);
        return;
    }
    board_init_after_tusb();

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

    if(tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD) {
        tuh_hid_receive_report(dev_addr, instance);
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    (void)dev_addr;
    (void)instance;
    previous_report = (hid_keyboard_report_t){0};
}

void tuh_hid_report_received_cb(
    uint8_t dev_addr,
    uint8_t instance,
    uint8_t const* report,
    uint16_t len) {
    if(len >= sizeof(hid_keyboard_report_t) &&
       tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD) {
        process_keyboard_report((hid_keyboard_report_t const*)report);
    }

    tuh_hid_receive_report(dev_addr, instance);
}

void usb_init(void) {
    TaskHandle_t handle = NULL;
    BaseType_t status = xTaskCreate(
        usb_host_task,
        "usb_host",
        3072,
        NULL,
        3,
        &handle);
    configASSERT(status == pdPASS);
    (void)handle;
}
