#!/usr/bin/env python3
"""Simple Bluetooth CLI for Arduino-like devices (HC-05/HC-06 style SPP).

This script connects to a Bluetooth MAC address over RFCOMM, then lets you:
- type commands and send them as newline-terminated text
- see incoming responses in real time

Example:
    python bluetooth_arduino_cli.py --mac 98:D3:31:FC:12:34
"""

from __future__ import annotations

import argparse
import re
import socket
import subprocess
import sys
import threading
import time


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Send/receive text commands to Arduino over Bluetooth RFCOMM"
    )
    parser.add_argument(
        "--mac",
        help="Bluetooth MAC address (example: 98:D3:31:FC:12:34)",
    )
    parser.add_argument(
        "--channel",
        type=int,
        default=1,
        help="RFCOMM channel (default: 1)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=0.25,
        help="Socket timeout in seconds (default: 0.25)",
    )
    parser.add_argument(
        "--scan",
        action="store_true",
        help="Scan nearby Bluetooth devices before connecting",
    )
    parser.add_argument(
        "--scan-timeout",
        type=int,
        default=8,
        help="Seconds to scan for nearby devices (default: 8)",
    )
    return parser.parse_args()


def _parse_devices(text: str) -> list[tuple[str, str]]:
    pattern = re.compile(r"Device\s+([0-9A-F:]{17})\s+(.+)$", re.MULTILINE)
    out: list[tuple[str, str]] = []
    seen: set[str] = set()
    for mac, name in pattern.findall(text):
        mac = mac.upper()
        if mac not in seen:
            out.append((mac, name.strip()))
            seen.add(mac)
    return out


def scan_devices(scan_timeout: int) -> list[tuple[str, str]]:
    """Scan nearby devices and return [(mac, name), ...].

    Uses bluetoothctl output parsing. On Linux this works with BlueZ.
    """
    collected: list[tuple[str, str]] = []
    seen: set[str] = set()

    commands = [
        ["bluetoothctl", "--timeout", str(scan_timeout), "scan", "on"],
        ["bluetoothctl", "devices"],
    ]

    for cmd in commands:
        try:
            result = subprocess.run(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                timeout=max(5, scan_timeout + 4),
                check=False,
            )
        except FileNotFoundError:
            # bluetoothctl not available on PATH.
            return []
        except subprocess.TimeoutExpired:
            continue

        for mac, name in _parse_devices(result.stdout):
            if mac not in seen:
                collected.append((mac, name))
                seen.add(mac)

    return collected


def choose_device(devices: list[tuple[str, str]]) -> str | None:
    if not devices:
        print("[info] No Bluetooth devices found during scan")
        return None

    print("[info] Discovered devices:")
    for i, (mac, name) in enumerate(devices, start=1):
        print(f"  {i}. {name} ({mac})")

    print('[info] Pick a number, type a MAC address, or "q" to quit')
    while True:
        raw = input("[scan] ").strip()
        if not raw:
            continue
        if raw.lower() in {"q", "quit", "exit"}:
            return None
        if raw.isdigit():
            idx = int(raw)
            if 1 <= idx <= len(devices):
                return devices[idx - 1][0]
            print("[error] Invalid index")
            continue
        if re.fullmatch(r"[0-9A-Fa-f:]{17}", raw):
            return raw.upper()
        print("[error] Enter a valid index or MAC address")


def reader_loop(sock: socket.socket, stop_event: threading.Event) -> None:
    """Read incoming bytes, split by newlines, and print lines."""
    buffer = b""
    while not stop_event.is_set():
        try:
            chunk = sock.recv(1024)
            if not chunk:
                print("\n[info] Connection closed by remote device")
                stop_event.set()
                return

            buffer += chunk
            while b"\n" in buffer:
                line, buffer = buffer.split(b"\n", 1)
                text = line.decode(errors="replace").strip()
                if text:
                    print(f"[rx] {text}")
        except TimeoutError:
            continue
        except OSError as exc:
            if not stop_event.is_set():
                print(f"\n[error] Receive failed: {exc}")
                stop_event.set()
            return


def main() -> int:
    args = parse_args()

    if not args.mac and not args.scan:
        print("[error] Provide --mac or use --scan")
        return 2

    if not hasattr(socket, "AF_BLUETOOTH"):
        print("[error] This Python build/platform does not support Bluetooth sockets")
        return 2

    target_mac = args.mac
    if args.scan:
        print(f"[info] Scanning for Bluetooth devices for {args.scan_timeout}s ...")
        devices = scan_devices(args.scan_timeout)
        selected = choose_device(devices)
        if selected:
            target_mac = selected
            print(f"[info] Selected {target_mac}")
        elif not target_mac:
            print("[error] No device selected")
            return 1

    if not target_mac:
        print("[error] No target MAC provided")
        return 2

    sock = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_STREAM, socket.BTPROTO_RFCOMM)
    sock.settimeout(args.timeout)

    print(f"[info] Connecting to {target_mac} on channel {args.channel} ...")
    try:
        sock.connect((target_mac, args.channel))
    except OSError as exc:
        print(f"[error] Could not connect: {exc}")
        sock.close()
        return 1

    print("[info] Connected")
    print("[info] Type commands and press Enter")
    print('[info] Type "exit" or press Ctrl+C to quit')

    stop_event = threading.Event()
    thread = threading.Thread(target=reader_loop, args=(sock, stop_event), daemon=True)
    thread.start()

    try:
        while not stop_event.is_set():
            try:
                line = input("[tx] ").strip()
            except EOFError:
                break

            if not line:
                continue
            if line.lower() in {"exit", "quit"}:
                break

            payload = (line + "\n").encode("ascii", errors="replace")
            try:
                sock.sendall(payload)
            except OSError as exc:
                print(f"[error] Send failed: {exc}")
                break
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        try:
            # Shutdown first so recv loop exits immediately on supported stacks.
            sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        sock.close()
        thread.join(timeout=1.0)
        time.sleep(0.05)

    print("[info] Disconnected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
