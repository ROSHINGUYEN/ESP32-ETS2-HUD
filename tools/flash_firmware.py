"""
Script nạp firmware thông minh cho ESP32-S3 (MIU-ETS2) qua cổng COM5.
Tự động chờ người dùng đưa bo mạch vào chế độ Download Bootloader (BOOT + RST).
Ngay khi phát hiện chip, tự động nạp bootloader, partition table và firmware.
"""

import sys
import os
import time
import subprocess

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(PROJECT_DIR, "tools"))
try:
    from miu_desktop_ets2 import find_default_port
    PORT = find_default_port() or "COM4"
except Exception:
    PORT = "COM4"
BUILD_DIR = os.path.join(PROJECT_DIR, ".pio", "build", "eilik_s3")

BOOTLOADER = os.path.join(BUILD_DIR, "bootloader.bin")
PARTITIONS = os.path.join(BUILD_DIR, "partitions.bin")
FIRMWARE = os.path.join(BUILD_DIR, "firmware.bin")

def check_files():
    for f in [BOOTLOADER, PARTITIONS, FIRMWARE]:
        if not os.path.exists(f):
            print(f"[LOI] Khong tim thay file: {f}")
            return False
    return True

def flash_now():
    if not check_files():
        return False

    cmd = [
        sys.executable, "-m", "esptool",
        "--chip", "esp32s3",
        "--port", PORT,
        "--baud", "460800",
        "--before", "default_reset",
        "--after", "hard_reset",
        "write_flash", "-z",
        "--flash_mode", "dio",
        "--flash_freq", "80m",
        "--flash_size", "16MB",
        "0x0000", BOOTLOADER,
        "0x8000", PARTITIONS,
        "0x10000", FIRMWARE
    ]

    print("=" * 60)
    print(" DANG NAP FIRMWARE XUONG ESP32-S3 (COM5 @ 460800 Baud)...")
    print("=" * 60)

    # Thử nạp tối đa 3 lần trong lúc người dùng giữ nút BOOT + RST
    for attempt in range(1, 4):
        print(f"\n[LAN {attempt}/3] Dang ket noi voi ESP32-S3 tren {PORT}...")
        print(">>> HAY NHAN GIU NUT 'BOOT', BAM NHA NUT 'RST', ROI THA NUT 'BOOT' <<<")
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode == 0:
            print("\n" + "=" * 60)
            print(" [THANH CONG] NAP CODE MOI XUONG ESP32-S3 HOAN TAT 100%!")
            print("=" * 60)
            print(proc.stdout)
            return True
        else:
            print(f"[CHUA KET NOI DUOC] Chi tiet: {proc.stderr.strip() or proc.stdout.strip()[-200:]}")
            time.sleep(2)

    return False

if __name__ == "__main__":
    success = flash_now()
    sys.exit(0 if success else 1)
