#pragma once

#include <stdint.h>
#include <stdbool.h>

#define KBW_MAGIC_LEN 4
#define KBW_FRAME_MAGIC 0xE7u

#define KBW_VERSION 1u

/* Host -> VGM */
#define KBW_CMD_PING      0x01u
#define KBW_CMD_START     0x02u
#define KBW_CMD_STOP      0x03u

/* VGM -> Flipper */
#define KBW_EVENT_KEY     0x10u

/* key event:
 *   byte 0 = 0xE7
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

/* Handshake:
 * Flipper -> VGM: "KBW1"
 * VGM -> Flipper: "ACK1"
 */
static const uint8_t kbw_host_magic[KBW_MAGIC_LEN] = {'K', 'B', 'W', '1'};
static const uint8_t kbw_ack_magic[KBW_MAGIC_LEN] = {'A', 'C', 'K', '1'};
