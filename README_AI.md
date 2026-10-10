# Instructions for writing the PC-side relay client

You are writing a computer program that switches a relay on and off. The relay is driven by a microcontroller connected over USB. There are two supported boards, and they run the same protocol. The firmware is finished and tested, so **do not change it**. Your job is only the host side. This document is the complete interface contract. The program should work with **either board** without code changes.

## Supported devices

| Board | Running VID:PID | Bootloader VID:PID | Linux stable path |
|---|---|---|---|
| Seeed Wio Terminal | `2886:802d` | `2886:002d` | `/dev/serial/by-id/usb-Seeed_Studio_Seeed_Wio_Terminal_*-if00` |
| Arduino MKR WiFi 1010 + MKR Relay Proto Shield | `2341:8054` | `2341:0054` | `/dev/serial/by-id/usb-Arduino_LLC_Arduino_MKR_WiFi_1010_*-if00` |

- Both are USB CDC-ACM serial devices (a virtual COM port). Linux: `/dev/ttyACM*`. Windows: `COMx`. macOS: `/dev/cu.usbmodem*`.
- Auto-detect the device with `serial.tools.list_ports` (pyserial) by matching either **running** VID:PID above. Do not hard-code `COMx` or `ttyACM0`, but do offer a manual port override option.
- If more than one matching device is connected, do not silently pick one. Require the user to choose (e.g. by port or serial number).
- A device that shows a **bootloader** VID:PID is not running the firmware. Report it ("board is in bootloader mode, reflash or reset it"), don't try to talk to it.
- Serial settings: 8N1, no flow control. The baud rate is **ignored** by the device; use **115200**.
- **NEVER open the port at 1200 baud.** On both boards, opening and closing at 1200 baud reboots into the bootloader, the firmware stops running, and the relay goes OFF.
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

```python
import time, serial
from serial.tools import list_ports

SUPPORTED = {
    (0x2886, 0x802D): "Wio Terminal",
    (0x2341, 0x8054): "MKR WiFi 1010",
}
BOOTLOADER = {(0x2886, 0x002D), (0x2341, 0x0054)}

def find_port():
    found, in_bootloader = [], []
    for p in list_ports.comports():
        if (p.vid, p.pid) in SUPPORTED:
            found.append(p.device)
        elif (p.vid, p.pid) in BOOTLOADER:
            in_bootloader.append(p.device)
    if len(found) == 1:
        return found[0]
    if len(found) > 1:
        raise RuntimeError(f"several relay boards found, choose one: {found}")
    if in_bootloader:
        raise RuntimeError(f"board in bootloader mode on {in_bootloader}")
    raise RuntimeError("no relay board found")

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

Each board folder also has `tools/relay_test.py` with a minimal `Relay` class. Note that its `command()` does **not** discard a stray `READY` line; use the loop above instead.

## Testing without the real program

From either board folder, `python tools/relay_test.py [PORT]` runs a protocol self-test against the device, and `python tools/relay_test.py [PORT] toggle N SECONDS` clicks the relay N times. Use these to tell device or wiring faults apart from bugs in your client.
