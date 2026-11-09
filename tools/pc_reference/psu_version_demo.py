#!/usr/bin/env python3
"""Cong cu thu nhanh SET/GET_VERSION tren cong COM that (can pyserial) + tu kiem tra ngoai tuyen.

  python psu_version_demo.py --selftest                       # doi chieu vector firmware, khong can phan cung
  python psu_version_demo.py --port COM5 --addr 1 get         # hoi version (app hoac recovery)
  python psu_version_demo.py --port COM5 --addr 1 info        # INFO: slot A/R + trang thai trial
  python psu_version_demo.py --port COM5 --addr 1 set FILE.bin  # CHI trong recovery, sau ERASE (+BEGIN)

Khung: AB CD | CMD(2) | ADDR | payload | CRC16 (BE) | E1 E2; CRC16-CCITT-FALSE tu ADDR den het payload.
"""
import argparse
import os
import sys
import time


def crc16(data: bytes, crc: int = 0xFFFF) -> int:
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def frame(cmd: int, addr: int, payload: bytes = b"") -> bytes:
    f = bytes([0xAB, 0xCD, cmd >> 8, cmd & 0xFF, addr]) + payload
    c = crc16(f[4:])
    return f + bytes([c >> 8, c & 0xFF, 0xE1, 0xE2])


def version_from_filename(path: str) -> str:
    name = os.path.splitext(os.path.basename(path.replace("\\", "/")))[0]
    name = "".join(ch if 0x20 <= ord(ch) <= 0x7E else "_" for ch in name)
    if not name:
        raise ValueError("ten file rong")
    if len(name) > 32:
        raise ValueError("ten file dai hon 32 ky tu, hay doi ten")
    return name


def set_version_frame(addr: int, version: str) -> bytes:
    return frame(0x9A9A, addr, version.encode("ascii").ljust(32, b"\x00"))


def get_version_frame(addr: int) -> bytes:
    return frame(0x9999, addr)


def parse_version_reply(f: bytes):
    """Tra ve (addr, version) hoac None neu khung sai."""
    if len(f) != 41 or f[:2] != b"\xAB\xCD" or f[39:] != b"\xE1\xE2" or f[2:4] != b"\xA9\xA9":
        return None
    if crc16(f[4:37]) != (f[37] << 8 | f[38]):
        return None
    v = bytearray()
    for b in f[5:37]:
        if not 0x20 <= b <= 0x7E:
            break
        v.append(b)
    return f[4], v.decode("ascii")


def parse_ack(f: bytes):
    """ACK 94 94 (17 byte) -> dict hoac None."""
    if len(f) != 17 or f[:4] != b"\xAB\xCD\x94\x94" or f[15:] != b"\xE1\xE2" or crc16(f[4:13]) != (f[13] << 8 | f[14]):
        return None
    return {"addr": f[4], "cmd": f[5], "status": f[6], "info": int.from_bytes(f[7:11], "big"), "slot": chr(f[11]), "state": f[12]}


def selftest() -> int:
    ok = True

    def chk(c, msg):
        nonlocal ok
        if not c:
            print("FAIL:", msg)
            ok = False

    chk(crc16(b"123456789") == 0x29B1, "crc16 vector chuan")
    chk(get_version_frame(1).hex(" ").upper() == "AB CD 99 99 01 F1 D1 E1 E2", "GET_VERSION")
    s = set_version_frame(1, "PSU_v1.2.3_abc123")
    chk(len(s) == 41 and s[:5] == b"\xAB\xCD\x9A\x9A\x01" and s[5:22] == b"PSU_v1.2.3_abc123" and s[22:37] == bytes(15), "SET_VERSION")
    reply = frame(0xA9A9, 1, b"PSU_v1.2.3_abc123".ljust(32, b"\x00"))
    chk(parse_version_reply(reply) == (1, "PSU_v1.2.3_abc123"), "parse A9 A9")
    chk(parse_version_reply(frame(0xA9A9, 1, bytes(32))) == (1, ""), "chuoi rong")
    chk(parse_version_reply(reply[:-1] + b"\x00") is None, "sai tail")
    ack = bytes.fromhex("ABCD94940195000000000041 80D4AFE1E2".replace(" ", ""))
    chk(parse_ack(ack) == {"addr": 1, "cmd": 0x95, "status": 0, "info": 0, "slot": "A", "state": 0x80}, "parse ACK INFO (vector firmware)")
    chk(version_from_filename(r"C:\fw\PSU_v1.2.3_abc123.bin") == "PSU_v1.2.3_abc123", "ten file -> version")
    print("selftest", "OK" if ok else "FAIL")
    return 0 if ok else 1


def exchange(ser, req: bytes, length: int, timeout=0.3, tries=3) -> bytes:
    for _ in range(tries):
        ser.reset_input_buffer()
        ser.write(req)
        end = time.time() + timeout
        buf = b""
        while len(buf) < length and time.time() < end:
            buf += ser.read(length - len(buf))
        if len(buf) == length:
            return buf
    raise TimeoutError("PSU khong tra loi")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--port")
    ap.add_argument("--addr", type=int, default=1)
    ap.add_argument("action", nargs="?", choices=["get", "info", "set"])
    ap.add_argument("file", nargs="?")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    if not a.port or not a.action:
        ap.error("can --port va lenh (get/info/set) hoac --selftest")
    import serial  # pip install pyserial
    with serial.Serial(a.port, 115200, timeout=0.05) as ser:
        if a.action == "get":
            r = parse_version_reply(exchange(ser, get_version_frame(a.addr), 41))
            print("version:", repr(r[1]) if r else "khung sai")
        elif a.action == "info":
            r = parse_ack(exchange(ser, frame(0x9595, a.addr), 17))
            print(r or "khung sai")
        else:
            v = version_from_filename(a.file)
            r = parse_ack(exchange(ser, set_version_frame(a.addr, v), 17))
            print("SET_VERSION", repr(v), "->", r)
    return 0


if __name__ == "__main__":
    sys.exit(main())
