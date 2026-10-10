# Wio Terminal USB Relay

Firmware for a Seeed Wio Terminal that switches a Grove Relay when a computer sends `ON` / `OFF` over the USB-C serial port.

## Hardware

- Seeed Wio Terminal (ATSAMD51) connected to the PC with a USB-C cable that **carries data** (many cables are charge-only).
- Grove Relay plugged into the **right-hand Grove port** (seen from the front), which is pin `D0`.
  This port supplies 3.3 V. The standard Grove Relay (v1.2) works at that voltage.
- The blue user LED on the bottom edge (between the USB-C port and the right Grove port) mirrors the relay state.

Power switch (left side): **up** = off, **middle** = on, **down** = reset (springs back).

## Behaviour

- After power-up or reset the relay is always **OFF**.
- After that it keeps the last commanded state, even if the PC closes the serial port or the program crashes.
- It does not wait for a PC connection, so it also works headless.

## Protocol

Native USB serial (CDC-ACM). The baud rate is ignored, so use 115200. Send one command per line, ending with `\n`. A `\r` is ignored, case doesn't matter, and surrounding spaces are trimmed.

| PC sends | Wio replies | Effect |
|---|---|---|
| `ON` | `OK ON` | relay on, LED on |
| `OFF` | `OK OFF` | relay off, LED off |
| `STATUS` | `STATE ON` / `STATE OFF` | none |
| anything else | `ERR UNKNOWN <CMD>` | none |
| line longer than 32 characters | `ERR TOO_LONG` | none |
| empty line | *(no reply)* | none |

On boot the board prints `READY`. You only see it if the port is already open at that moment.

## Build and flash (PlatformIO)

PlatformIO is installed at `~/.platformio/penv/bin/pio` (it is not on PATH). Run these from this directory:

```sh
~/.platformio/penv/bin/pio run                 # build
~/.platformio/penv/bin/pio run -t upload       # build + flash
~/.platformio/penv/bin/pio device monitor      # interactive terminal (type ON / OFF, Ctrl+C to quit)
```

Uploading resets the board into the bootloader automatically (a "1200-baud touch"), and `bossac` writes the flash.

**If the upload fails or the port disappears**, slide the power switch **down twice quickly**. The board then enters bootloader mode (the blue LED pulses slowly) and always shows up over USB. Upload again.

## Linux setup (one-time)

This udev rule stops ModemManager from probing the port and makes it accessible:

```sh
echo 'ATTRS{idVendor}=="2886", ENV{ID_MM_DEVICE_IGNORE}="1", MODE="0666"' \
  | sudo tee /etc/udev/rules.d/99-seeed-wio.rules
sudo udevadm control --reload && sudo udevadm trigger
```

The board appears as `/dev/ttyACM0`, or with a stable name under `/dev/serial/by-id/usb-Seeed_Studio_Seeed_Wio_Terminal_*`.
USB IDs: `2886:802d` (application) and `2886:002d` (bootloader).

Troubleshooting: if `lsusb | grep 2886` prints nothing, check the power switch (middle), the cable (data, not charge-only), and the port. Then try the double-slide.

## Testing

`tools/relay_test.py` needs `pyserial`. PlatformIO's own Python already includes it:

```sh
~/.platformio/penv/bin/python tools/relay_test.py /dev/ttyACM0                # protocol self-test
~/.platformio/penv/bin/python tools/relay_test.py /dev/ttyACM0 toggle 5 2     # 5 on/off cycles, 2 s apart
```

The `Relay` class in that script is a minimal client you can copy into other programs.

## Files

- `platformio.ini`: board and framework configuration (`atmelsam` / `seeed_wio_terminal` / Arduino)
- `src/main.cpp`: the firmware (`RELAY_PIN` at the top sets the pin)
- `tools/relay_test.py`: test script and example client
- `README_AI.md`: specification for an AI or developer writing the PC-side program
