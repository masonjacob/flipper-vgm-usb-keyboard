#pragma once

#include <stdint.h>
#include <stdbool.h>

#define KBW_MAGIC_LEN        4u
#define KBW_FRAME_MAGIC      0xE7u
#define KBW_EVENT_KEY        0x10u

static const uint8_t kbw_host_magic[KBW_MAGIC_LEN] = {'K', 'B', 'W', '1'};
static const uint8_t kbw_ack_magic[KBW_MAGIC_LEN] = {'A', 'C', 'K', '1'};

/* VGM -> Flipper key frame:
 *   byte 0 = KBW_FRAME_MAGIC
 *   byte 1 = KBW_EVENT_KEY
 *   byte 2 = HID modifier bitmap
 *   byte 3 = HID usage ID
 */
typedef struct {
    uint8_t magic;
    uint8_t type;
    uint8_t modifiers;
    uint8_t usage;
} KbwKeyEvent;
