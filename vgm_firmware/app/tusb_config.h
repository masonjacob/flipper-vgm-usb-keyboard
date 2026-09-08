#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/* RP2040 native USB controller. */
#define CFG_TUSB_MCU            OPT_MCU_RP2040

#define CFG_TUH_ENABLED        1
#define CFG_TUH_RHPORT         0
#define BOARD_TUH_RHPORT       0
#define CFG_TUH_MAX_SPEED      OPT_MODE_DEFAULT_SPEED

#define CFG_TUH_HUB            0
#define CFG_TUH_CDC            0
#define CFG_TUH_HID            4
#define CFG_TUH_MSC            0
#define CFG_TUH_VENDOR        0

#define CFG_TUH_DEVICE_MAX     1
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_HID_EP_BUFSIZE 64

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN     __attribute__((aligned(4)))

#ifdef __cplusplus
}
#endif

#endif
