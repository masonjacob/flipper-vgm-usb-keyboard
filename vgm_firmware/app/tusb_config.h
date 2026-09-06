#ifndef _TUSB_CONFIG_VGM_KEYBOARD_H_
#define _TUSB_CONFIG_VGM_KEYBOARD_H_

#define CFG_TUSB_MCU               OPT_MCU_RP2040
#define CFG_TUSB_OS                OPT_OS_FREERTOS
#define CFG_TUSB_DEBUG             0

#define CFG_TUH_ENABLED            1
#define CFG_TUH_RHPORT             0
#define CFG_TUH_MAX_SPEED          OPT_MODE_FULL_SPEED
#define CFG_TUH_HID                4
#define CFG_TUH_CDC                0
#define CFG_TUH_MSC                0
#define CFG_TUH_VENDOR             0

#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_TASK_QUEUE_SZ      16

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN         __attribute__((aligned(4)))

#endif
