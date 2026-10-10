#!/usr/bin/env python3
"""Test the Wio Terminal relay firmware, and serve as an example client.

Usage:
    python relay_test.py [PORT]                      # protocol self-test (default)
    python relay_test.py [PORT] toggle [N] [SECONDS] # click the relay N times

PORT defaults to /dev/ttyACM0 (on Windows e.g. COM5). Requires pyserial.
"""
import sys
import time

import serial


class Relay:
    """Minimal client for the Wio relay protocol. Copy this into your own program."""

    def __init__(self, port, timeout=1.0):
        self.ser = serial.Serial(port, 115200, timeout=timeout)
        time.sleep(0.3)  # let the USB CDC link settle
        self.ser.reset_input_buffer()  # drop a possible "READY" banner

    def command(self, cmd):
        self.ser.write((cmd + "\n").encode("ascii"))
        reply = self.ser.readline().decode("ascii", errors="replace").strip()
        if not reply:
            raise TimeoutError(f"no reply to {cmd!r}")
        return reply

    def on(self):
        if self.command("ON") != "OK ON":
            raise RuntimeError("relay did not acknowledge ON")

    def off(self):
        if self.command("OFF") != "OK OFF":
            raise RuntimeError("relay did not acknowledge OFF")

    def is_on(self):
        return self.command("STATUS") == "STATE ON"

    def close(self):
        self.ser.close()


def self_test(relay):
    cases = [
        ("OFF", "OK OFF"),
        ("STATUS", "STATE OFF"),
        ("ON", "OK ON"),
        ("STATUS", "STATE ON"),
        ("  off\r", "OK OFF"),
        ("status", "STATE OFF"),
        ("foo", "ERR UNKNOWN FOO"),
        ("X" * 40, "ERR TOO_LONG"),
    ]
    failed = 0
    for cmd, expected in cases:
        got = relay.command(cmd)
        ok = got == expected
        failed += not ok
        print(f"{'PASS' if ok else 'FAIL'}  {cmd[:20]!r:24} -> {got!r}"
              + ("" if ok else f" (expected {expected!r})"))
    print("ALL PASSED" if not failed else f"{failed} FAILED")
    return failed == 0


def toggle(relay, count, period):
    for i in range(count):
        relay.on()
        print(f"{i + 1}/{count} ON")
        time.sleep(period)
        relay.off()
        print(f"{i + 1}/{count} OFF")
        time.sleep(period)


def main(argv):
    port = argv[1] if len(argv) > 1 else "/dev/ttyACM0"
    mode = argv[2] if len(argv) > 2 else "test"
    relay = Relay(port)
    try:
        if mode == "toggle":
            count = int(argv[3]) if len(argv) > 3 else 5
            period = float(argv[4]) if len(argv) > 4 else 2.0
            toggle(relay, count, period)
            return 0
        return 0 if self_test(relay) else 1
    finally:
        relay.close()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
