# USB Serial Relay

Firmware that lets a computer switch a relay on and off by sending `ON` / `OFF` over a USB serial port. There are two hardware versions. Both use the same protocol, so a PC program written for one also works with the other.

| Folder | Board | Relay | Relay pin | USB VID:PID |
|---|---|---|---|---|
| [`wio_relay/`](wio_relay/) | Seeed Wio Terminal (SAMD51) | Grove Relay in the right-hand Grove port | `D0` | `2886:802d` |
| [`mkr_relay/`](mkr_relay/) | Arduino MKR WiFi 1010 (SAMD21) | RELAY1 on the MKR Relay Proto Shield | `D1` | `2341:8054` |

Each folder's `README.md` covers wiring, flashing, the Linux udev rule and troubleshooting for that board. The MKR shield relay is rated for **low voltage only** (max 24 VAC / 50 VDC, 1 A). Do not switch mains with it.

## Behaviour (both boards)

- After power-up or reset the relay is always **OFF**.
- After that it keeps the last commanded state, even if the PC closes the serial port or the program crashes.
- It does not wait for a PC connection, so it also works headless.
- The board's LED mirrors the relay state.

## Protocol

Native USB serial (CDC-ACM). The baud rate is ignored, so use 115200. Send one command per line, ending with `\n`. A `\r` is ignored, case doesn't matter, and surrounding spaces are trimmed.

| PC sends | Board replies | Effect |
|---|---|---|
| `ON` | `OK ON` | relay on, LED on |
| `OFF` | `OK OFF` | relay off, LED off |
| `STATUS` | `STATE ON` / `STATE OFF` | none |
| anything else | `ERR UNKNOWN <CMD>` | none |
| line longer than 32 characters | `ERR TOO_LONG` | none |
| empty line | *(no reply)* | none |

On boot the board prints `READY`. You only see it if the port is already open at that moment.

**Never open the port at 1200 baud.** That reboots the board into its bootloader and the relay turns off.

## Quick start

PlatformIO is installed at `~/.platformio/penv/bin/pio` (it is not on PATH). From the folder for your board:

```sh
~/.platformio/penv/bin/pio run -t upload                                   # build + flash
~/.platformio/penv/bin/python tools/relay_test.py /dev/ttyACM0             # protocol self-test
~/.platformio/penv/bin/python tools/relay_test.py /dev/ttyACM0 toggle 5 2  # 5 on/off cycles, 2 s apart
```

Or type commands by hand with `~/.platformio/penv/bin/pio device monitor` (Ctrl+C to quit).

## Writing the PC program

See [`README_AI.md`](README_AI.md). It is the complete interface contract for a host program that works with either board, written so it can be handed directly to an AI or a developer.

## Layout

```
wio_relay/   Wio Terminal firmware, test script and docs
mkr_relay/   MKR WiFi 1010 firmware, test script and docs
  platformio.ini       board configuration
  src/main.cpp         firmware (RELAY_PIN at the top)
  tools/relay_test.py  self-test and example client
  README.md            board-specific setup
  README_AI.md         board-specific host contract
```
