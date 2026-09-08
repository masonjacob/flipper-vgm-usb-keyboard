# VGM USB Keyboard

Flipper Zero companion application for the custom Video Game Module (VGM) USB keyboard-host firmware.

The VGM firmware receives USB HID keyboard events from a keyboard connected to the VGM's USB-C port and forwards them to the Flipper Zero over the expansion UART. This FAP receives those events, provides a small text editor, and saves the document to the Flipper SD card.

## Target firmware

This application is intended to be built against **Momentum Firmware**.

Place the application directory at:

```text
Momentum-Firmware/
└── applications_user/
    └── vgm_usb_keyboard/
        ├── application.fam
        ├── vgm_usb_keyboard.c
        └── README.md
```

The application manifest should use:

```python
appid="vgm_usb_keyboard"
entry_point="vgm_usb_keyboard_app"
```

## Building on NixOS

Momentum normally downloads and uses its own Flipper build toolchain. That toolchain contains generic dynamically linked Linux binaries, which do not run directly on a default NixOS installation.

The simplest development workflow is to run `fbt` through `steam-run`. This provides an FHS-compatible runtime environment while still allowing Momentum to bootstrap and use its own supported toolchain.

### 1. Install or enter an environment with `steam-run`

For a temporary shell:

```bash
nix-shell -p steam-run
```

Alternatively, add `steam-run` to your normal NixOS development environment.

### 2. Clone Momentum Firmware

```bash
git clone --recursive https://github.com/Next-Flip/Momentum-Firmware.git
cd Momentum-Firmware
```

If the repository is already cloned, make sure its submodules are initialized:

```bash
git submodule update --init --recursive
```

### 3. Install the application source

Copy the application directory into:

```text
applications_user/vgm_usb_keyboard/
```

For example:

```bash
cp -r /path/to/vgm_usb_keyboard applications_user/vgm_usb_keyboard
```

### 4. Do not use `FBT_NOENV=1`

For this NixOS workflow, allow Momentum to use its own downloaded toolchain.

If `FBT_NOENV` was previously set, clear it:

```bash
unset FBT_NOENV
```

Do not manually provide SCons, Momentum's Python modules, or a replacement ARM compiler unless you specifically intend to maintain a complete custom FBT toolchain.

### 5. Build the FAP

From the root of the Momentum Firmware repository:

```bash
steam-run ./fbt fap_vgm_usb_keyboard
```

On the first run, Momentum may download and unpack its supported toolchain. This is expected.

A successful build produces `vgm_usb_keyboard.fap` somewhere under the `build/` directory. Locate it with:

```bash
find build -name 'vgm_usb_keyboard.fap'
```

## Build and launch directly on a connected Flipper

Connect the Flipper Zero over USB, then run:

```bash
steam-run ./fbt launch APPSRC=applications_user/vgm_usb_keyboard
```

This builds the application, transfers it to the Flipper, and launches it.

This is the recommended workflow while developing or testing changes.

## Manual installation

If you want to copy the compiled FAP manually, place:

```text
vgm_usb_keyboard.fap
```

on the Flipper SD card at:

```text
/apps/Tools/vgm_usb_keyboard.fap
```

Then launch it from:

```text
Apps → Tools → VGM USB Keyboard
```

## Application behavior

The application:

- disables the normal expansion-module worker while active;
- acquires the expansion USART used to communicate with the VGM;
- sends the `KBW1` handshake to the VGM;
- receives four-byte USB HID keyboard events from the RP2040;
- converts standard US-layout HID usage IDs to text;
- provides basic cursor movement and editing;
- saves the document to the Flipper SD card.

The document is stored at:

```text
/ext/apps_data/vgm_usb_keyboard/notes.txt
```

## Controls

### USB keyboard connected to the VGM

Supported keys include:

- letters and number row;
- Shift and Caps Lock;
- Space, Enter, and Tab;
- Backspace and Delete;
- arrow keys;
- Home and End;
- Page Up and Page Down;
- `Ctrl+S` to save.

### Flipper buttons

- **OK** — save the file;
- **Back** — save and exit;
- **Left / Right** — move the cursor;
- **Up / Down** — scroll the view.

## NixOS troubleshooting

### `Could not start dynamically linked executable: python3`

This means Momentum's downloaded generic Linux toolchain was started directly on NixOS.

Run the build through `steam-run`:

```bash
steam-run ./fbt fap_vgm_usb_keyboard
```

Do not work around this by setting `FBT_NOENV=1` unless you intentionally want to reproduce Momentum's complete build environment yourself.

### Missing Python modules such as `SCons`, `ansi`, `oslex`, or `cxxheaderparser`

These errors normally mean `FBT_NOENV=1` is enabled and `fbt` is using the NixOS system Python instead of Momentum's bundled environment.

Clear it:

```bash
unset FBT_NOENV
```

Then use:

```bash
steam-run ./fbt fap_vgm_usb_keyboard
```

### Unsupported ARM GCC version

Momentum validates the compiler version it supports. If you see a message similar to:

```text
Toolchain version is not supported
```

make sure you are not overriding Momentum's bundled toolchain with a system ARM GCC. Clear `FBT_NOENV` and run through `steam-run`.

## VGM firmware requirement

This FAP requires the matching custom VGM firmware to be installed on the Video Game Module. The VGM and FAP must use the same UART settings and `KBW1` keyboard-event protocol.

If the app starts but displays `NO VGM`, the Flipper application is running but the UART handshake with the Video Game Module has not succeeded.
