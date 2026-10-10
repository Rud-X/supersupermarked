# Instructions for writing the PC-side relay client

You are writing a computer program that switches a relay on and off. The relay (RELAY1 of an Arduino MKR Relay Proto Shield) is driven by an Arduino MKR WiFi 1010 microcontroller connected over USB. The firmware is finished and tested, so **do not change it**. Your job is only the host side. This document is the complete interface contract.

## Device

- USB CDC-ACM serial device (a virtual COM port).
  - Linux: `/dev/ttyACM*`. Prefer the stable path `/dev/serial/by-id/usb-Arduino_LLC_Arduino_MKR_WiFi_1010_*-if00` (check the exact name with `ls /dev/serial/by-id/`).
  - Windows: `COMx`. macOS: `/dev/cu.usbmodem*`.
- USB VID:PID while running: **`2341:8054`**. Auto-detect it with `serial.tools.list_ports` (pyserial), matching `vid == 0x2341 and pid == 0x8054`. Do not hard-code `COMx` or `ttyACM0` without an override option.
- Serial settings: 8N1, no flow control. The baud rate is **ignored** by the device; use **115200**.
- **NEVER open the port at 1200 baud.** On this board, opening and closing at 1200 baud reboots it into the bootloader (PID becomes `2341:0054`), the firmware stops running, and the relay goes OFF.
- Opening or closing the port at any other baud rate does **not** reset the board and does **not** change the relay state.

## Protocol

ASCII, line based, request/response. Send **one command at a time** and wait for its reply before sending the next.

Request: the command followed by `\n`. (`\r\n` is also accepted. Case-insensitive. Surrounding whitespace is trimmed.)

| Send | Reply (followed by `\r\n`) | Meaning |
|---|---|---|
| `ON\n` | `OK ON` | relay is now energised |
| `OFF\n` | `OK OFF` | relay is now released |
| `STATUS\n` | `STATE ON` or `STATE OFF` | current state, no change |
| any other text | `ERR UNKNOWN <TEXT UPPERCASED>` | ignored, state unchanged |
| line > 32 chars | `ERR TOO_LONG` | ignored, state unchanged |
| empty line | *(nothing)* | do not wait for a reply |

- Replies arrive within a few milliseconds. Use a read timeout of about **1 s**. If no reply arrives in time, treat the device as disconnected or unresponsive.
- `ON` when already on (or `OFF` when already off) is harmless and still returns `OK ...`. Commands are idempotent.
- **Unsolicited line:** the device prints `READY` once at boot. If the board resets while your program has the port open, a stray `READY` can arrive. Discard any `READY` line while waiting for a reply, then keep reading for the real reply.
- After opening the port, wait about 0.3 s, then flush the input buffer before sending the first command.

## Device state rules

- After power-up, reset, or USB power loss, the relay is **always OFF**.
- Otherwise the relay keeps the last commanded state indefinitely, even if the host program exits or closes the port.
- So, if the program needs a specific state, it must **set that state explicitly** after every (re)connect. Don't assume it persisted, and don't assume it is off. Send `ON` or `OFF`, or query `STATUS`.
- If the program must guarantee the relay is OFF when it exits, send `OFF` on shutdown (in a `finally` / signal handler). The firmware will not do it automatically.

## Robustness requirements

- Only one process can have the port open at a time. Keep **one long-lived connection** owned by one component. Don't open and close the port per command. If several threads need the relay, serialise access (a lock or a single worker queue).
- Verify every reply. Anything other than the expected `OK ON` / `OK OFF` is an error to surface to the caller, not something to ignore silently.
- Handle unplugging: a write or read raises `serial.SerialException` (or an OS error). Close the port, poll for the device to reappear (by VID:PID, every ~1 s), reopen it, and re-apply the desired state.
- The port name may change after a reconnect (e.g. `ttyACM0` → `ttyACM1`). Re-detect by VID:PID.

## Reference implementation (Python, pyserial)

A tested minimal client is in `tools/relay_test.py` (class `Relay`). Build on it. The core is:

```python
import time, serial
from serial.tools import list_ports

def find_port():
    for p in list_ports.comports():
        if p.vid == 0x2341 and p.pid == 0x8054:
            return p.device
    raise RuntimeError("MKR WiFi 1010 not found")

ser = serial.Serial(find_port(), 115200, timeout=1)
time.sleep(0.3)
ser.reset_input_buffer()

def command(cmd):
    ser.write((cmd + "\n").encode("ascii"))
    while True:
        reply = ser.readline().decode("ascii", errors="replace").strip()
        if not reply:
            raise TimeoutError(f"no reply to {cmd!r}")
        if reply != "READY":
            return reply

assert command("ON") == "OK ON"
assert command("OFF") == "OK OFF"
```

Other languages work the same way. Open the serial port at 115200, write `CMD\n`, and read one `\r\n`-terminated line.

## Testing without the real program

`python tools/relay_test.py [PORT]` runs a protocol self-test against the device. `python tools/relay_test.py [PORT] toggle N SECONDS` clicks the relay N times. Use these to tell device or wiring faults apart from bugs in your client.
