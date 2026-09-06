# Flipper Zero VGM USB Text Writer firmware

This is a dedicated Video Game Module firmware variant that turns the VGM's USB-C
connector into a USB HID keyboard host and forwards keyboard press events over the
VGM<->Flipper UART to the companion `usb_text_writer` FAP.

Base this directory on a checkout of:
https://github.com/flipperdevices/video-game-module

Copy the files from `app/` over the matching upstream `app/` files, then run:

    cd video-game-module
    ( cd build && cmake .. && make )

The resulting UF2 is in `build/app/firmware.uf2` (and the project also creates
the versioned `vgm-fw-*.uf2` output).

Important:
- This firmware intentionally replaces the normal VGM screen/RPC behavior.
- The USB-C port is used in USB host mode. A keyboard must not be attached
  through a passive USB-C cable that leaves the module in device mode.
- Keyboard translation assumes standard USB HID boot-protocol usage IDs.
- The transport between VGM and Flipper is a small raw UART protocol, not the
  normal VGM protobuf/RPC session.
