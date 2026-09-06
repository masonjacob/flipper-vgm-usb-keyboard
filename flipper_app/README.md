# USB Text Writer

Companion Flipper Zero FAP for the VGM USB keyboard-host firmware.

Place this directory at:

    flipperzero-firmware/applications_user/usb_text_writer

Build:

    ./fbt fap_usb_text_writer

Or build + launch directly:

    ./fbt launch APPSRC=applications_user/usb_text_writer

The app:
- disables the normal expansion-module worker,
- acquires the USART connected to the VGM,
- performs a small `KBW1` handshake,
- receives 4-byte keyboard events,
- edits an in-memory ASCII document,
- saves it to `/ext/apps_data/usb_text_writer/notes.txt`.

Controls:
- VGM keyboard: typing and editing
- Flipper OK: Save
- Flipper Back: Quit (the app saves first)
- Flipper Up/Down: scroll
- Flipper Left/Right: move cursor

This app targets current Flipper Zero firmware APIs. External FAPs only
receive symbols exposed by the firmware API table, so if your firmware branch
does not export the direct serial/expansion symbols, build it as an internal
application or add those symbols to the target API export list.
