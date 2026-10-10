# MKR WiFi 1010 USB Relay

Firmware for an Arduino MKR WiFi 1010 on an Arduino MKR Relay Proto Shield. It switches **RELAY1** when a computer sends `ON` / `OFF` over the USB serial port. The protocol is identical to `../wio_relay`.

## Hardware

- Arduino MKR WiFi 1010 (SAMD21) plugged into the MKR Relay Proto Shield and connected to the PC with a micro-USB cable that **carries data** (many cables are charge-only).
- The firmware uses only **RELAY1** (driven by pin `D1`). RELAY2 (pin `D2`) is not used and stays off.
- Wire the load to RELAY1's screw terminals: **COM + NO** (closed while ON) or **COM + NC** (closed while OFF).
- Contact rating (Omron G5V-1): max **24 VAC / 50 VDC**, 1 A switching, 2 A carry, 30 W / 62.5 VA. **Low voltage only, no mains.**
- The shield's RELAY1 LED and the board's built-in LED (`D6`) both mirror the relay state.
- Power: the shield is powered from the MKR board. USB power is enough. (I could not check from the schematic which rail feeds the relay coils. If you run on battery, check that the relay still clicks.)

## Behaviour

- After power-up or reset the relay is always **OFF**.
- After that it keeps the last commanded state, even if the PC closes the serial port or the program crashes.
- It does not wait for a PC connection, so it also works headless.

## Protocol

Native USB serial (CDC-ACM). The baud rate is ignored, so use 115200. Send one command per line, ending with `\n`. A `\r` is ignored, case doesn't matter, and surrounding spaces are trimmed.

| PC sends | MKR replies | Effect |
|---|---|---|
| `ON` | `OK ON` | relay on, LEDs on |
| `OFF` | `OK OFF` | relay off, LEDs off |
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

**If the upload fails or the port disappears**, **double-press the RESET button** quickly. The board then enters bootloader mode (the built-in LED fades in and out) and always shows up over USB. Upload again.

## Linux setup (one-time)

This udev rule stops ModemManager from probing the port and makes it accessible:

```sh
echo 'ATTRS{idVendor}=="2341", ENV{ID_MM_DEVICE_IGNORE}="1", MODE="0666"' \
  | sudo tee /etc/udev/rules.d/99-arduino-mkr.rules
sudo udevadm control --reload && sudo udevadm trigger
```

The board appears as `/dev/ttyACM0`, or with a stable name under `/dev/serial/by-id/`. That name is something like `usb-Arduino_LLC_Arduino_MKR_WiFi_1010_*-if00`; run `ls /dev/serial/by-id/` to see the exact one.
USB IDs: `2341:8054` (application) and `2341:0054` (bootloader).

Troubleshooting: if `lsusb | grep 2341` prints nothing, check the cable (data, not charge-only) and the port. Then try the RESET double-press.

## Testing

`tools/relay_test.py` needs `pyserial`. PlatformIO's own Python already includes it:

```sh
~/.platformio/penv/bin/python tools/relay_test.py /dev/ttyACM0                # protocol self-test
~/.platformio/penv/bin/python tools/relay_test.py /dev/ttyACM0 toggle 5 2     # 5 on/off cycles, 2 s apart
```

The `Relay` class in that script is a minimal client you can copy into other programs.

## Files

- `platformio.ini`: board and framework configuration (`atmelsam` / `mkrwifi1010` / Arduino)
- `src/main.cpp`: the firmware (`RELAY_PIN` at the top sets the pin; set it to `2` to use RELAY2)
- `tools/relay_test.py`: test script and example client
- `README_AI.md`: specification for an AI or developer writing the PC-side program
