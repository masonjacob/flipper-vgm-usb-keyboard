# Flipper Zero Video Game Module USB Keyboard -> Text File

This bundle implements a dedicated USB-host firmware for the Flipper Video Game
Module (RP2040) plus a Flipper Zero FAP.

The VGM USB-C port is used as a USB host for a standard USB HID keyboard. The
RP2040 forwards key-press events to the Flipper Zero over the module UART. The
Flipper FAP receives them, provides a small text editor, and saves:

    /ext/apps_data/usb_text_writer/notes.txt

## Current upstream/API basis

Verified against current upstream documentation/source:

- The VGM repository uses the RP2040 and its existing application build is
  CMake-based. The upstream app currently has a dedicated `usb.c`, `uart.c`,
  `main.c`, and links the Pico SDK/FreeRTOS/protobuf stack.
- Flipper's current firmware supports external FAPs under `applications_user`
  and builds them with `./fbt fap_<APPID>` or `./fbt launch APPSRC=...`.
- Flipper's current expansion API exposes `RECORD_EXPANSION`, and the
  documentation explicitly says applications requiring serial access should
  call `expansion_disable()` before acquiring the serial handle and re-enable
  expansion after releasing it.
- Flipper's current serial HAL exposes `furi_hal_serial_control_acquire()`,
  `furi_hal_serial_control_release()`, `furi_hal_serial_init()`,
  `furi_hal_serial_tx()`, and async RX callbacks.
- TinyUSB's current RP2040 host examples use `tusb_rhport_init_t` with
  `TUSB_ROLE_HOST`, `tuh_task()`, and HID mount/report callbacks.

The implementation deliberately uses a small custom UART protocol rather than
the official VGM protobuf/RPC display protocol. That keeps keyboard transport
independent of the VGM's display-streaming application.

## Build

### 1. VGM firmware

Start from the upstream Video Game Module repository:

    git clone --recursive https://github.com/flipperdevices/video-game-module.git
    cd video-game-module

Copy the bundle's `vgm_firmware/app/*` over the upstream `app/` directory, then:

    cd build
    cmake ..
    make

Flash `build/app/firmware.uf2` to the VGM in BOOTSEL mode.

### 2. Flipper FAP

In a Flipper Zero firmware checkout:

    cp -r flipper_app applications_user/usb_text_writer
    ./fbt fap_usb_text_writer

Or:

    ./fbt launch APPSRC=applications_user/usb_text_writer

## First test

1. Attach the VGM to the Flipper Zero.
2. Connect a standard wired USB keyboard to the VGM USB-C port.
3. Launch `USB Text Writer`.
4. Wait for the app to show `USB`.
5. Type:

       Hello from my USB keyboard!

6. Press the Flipper OK button to save.
7. The file is:

       /ext/apps_data/usb_text_writer/notes.txt

## Current limitations

- ASCII/US-HID layout only.
- USB composite keyboards with a separate non-keyboard HID interface should
  work, but only keyboard interfaces are consumed.
- Clipboard operations are not implemented.
- The editor stores up to 8191 bytes in RAM.
- The bundle is dedicated firmware: it intentionally removes normal VGM
  display/RPC behavior.

## Hardware / power

The VGM documentation says its USB-C port can be used in host mode with custom
applications and that USB Power Delivery is not supported. Keyboard power
requirements therefore need to stay within what the module/USB host path can
provide.
