#!/usr/bin/env python3
"""
=====================================================================================
DỰ ÁN: ROBOT MIU EILIK ESP32-S3
FILE: pc_tools/eilik_bridge.py
MÔ TẢ: Công cụ cầu nối PC (Python 3.10+) truyền âm thanh hệ thống (WASAPI loopback),
       video/ảnh dither Floyd-Steinberg lên OLED 128x64, gửi lệnh và nhận Telemetry.
=====================================================================================
"""

import sys
import os

if sys.platform.startswith("win"):
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

import time
import struct
import argparse
import threading
import serial
import serial.tools.list_ports
import numpy as np
from PIL import Image

# Định nghĩa các hằng số giao thức khung nhị phân
SYNC_1 = 0xE1
SYNC_2 = 0x1C

MSG_HELLO_CAPS   = 0x01
MSG_AUDIO_CFG    = 0x02
MSG_AUDIO_DATA   = 0x03
MSG_AUDIO_CREDIT = 0x04
MSG_FRAME_1BPP   = 0x05
MSG_CMD          = 0x06
MSG_TELEM        = 0x07
MSG_PING_PONG    = 0x7F

def compute_crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF

def build_packet(msg_type: int, flags: int, payload: bytes) -> bytes:
    length = len(payload)
    header = struct.pack("<BBBBH", SYNC_1, SYNC_2, msg_type, flags, length)
    crc_data = header[2:] + payload
    crc = compute_crc16(crc_data)
    return header + payload + struct.pack("<H", crc)

def find_eilik_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        # VID/PID thông dụng của ESP32-S3 USB Native JTAG/CDC
        if (p.vid == 0x303A) or ("ESP32" in p.description) or ("USB Serial" in p.description):
            return p.device
    if ports:
        return ports[0].device
    return None

class EilikBridge:
    def __init__(self, port_name, baud=115200):
        self.port_name = port_name
        self.baud = baud
        self.ser = None
        self.running = False
        self.audio_credit = 1024
        self.sample_pts = 0

    def connect(self):
        print(f"[BRIDGE] Đang kết nối tới cổng {self.port_name}...")
        self.ser = serial.Serial(self.port_name, self.baud, timeout=0.1)
        self.running = True
        time.sleep(1.0)
        # Gửi gói tin Hello
        pkt = build_packet(MSG_HELLO_CAPS, 0, b'{"client":"eilik_bridge_py"}')
        self.ser.write(pkt)
        print("[BRIDGE] Đã gửi thông điệp HELLO_CAPS.")

    def rx_worker(self):
        buf = bytearray()
        while self.running and self.ser and self.ser.is_open:
            try:
                raw = self.ser.read(64)
                if raw:
                    buf.extend(raw)
                    while len(buf) >= 8:
                        if buf[0] == SYNC_1 and buf[1] == SYNC_2:
                            mtype, flags, length = struct.unpack("<BBH", buf[2:6])
                            total_len = 6 + length + 2
                            if len(buf) >= total_len:
                                payload = buf[6:6+length]
                                crc = struct.unpack("<H", buf[6+length:total_len])[0]
                                self.handle_packet(mtype, flags, payload)
                                buf = buf[total_len:]
                            else:
                                break
                        else:
                            buf.pop(0)
            except Exception as e:
                time.sleep(0.01)

    def handle_packet(self, mtype, flags, payload):
        if mtype == MSG_AUDIO_CREDIT:
            req, fill, underruns = struct.unpack("<HBH", payload)
            self.audio_credit = req
        elif mtype == MSG_TELEM:
            print(f"[ROBOT TELEM] {payload.decode('utf-8', errors='ignore')}")

    def send_cmd(self, json_str: str):
        pkt = build_packet(MSG_CMD, 0, json_str.encode('utf-8'))
        self.ser.write(pkt)

    def send_frame_image(self, img_pil):
        # Resize về 128x64 và dither 1-bit Floyd-Steinberg
        img_gray = img_pil.convert('L').resize((128, 64))
        img_1bit = img_gray.convert('1', dither=Image.FLOYDSTEINBERG)
        arr = np.array(img_1bit, dtype=np.uint8)

        # Chuyển đổi ma trận ảnh thành Page-major (8 trang x 128 cột = 1024 bytes)
        packed = bytearray(1024)
        for page in range(8):
            for col in range(128):
                byte_val = 0
                for bit in range(8):
                    row = page * 8 + bit
                    if arr[row, col] != 0:
                        byte_val |= (1 << bit)
                packed[page * 128 + col] = byte_val

        payload = struct.pack("<I", self.sample_pts) + packed
        pkt = build_packet(MSG_FRAME_1BPP, 0, payload)
        self.ser.write(pkt)

    def close(self):
        self.running = False
        if self.ser:
            self.ser.close()

def main():
    parser = argparse.ArgumentParser(description="Eilik Robot PC Bridge Tool")
    parser.add_argument("--port", type=str, default=None, help="Cổng COM Serial")
    parser.add_argument("--cmd", type=str, default=None, help="Gửi chuỗi lệnh JSON")
    parser.add_argument("--image", type=str, default=None, help="Gửi ảnh lên màn hình OLED")
    args = parser.parse_args()

    port = args.port or find_eilik_port()
    if not port:
        print("[LỖI] Không tìm thấy cổng Serial của robot! Hãy chỉ định --port COMx")
        sys.exit(1)

    bridge = EilikBridge(port)
    bridge.connect()

    t_rx = threading.Thread(target=bridge.rx_worker, daemon=True)
    t_rx.start()

    if args.cmd:
        print(f"[BRIDGE] Đang gửi lệnh: {args.cmd}")
        bridge.send_cmd(args.cmd)
        time.sleep(0.5)
    elif args.image:
        print(f"[BRIDGE] Đang nạp ảnh: {args.image}")
        img = Image.open(args.image)
        bridge.send_frame_image(img)
        time.sleep(1.0)
    else:
        print("[BRIDGE] Đang lắng nghe Telemetry (Nhấn Ctrl+C để thoát)...")
        try:
            while True:
                time.sleep(0.2)
        except KeyboardInterrupt:
            pass

    bridge.close()

if __name__ == "__main__":
    main()
