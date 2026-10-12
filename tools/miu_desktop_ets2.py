#!/usr/bin/env python3
"""
Miu Desktop Companion - Euro Truck Simulator 2 / Sim Racing Dashboard
Cầu nối telemetry giữa ETS2 Telemetry Server và Robot ESP32-S3 qua USB Serial.
"""

import sys
import os
import time
import math
import json
import argparse
import threading
import ctypes
import struct
import traceback
from ctypes import wintypes
from typing import Optional, Dict, Any

repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if repo_root not in sys.path:
    sys.path.insert(0, repo_root)

def _global_exception_hook(ex_cls, ex_val, tb):
    try:
        crash_file = os.path.join(os.path.dirname(__file__), "crash.log")
        with open(crash_file, "a", encoding="utf-8") as f:
            f.write(f"\n--- CRASH AT {time.strftime('%Y-%m-%d %H:%M:%S')} ---\n")
            traceback.print_exception(ex_cls, ex_val, tb, file=f)
    except Exception:
        pass

sys.excepthook = _global_exception_hook

class SafeStdoutWriter:
    def __init__(self, original, fallback_path=None):
        self.orig = original
        self.fallback = None
        if fallback_path:
            try:
                self.fallback = open(fallback_path, "a", encoding="utf-8", buffering=1)
            except Exception:
                pass

    def write(self, s):
        if self.orig:
            try:
                self.orig.write(s)
            except Exception:
                pass
        if self.fallback:
            try:
                self.fallback.write(s)
            except Exception:
                pass
        return len(s) if s else 0

    def flush(self):
        if self.orig:
            try:
                self.orig.flush()
            except Exception:
                pass
        if self.fallback:
            try:
                self.fallback.flush()
            except Exception:
                pass

    @property
    def encoding(self):
        return "utf-8"

    def isatty(self):
        return False

    def fileno(self):
        if self.fallback:
            return self.fallback.fileno()
        raise OSError("fileno not available")

log_path = os.path.join(os.path.dirname(__file__), "daemon.log")
sys.stdout = SafeStdoutWriter(sys.stdout, log_path)
sys.stderr = SafeStdoutWriter(sys.stderr, log_path)

try:
    import serial
    import serial.tools.list_ports as list_ports
except ImportError:
    print("[ERROR] Thieu thu vien pyserial. Cai dat bang: pip install pyserial")
    sys.exit(1)

try:
    import requests
    _REQUESTS_OK = True
except ImportError:
    _REQUESTS_OK = False
    print("[WARN] Khong co thu vien requests - chi dung Shared Memory (khong fallback HTTP)")

try:
    import psutil
    _PSUTIL_OK = True
except ImportError:
    _PSUTIL_OK = False


DEFAULT_BAUD = 115200
DEFAULT_ETS2_URL = "http://127.0.0.1:25555/api/ets2/telemetry"
DEFAULT_TEST_JSON = os.path.join(
    os.path.dirname(__file__), "mock_telemetry.json"
)


class Ets2SharedMemoryReader:
    """Doc truc tiep du lieu Telemetry tu Windows Shared Memory cua game Euro Truck Simulator 2."""

    def __init__(self):
        self.k32 = ctypes.windll.kernel32
        self.k32.OpenFileMappingW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
        self.k32.OpenFileMappingW.restype = wintypes.HANDLE
        self.k32.MapViewOfFile.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD, ctypes.c_size_t]
        self.k32.MapViewOfFile.restype = ctypes.c_void_p
        self.k32.CloseHandle.argtypes = [wintypes.HANDLE]
        self.k32.UnmapViewOfFile.argtypes = [ctypes.c_void_p]
        self.hMap = None
        self.pBuf = None
        self.hMapScs = None
        self.pBufScs = None
        self.ets2_pid: Optional[int] = None

    def _find_ets2_pid(self) -> Optional[int]:
        if not _PSUTIL_OK:
            return None
        try:
            for p in psutil.process_iter(['pid', 'name']):
                pname = (p.info['name'] or '').lower()
                if pname in ('eurotrucks2.exe', 'amtrucks.exe'):
                    return p.info['pid']
        except Exception:
            pass
        return None

    def _ensure_scs_telemetry(self):
        """Thu ket noi them vung nho Local\\SCSTelemetry de lay gia tri hop so dashboard chuan."""
        if not self.pBufScs:
            try:
                self.hMapScs = self.k32.OpenFileMappingW(0x0004, False, "Local\\SCSTelemetry")
                if self.hMapScs:
                    self.pBufScs = self.k32.MapViewOfFile(self.hMapScs, 0x0004, 0, 0, 32768)
            except Exception:
                pass

    def connect(self) -> bool:
        if self.pBuf:
            if _PSUTIL_OK and self.ets2_pid:
                if not psutil.pid_exists(self.ets2_pid):
                    self.close()
                    return False
            self._ensure_scs_telemetry()
            return True
        # File mapping name do ets2-telemetry-server.dll tao trong game
        self.hMap = self.k32.OpenFileMappingW(0x0004, False, "Local\\Ets2TelemetryServer")
        if self.hMap:
            self.pBuf = self.k32.MapViewOfFile(self.hMap, 0x0004, 0, 0, 2048)
            if self.pBuf:
                self.ets2_pid = self._find_ets2_pid()
                self._ensure_scs_telemetry()
                return True
            self.k32.CloseHandle(self.hMap)
            self.hMap = None

        # Neu chua mo duoc SHM (game dang khoi dong trong doan intro), kiem tra tien trinh eurotrucks2
        if _PSUTIL_OK:
            now = time.time()
            if now - getattr(self, "_last_pid_scan", 0.0) >= 0.5:
                self._last_pid_scan = now
                pid = self._find_ets2_pid()
                if pid:
                    self.ets2_pid = pid
                    return True
        return False

    def close(self):
        if self.pBuf:
            try:
                self.k32.UnmapViewOfFile(ctypes.c_void_p(self.pBuf))
            except Exception:
                pass
            self.pBuf = None
        if self.hMap:
            try:
                self.k32.CloseHandle(self.hMap)
            except Exception:
                pass
            self.hMap = None
        if self.pBufScs:
            try:
                self.k32.UnmapViewOfFile(ctypes.c_void_p(self.pBufScs))
            except Exception:
                pass
            self.pBufScs = None
        if self.hMapScs:
            try:
                self.k32.CloseHandle(self.hMapScs)
            except Exception:
                pass
            self.hMapScs = None
        self.ets2_pid = None

    # Funbit ETS2 Telemetry Plugin shared memory struct offsets (verified from plugin source)
    # Offset 0:   uint32  time          - tick counter / simulation timestamp
    # Offset 4:   uint32  paused        - 1 if game is paused
    # Offset 8:   uint32  pluginVersion - plugin version number (phai >= 1)
    # Offset 24:  float   speed         - truck speed in m/s
    # Offset 64:  int32   gear          - current gear
    # Offset 80:  float[4] rpm, rpmMax, fuel, fuelCapacity
    # Offset 160: int32   timeAbsolute  - tong so phut troi qua trong the gioi game ETS2 (chuan 100%)
    # Offset 564: int32   retarder
    # Offset 656: float[6] wear (engine, trans, cabin, chassis, wheels, trailer)
    # Offset 680: float   odometer (km)
    # Offset 684: float   cruiseControlSpeed (m/s)
    # Offset 996: int32   displayedGear
    # Offset 1000: float  navigationDistance (m)
    # Offset 1008: float  navigationSpeedLimit (m/s)

    def read(self) -> Optional[Dict[str, Any]]:
        if not self.connect():
            return None
        try:
            if _PSUTIL_OK and self.ets2_pid:
                if not psutil.pid_exists(self.ets2_pid):
                    self.close()
                    return None

            # Neu chua co pBuf (game dang trong doan intro khoi dong chua kip nap plugin):
            if not self.pBuf:
                # Thu ket noi lai SHM neu plugin vua tao xong
                self.hMap = self.k32.OpenFileMappingW(0x0004, False, "Local\\Ets2TelemetryServer")
                if self.hMap:
                    self.pBuf = self.k32.MapViewOfFile(self.hMap, 0x0004, 0, 0, 2048)
                if not self.pBuf:
                    # Tra ve goi connected mac dinh de ESP32 hien thi Loading Splash ngay trong doan intro!
                    return {
                        "game": "ets2",
                        "spd": 0, "lim": 0, "cc": 0, "cc_on": 0,
                        "gear": 0, "rpm": 0, "rpm_max": 2500,
                        "fuel": 100, "fuel_l": 0, "fuel_cap": 0, "fuel_warn": 0,
                        "bl_l": 0, "bl_r": 0, "low": 0, "high": 0, "park": 1,
                        "air": 0, "ret": 0, "wip": 0, "bcn": 0, "mbrk": 0,
                        "wear": 0.0, "trk_w": 0, "crg_w": -1, "trl_on": 0,
                        "odo": 0, "eta_d": 0, "gh": -1, "gm": -1,
                        "conn": 1, "pause": 1,
                    }

            raw = ctypes.string_at(self.pBuf, 1024)

            # Offset 8: pluginVersion (phai >= 1 neu DLL telemetry da duoc nap trong game)
            plugin_ver = struct.unpack_from("<I", raw, 8)[0]
            if plugin_ver < 1:
                return None

            # Offset 4: paused flag
            paused = struct.unpack_from("<I", raw, 4)[0]

            # Offset 160: timeAbsolute (minutes elapsed in game world, non-zero = in-game)
            time_absolute = struct.unpack_from("<i", raw, 160)[0]

            # Khi time_absolute == 0: Game dang o man hinh menu / profile / loading
            # ETS2 da bat va DLL telemetry dang chay -> gui payload connected de ESP32 hien thi Loading Splash!
            if time_absolute <= 0:
                return {
                    "game": "ets2",
                    "spd": 0,
                    "lim": 0,
                    "cc": 0,
                    "cc_on": 0,
                    "gear": 0,
                    "rpm": 0,
                    "rpm_max": 2500,
                    "fuel": 100,
                    "fuel_l": 0,
                    "fuel_cap": 0,
                    "fuel_warn": 0,
                    "bl_l": 0,
                    "bl_r": 0,
                    "low": 0,
                    "high": 0,
                    "park": 1,
                    "air": 0,
                    "ret": 0,
                    "wip": 0,
                    "bcn": 0,
                    "mbrk": 0,
                    "wear": 0.0,
                    "trk_w": 0,
                    "crg_w": -1,
                    "trl_on": 0,
                    "odo": 0,
                    "eta_d": 0,
                    "gh": -1,
                    "gm": -1,
                    "conn": 1,
                    "pause": 1,
                }

            # Decode game clock tu timeAbsolute (offset 160) - tong so phut trong game ETS2 (chuan 100%)
            game_h = (time_absolute // 60) % 24
            game_m = time_absolute % 60

            speed_ms = struct.unpack_from("<f", raw, 24)[0]
            gear = struct.unpack_from("<i", raw, 64)[0]
            rpm, rpmMax, fuel, fuelCap = struct.unpack_from("<4f", raw, 80)
            retarder = struct.unpack_from("<i", raw, 564)[0]
            cc_on = raw[580]
            wipers = raw[581]
            park = raw[582]
            mbrk = raw[583]
            bl_l = raw[586]
            bl_r = raw[587]
            low = raw[591]
            high = raw[592]
            beacon = raw[604]
            air = raw[608]
            fuel_warn = struct.unpack_from("<i", raw, 624)[0]

            trailer_on = 1 if raw[21] != 0 else 0
            wears = struct.unpack_from("<6f", raw, 656)
            # wears[0..4]: engine, trans, cabin, chassis, wheels
            truck_wear_pct = int(round(100.0 * (sum(wears[:5]) / 5.0)))
            # wears[5]: wearTrailer
            cargo_wear_pct = int(round(wears[5] * 100.0)) if trailer_on else -1
            total_wear = float(sum(wears))

            odometer = struct.unpack_from("<f", raw, 680)[0]
            odo_km = int(round(odometer)) if (0 < odometer < 10000000.0) else 0

            fuel_l = int(round(fuel)) if fuel > 0 else 0
            fuel_cap = int(round(fuelCap)) if fuelCap > 0 else 0
            fuel_pct = int(round(100.0 * fuel / fuelCap)) if fuelCap > 0 else 50

            cc_spd = struct.unpack_from("<f", raw, 684)[0] * 3.6
            disp_gear = struct.unpack_from("<i", raw, 996)[0]
            fwd_gears = struct.unpack_from("<i", raw, 68)[0]
            nav_d = struct.unpack_from("<f", raw, 1000)[0] / 1000.0
            nav_lim = struct.unpack_from("<f", raw, 1008)[0] * 3.6
            spd_kmh = max(0, int(round(speed_ms * 3.6)))
            eta_d_km = int(round(nav_d)) if (0 < nav_d < 50000.0) else 0

            # Xac dinh cap so hien thi (D1..D12) dong bo 100% voi taplo trong game:
            # Uu tien 1: Doc gearDashboard tu Local\SCSTelemetry (offset 508, SCS SDK chuan)
            final_gear = None
            if self.pBufScs:
                try:
                    raw_scs = ctypes.string_at(self.pBufScs, 1024)
                    scs_dash_gear = struct.unpack_from("<i", raw_scs, 508)[0]
                    scs_mech_gear = struct.unpack_from("<i", raw_scs, 504)[0]
                    if scs_dash_gear != 0:
                        final_gear = scs_dash_gear
                    elif scs_mech_gear == 0:
                        final_gear = 0
                except Exception:
                    pass

            # Uu tien 2 (Fallback Funbit): Bu tru crawler gear neu hop so co crawler (vi du Scania 12+2)
            if final_gear is None:
                raw_g = disp_gear if disp_gear != 0 else gear
                if fwd_gears > 12 and raw_g > 0:
                    crawlers = fwd_gears - 12
                    final_gear = (raw_g - crawlers) if raw_g > crawlers else raw_g
                else:
                    final_gear = raw_g

            return {
                "game": "ets2",
                "spd": spd_kmh,
                "lim": int(round(nav_lim)) if nav_lim > 0 else 0,
                "cc": int(round(cc_spd)) if cc_spd > 0 else 0,
                "cc_on": 1 if cc_on else 0,
                "gear": int(final_gear),
                "rpm": int(round(rpm)),
                "rpm_max": int(round(rpmMax if rpmMax > 500 else 2500)),
                "fuel": fuel_pct,
                "fuel_l": fuel_l,
                "fuel_cap": fuel_cap,
                "fuel_warn": 1 if (fuel_warn > 0 or fuel_pct < 15) else 0,
                "bl_l": 1 if bl_l else 0,
                "bl_r": 1 if bl_r else 0,
                "low": 1 if low else 0,
                "high": 1 if high else 0,
                "park": 1 if park else 0,
                "air": 1 if air else 0,
                "ret": int(retarder),
                "wip": 1 if wipers else 0,
                "bcn": 1 if beacon else 0,
                "mbrk": 1 if mbrk else 0,
                "wear": total_wear,
                "trk_w": truck_wear_pct,
                "crg_w": cargo_wear_pct,
                "trl_on": trailer_on,
                "odo": odo_km,
                "eta_d": eta_d_km,
                "gh": game_h,
                "gm": game_m,
                "conn": 1,
                "pause": 1 if (paused != 0) else 0,
            }
        except Exception:
            self.close()
            return None


def find_default_port() -> Optional[str]:
    """Tự động tìm cổng COM của ESP32-S3 hoặc CH340/CH343 (Loại trừ tuyệt đối COM1 mainboard)."""
    ports = list(list_ports.comports())
    if not ports:
        return None

    usb_candidates = []
    for p in ports:
        dev = p.device.upper()
        desc = (p.description or "").lower()
        hwid = (p.hwid or "").lower()

        # Bỏ qua COM1 và cổng ACPI PNP của mainboard
        if dev == "COM1" or "acpi\\pnp0501" in hwid or "communications port" in desc:
            continue

        # Ưu tiên cao nhất: CH340, CH343, ESP32, CP210x, USB Serial
        if any(sig in desc or sig in hwid for sig in ["ch34", "esp32", "cp210", "usb serial", "usb-serial", "ftdi"]):
            return p.device

        # Cổng bất kỳ có chứa chữ ký USB
        if "usb" in hwid or "usb" in desc:
            usb_candidates.append(p.device)

    if usb_candidates:
        return usb_candidates[0]

    # Tuyệt đối KHÔNG trả về COM1 nếu chưa tìm thấy thiết bị USB
    return None


class Ets2DesktopCompanion:
    def __init__(
        self,
        port: str,
        baud: int = DEFAULT_BAUD,
        server_url: Optional[str] = None,
        fps: int = 25,
        demo: bool = False,
        test_file: Optional[str] = None,
    ):
        self.specified_port = port
        self.port_name = port
        self.baud = baud
        self.server_url = server_url or DEFAULT_ETS2_URL

        self.target_interval = 1.0 / max(1, fps)
        self.demo = demo
        self.test_file = test_file or DEFAULT_TEST_JSON
        self.ser: Optional[serial.Serial] = None
        self.running = False

        # Session HTTP tai su dung TCP socket keep-alive (giam do tre < 2ms)
        self.session = requests.Session() if _REQUESTS_OK else None

        # Demo simulation state
        self._sim_t = 0.0
        self._sim_speed = 0.0
        self._sim_gear = 1
        self._sim_rpm = 800.0
        self._sim_fuel = 95.0

        # Statistics
        self.packets_sent = 0
        self.server_ok = False
        self.last_fps_calc = time.time()
        self.fps_counter = 0
        self.current_fps = 0.0
        self.game_connected_prev = False

        # Reader doc truc tiep Windows Shared Memory cua ETS2 (khong can mo port 25555)
        self.shm_reader = (
            Ets2SharedMemoryReader() if sys.platform == "win32" else None
        )
        self.source_name = "WAITING"

        # Event Tracking State (Fines, tolls, damage & cockpit actions)
        self.prev_ret = 0
        self.prev_wip = 0
        self.prev_bcn = 0
        self.prev_mbrk = 0
        self.prev_park = 0
        self.prev_cc_on = 0
        self.prev_wear = 0.0
        self.overspeed_start = 0.0
        self.last_speeding_fine = 0.0
        self.toll_stop_time = 0.0
        self.toll_was_moving = False
        self.active_event_text = ""
        self.event_expire_time = 0.0

    def process_events(self, p: Dict[str, Any]):
        now = time.time()
        spd = p.get("spd", 0)
        lim = p.get("lim", 0)
        ret = p.get("ret", 0)
        wip = p.get("wip", 0)
        bcn = p.get("bcn", 0)
        mbrk = p.get("mbrk", 0)
        park = p.get("park", 0)
        cc_on = p.get("cc_on", 0)
        cc_spd = p.get("cc", 0)
        wear = p.get("wear", 0.0)

        new_event = None

        # 1. Va cham tai nan (Collision / Crash wear increase)
        if self.prev_wear > 0.0 and wear > (self.prev_wear + 0.002):
            new_event = "VA CHAM: -400 EUR"
        self.prev_wear = wear

        # 2. Phat chay qua toc do (Speed camera fine)
        if lim > 0 and spd > (lim + 3):
            if self.overspeed_start == 0.0:
                self.overspeed_start = now
            elif (now - self.overspeed_start >= 3.0) and (now - self.last_speeding_fine >= 18.0):
                new_event = "PHAT TOC DO: -550 EUR"
                self.last_speeding_fine = now
                self.overspeed_start = 0.0
        else:
            self.overspeed_start = 0.0

        # 3. Tram thu phi cao toc (Toll gate fee)
        if spd > 25 and lim >= 60:
            self.toll_was_moving = True
        elif spd == 0 and self.toll_was_moving:
            if self.toll_stop_time == 0.0:
                self.toll_stop_time = now
        elif spd > 5 and self.toll_stop_time > 0.0:
            if now - self.toll_stop_time >= 1.5:
                new_event = "THU PHI: -45 EUR"
            self.toll_stop_time = 0.0
            self.toll_was_moving = False

        # 4. Thao tac phim buong lai (Cockpit buttons)
        if not new_event:
            if ret != self.prev_ret:
                if ret > 0:
                    new_event = f"RETARDER: BAC {ret}"
                else:
                    new_event = "RETARDER: TAT"
            elif wip != self.prev_wip:
                new_event = "GAT MUA: BAT" if wip else "GAT MUA: TAT"
            elif bcn != self.prev_bcn:
                new_event = "DEN NOC: BAT" if bcn else "DEN NOC: TAT"
            elif mbrk != self.prev_mbrk:
                new_event = "PHANH MAY: BAT" if mbrk else "PHANH MAY: TAT"
            elif park != self.prev_park:
                new_event = "PHANH TAY: KEO" if park else "PHANH TAY: NHA"
            elif cc_on != self.prev_cc_on:
                new_event = f"CRUISE: {cc_spd} km/h" if cc_on else "CRUISE: TAT"

        self.prev_ret = ret
        self.prev_wip = wip
        self.prev_bcn = bcn
        self.prev_mbrk = mbrk
        self.prev_park = park
        self.prev_cc_on = cc_on

        if new_event:
            self.active_event_text = new_event
            self.event_expire_time = now + 2.2
            print(f"\n[SU KIEN COCKPIT] >> {new_event} <<")

        if now < self.event_expire_time and self.active_event_text:
            p["evt"] = self.active_event_text

    def open_serial(self) -> bool:
        if not self.port_name:
            self.port_name = find_default_port()
            if not self.port_name:
                return False

        if self.port_name.upper() == "COM1":
            print("[SERIAL] Bo qua COM1 vi la cong mainboard, khong phai robot ESP32.")
            if not self.specified_port:
                self.port_name = None
            return False

        try:
            print(f"[SERIAL] Dang mo cong {self.port_name} ({self.baud} baud)...")
            self.ser = serial.Serial()
            self.ser.port = self.port_name
            self.ser.baudrate = self.baud
            self.ser.timeout = 0.1
            self.ser.write_timeout = 2.0
            self.ser.dtr = False
            self.ser.rts = False
            self.ser.open()
            time.sleep(0.3)
            self.game_connected_prev = False
            # Neu o che do DEMO thi bat che do GAME ngay
            if self.demo:
                self.send_command("MODE=GAME")
            print(f"[SERIAL] Da ket noi thanh cong voi {self.port_name} (ETS2 Telemetry)!")
            return True
        except Exception as e:
            print(f"[SERIAL] Khong the mo cong {self.port_name}: {e}")
            self.ser = None
            if not self.specified_port:
                self.port_name = None
            return False

    def close_serial(self):
        if self.ser:
            try:
                if self.ser.is_open:
                    self.ser.close()
            except Exception:
                pass
            self.ser = None
            print("\n[SERIAL] Da ngat ket noi Serial an toan.")
        if not self.specified_port:
            self.port_name = None
        self.game_connected_prev = False

    def ensure_serial(self) -> bool:
        """Tu dong kiem tra va ket noi lai cong Serial neu bi ngat."""
        if self.ser and self.ser.is_open:
            if getattr(self.ser, "port", "").upper() == "COM1":
                self.close_serial()
                return False
            return True
        return self.open_serial()

    def send_command(self, cmd: str):
        if self.ser and self.ser.is_open:
            try:
                line = (cmd + "\n").encode("utf-8")
                self.ser.write(line)
            except Exception as e:
                print(f"[WARN] Mat ket noi Serial: {e}")
                self.close_serial()

    def check_mcp_commands(self):
        """Kiem tra va chuyen tiep lenh tu cong MCP Server hoac client ngoai vi."""
        cmd_file = os.path.join(os.path.dirname(__file__), "mcp_command.txt")
        if os.path.exists(cmd_file):
            try:
                with open(cmd_file, "r", encoding="utf-8") as f:
                    cmd = f.read().strip()
                try:
                    os.remove(cmd_file)
                except Exception:
                    pass
                if cmd:
                    self.send_command(cmd)
                    print(f"\n[MCP] >> Da chuyen tiep lenh toi ESP32: {cmd} <<")
            except Exception:
                pass

    def dump_state(self, payload: Optional[Dict[str, Any]] = None):
        """Ghi trang thai telemetry va ket noi ra file json cho MCP Server va Dashboard doc."""
        now = time.time()
        if now - getattr(self, "_last_state_dump", 0.0) < 0.3:
            return
        self._last_state_dump = now
        state_file = os.path.join(os.path.dirname(__file__), "telemetry_state.json")
        try:
            data = {
                "daemon_running": True,
                "pid": os.getpid(),
                "port": self.port_name,
                "baud": self.baud,
                "serial_connected": bool(self.ser and self.ser.is_open),
                "game": self.game,
                "game_connected": bool(self.game_connected_prev),
                "source": self.source_name,
                "fps": round(self.current_fps, 1),
                "packets_sent": self.packets_sent,
                "active_event": self.active_event_text if now < self.event_expire_time else "",
                "timestamp": now,
                "telemetry": payload or {},
            }
            tmp_file = state_file + ".tmp"
            with open(tmp_file, "w", encoding="utf-8") as f:
                json.dump(data, f, ensure_ascii=False, indent=2)
            os.replace(tmp_file, state_file)
        except Exception:
            pass

    def fetch_server_telemetry(self) -> Optional[Dict[str, Any]]:
        """Lay telemetry tu game server qua HTTP REST (ETS2 port 25555 hoac FS25 port 8080/SimHub)."""
        if not _REQUESTS_OK or not self.session:
            return None
        try:
            r = self.session.get(self.server_url, timeout=0.15)
            if r.status_code == 200:
                self.server_ok = True
                return r.json()
        except Exception:
            self.server_ok = False
        return None

    def fetch_live_telemetry(self) -> Optional[Dict[str, Any]]:
        """Lay telemetry uu tien doc truc tiep Shared Memory, sau do fallback sang REST API."""
        # 1. Uu tien tuyet doi: Doc truc tiep tu Windows Shared Memory (Local\Ets2TelemetryServer)
        if self.shm_reader:
            shm_data = self.shm_reader.read()
            if shm_data:
                self.server_ok = True
                self.source_name = "SHARED MEMORY (DIRECT)"
                return shm_data

        # 2. Fallback sang HTTP Telemetry Server (Port 25555 cho ETS2)
        raw = self.fetch_server_telemetry()
        if raw:
            self.server_ok = True
            self.source_name = f"HTTP REST ({self.server_url})"
            return self.extract_telemetry_payload(raw)

        self.server_ok = False
        self.source_name = "WAITING"
        return None

    def generate_demo_telemetry(self) -> Dict[str, Any]:
        """Tao du lieu mo phong lai xe thuc te neu khong bat game."""
        self._sim_t += self.target_interval

        # Mo phong chu ky tang toc -> chay deu -> giam toc
        cycle = (self._sim_t % 40.0) / 40.0  # Chu ky 40s
        if cycle < 0.4:
            # Tang toc
            progress = cycle / 0.4
            self._sim_speed = 10.0 + progress * 75.0  # 10 -> 85 km/h
            self._sim_gear = min(12, max(1, int(1 + progress * 11)))
            self._sim_rpm = 1100.0 + (math.sin(self._sim_t * 3.0) * 300.0)
        elif cycle < 0.75:
            # Toc do cao on dinh tren cao toc
            self._sim_speed = 85.0 + math.sin(self._sim_t * 0.5) * 3.0
            self._sim_gear = 12
            self._sim_rpm = 1250.0 + math.sin(self._sim_t * 0.8) * 100.0
        else:
            # Giam toc / dung den do
            progress = (cycle - 0.75) / 0.25
            self._sim_speed = max(0.0, 85.0 * (1.0 - progress))
            self._sim_gear = max(0, int(12 * (1.0 - progress)))
            self._sim_rpm = 750.0 + self._sim_speed * 10.0

        # Nhap nhay xi nhan luc re
        blinker_l = (cycle < 0.15 or cycle > 0.85) and (int(self._sim_t * 2.5) % 2 == 0)
        blinker_r = (0.35 < cycle < 0.45) and (int(self._sim_t * 2.5) % 2 == 0)

        # Xang giam dan nhe
        self._sim_fuel = max(10.0, 92.0 - (self._sim_t * 0.05))

        # Demo cac su kien va icon chuc nang
        demo_evt = ""
        sec = int(self._sim_t) % 30
        ret_val = 0
        wip_val = 0
        if 4 <= sec <= 6:
            demo_evt = "RETARDER: BAC 2"
            ret_val = 2
        elif 10 <= sec <= 12:
            demo_evt = "THU PHI: -45 EUR"
        elif 16 <= sec <= 18:
            demo_evt = "PHAT TOC DO: -550 EUR"
        elif 22 <= sec <= 24:
            demo_evt = "GAT MUA: BAT"
            wip_val = 1
        elif 26 <= sec <= 28:
            demo_evt = "VA CHAM: -400 EUR"

        res = {
            "game": "ets2",
            "spd": int(round(self._sim_speed)),
            "lim": 80 if cycle < 0.6 else 60,
            "cc": 85 if (0.4 <= cycle < 0.75) else 0,
            "cc_on": 1 if (0.4 <= cycle < 0.75) else 0,
            "gear": self._sim_gear,
            "rpm": int(round(self._sim_rpm)),
            "rpm_max": 2500,
            "fuel": int(round(self._sim_fuel)),
            "fuel_l": int(round(self._sim_fuel * 8.5)),
            "fuel_cap": 900,
            "fuel_warn": 1 if self._sim_fuel < 15.0 else 0,
            "bl_l": 1 if blinker_l else 0,
            "bl_r": 1 if blinker_r else 0,
            "low": 1,
            "high": 0,
            "park": 1 if self._sim_speed == 0 else 0,
            "air": 0,
            "ret": ret_val,
            "wip": wip_val,
            "bcn": 1 if (0.4 <= cycle < 0.6) else 0,
            "mbrk": 0,
            "wear": 0.02,
            "trk_w": 2 if cycle < 0.8 else 5,
            "crg_w": 0,
            "trl_on": 1,
            "odo": int(round(12450 + self._sim_t * 0.1)),
            "eta_d": max(5, int(145 - self._sim_t * 0.5)),
            "eta_t": max(5, int(110 - self._sim_t * 0.4)),
            "gh": int(14 + (self._sim_t / 120.0)) % 24,
            "gm": int((self._sim_t * 2) % 60),
            "conn": 1,
            "pause": 0,
        }
        if demo_evt:
            res["evt"] = demo_evt
        return res

    def extract_telemetry_payload(self, raw: Dict[str, Any]) -> Dict[str, Any]:
        """Trich xuat cac truong can thiet tu JSON goc cua Telemetry Server."""
        game = raw.get("game", {})
        truck = raw.get("truck", {})
        nav = raw.get("navigation", {})

        fuel = truck.get("fuel", 500.0)
        fuel_cap = truck.get("fuelCapacity", 700.0)
        fuel_pct = int(round(100.0 * fuel / fuel_cap)) if fuel_cap > 0 else 50
        fuel_l = int(round(fuel)) if fuel > 0 else 0
        fuel_cap_l = int(round(fuel_cap)) if fuel_cap > 0 else 0

        # Trailer & Cargo damage
        trailer = raw.get("trailer", {})
        trailer_on = 1 if trailer.get("attached", False) else 0
        cargo_wear_pct = int(round(trailer.get("wear", 0.0) * 100.0)) if trailer_on else -1

        # Truck wear (average across 5 components)
        truck_wear_avg = (
            truck.get("wearEngine", 0.0) +
            truck.get("wearTransmission", 0.0) +
            truck.get("wearCabin", 0.0) +
            truck.get("wearChassis", 0.0) +
            truck.get("wearWheels", 0.0)
        ) / 5.0
        truck_wear_pct = int(round(truck_wear_avg * 100.0))
        odo_km = int(round(truck.get("odometer", 0.0)))

        # Gear: displayedGear hoac gear (kem bu tru crawler gear)
        gear = truck.get("displayedGear", truck.get("gear", 0))
        fwd_gears = truck.get("forwardGears", 12)
        if fwd_gears > 12 and gear > 0:
            crawlers = fwd_gears - 12
            if gear > crawlers:
                gear = gear - crawlers

        # ETA
        eta_d = int(round(nav.get("estimatedDistance", 0) / 1000.0))

        # Game time
        gh = -1
        gm = -1
        gt = game.get("time")
        if isinstance(gt, str) and "T" in gt:
            try:
                t_part = gt.split("T")[1][:5]
                gh, gm = map(int, t_part.split(":"))
            except Exception:
                pass
        elif isinstance(gt, (int, float)):
            gh = (int(gt) // 60) % 24
            gm = int(gt) % 60

        payload = {
            "game": "ets2",
            "spd": int(round(truck.get("speed", 0.0))),
            "lim": int(nav.get("speedLimit", 0)),
            "cc": int(round(truck.get("cruiseControlSpeed", 0.0))),
            "cc_on": 1 if truck.get("cruiseControlOn", False) else 0,
            "gear": int(gear),
            "rpm": int(round(truck.get("engineRpm", 0.0))),
            "rpm_max": int(round(truck.get("engineRpmMax", 2500.0))),
            "fuel": fuel_pct,
            "fuel_l": fuel_l,
            "fuel_cap": fuel_cap_l,
            "fuel_warn": 1 if truck.get("fuelWarningOn", False) else 0,
            "bl_l": 1 if (truck.get("blinkerLeftActive", False) or truck.get("blinkerLeftOn", False)) else 0,
            "bl_r": 1 if (truck.get("blinkerRightActive", False) or truck.get("blinkerRightOn", False)) else 0,
            "low": 1 if truck.get("lightsBeamLowOn", False) else 0,
            "high": 1 if truck.get("lightsBeamHighOn", False) else 0,
            "park": 1 if truck.get("parkBrakeOn", False) else 0,
            "air": 1 if truck.get("airPressureWarningOn", False) else 0,
            "ret": int(truck.get("retarderBrake", 0)),
            "wip": 1 if truck.get("wipersOn", False) else 0,
            "bcn": 1 if (truck.get("lightsBeaconOn", False) or truck.get("beaconOn", False)) else 0,
            "mbrk": 1 if truck.get("motorBrakeOn", False) else 0,
            "wear": float(truck.get("wearEngine", 0.0) + truck.get("wearTransmission", 0.0) + truck.get("wearCabin", 0.0)),
            "trk_w": truck_wear_pct,
            "crg_w": cargo_wear_pct,
            "trl_on": trailer_on,
            "odo": odo_km,
            "eta_d": eta_d,
            "gh": gh,
            "gm": gm,
            "conn": 1 if game.get("connected", False) else 0,
            "pause": 1 if game.get("paused", False) else 0,
        }
        return payload

    def send_telemetry_packet(self, payload: Dict[str, Any]):
        """Dong goi va truyen sang ESP32 duoi dang JSON gon nhe."""
        if not self.ser or not self.ser.is_open:
            return

        json_bytes = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        line = b"ETS2=" + json_bytes + b"\n"

        try:
            self.ser.write(line)
            self.packets_sent += 1
            self.fps_counter += 1
        except (serial.SerialException, OSError) as e:
            print(f"\n[SERIAL ERROR] Mat ket noi voi robot: {e}")
            self.close_serial()
        except serial.SerialTimeoutException:
            # Buffer timeout ngan mot khung hinh, bo qua de khong gay mat ket noi
            pass
        except Exception as e:
            print(f"\n[WRITE ERROR] {e}")
            self.close_serial()

    def update_fps(self):
        now = time.time()
        elapsed = now - self.last_fps_calc
        if elapsed >= 1.0:
            self.current_fps = self.fps_counter / elapsed
            self.fps_counter = 0
            self.last_fps_calc = now

    def render_console_dashboard(self, p: Dict[str, Any]):
        """Ve bang dieu khien truc quan trong terminal."""
        self.update_fps()
        spd = p.get("spd", 0)
        lim = p.get("lim", 0)
        gear = p.get("gear", 0)
        gear_str = f"D{gear}" if gear > 0 else ("N" if gear == 0 else f"R{abs(gear)}")
        cc = p.get("cc", 0)
        cc_str = f"CRUISE {cc}k" if p.get("cc_on", 0) else "          "
        rpm = p.get("rpm", 0)
        fuel = p.get("fuel", 0)
        bl_l = "<" if p.get("bl_l", 0) else " "
        bl_r = ">" if p.get("bl_r", 0) else " "
        lim_str = f"[{lim}]" if lim > 0 else "[--]"
        warn_str = "! OVER" if (lim > 0 and spd > lim) else "      "

        ret = p.get("ret", 0)
        ret_str = f"RET:{ret}" if ret > 0 else "     "
        wip_str = "WIP" if p.get("wip", 0) else "   "
        bcn_str = "BCN" if p.get("bcn", 0) else "   "
        evt_str = f"| >> {p['evt']} <<" if p.get("evt") else ""

        trkw = p.get("trk_w", 0)
        crgw = f"{p.get('crg_w', 0)}%" if p.get("trl_on", 0) else "--"
        fuel_l = p.get("fuel_l", 0)
        fuel_str = f"{fuel}% ({fuel_l}L)" if fuel_l > 0 else f"{fuel}%"
        eta_d = p.get("eta_d", 0)
        eta_str = f"| ETA: {eta_d}km " if eta_d > 0 else ""

        try:
            sys.stdout.write(
                f"\r| {bl_l} {gear_str:3s} {bl_r} | SPD: {spd:3d} km/h {lim_str} {warn_str} | RPM: {rpm:4d} | FUEL: {fuel_str} | TRK: {trkw}% | CRG: {crgw} {eta_str}| {ret_str} {wip_str} {bcn_str} | TX: {self.current_fps:4.1f} FPS {evt_str} "
            )
            sys.stdout.flush()
        except Exception:
            pass

    def run(self):
        self.running = True
        print("\n" + "=" * 70)
        print("  MIU EILIK - EURO TRUCK SIMULATOR 2 DASHBOARD COMPANION")
        print(f"  Port: {self.port_name or 'Auto-Detect'} | Rate: {int(1.0 / self.target_interval)} FPS")
        if self.demo:
            print("  Che do: MO PHONG XE CHAY (Demo Mode)")
        else:
            print(f"  Telemetry: Windows Shared Memory (Direct) + Fallback REST ({self.server_url})")
        print("  Nhan Ctrl+C de dung an toan")
        print("=" * 70 + "\n")

        try:
            while self.running:
                loop_start = time.time()
                self.check_mcp_commands()

                if not self.ensure_serial():
                    self.dump_state(None)
                    sys.stdout.write(
                        "\r[DANG DOI ROBOT] Dang doi ket noi robot ESP32 qua USB... | Windows Shared Memory   "
                    )
                    sys.stdout.flush()
                    time.sleep(1.0)
                    continue

                if self.demo:
                    payload = self.generate_demo_telemetry()
                    self.send_telemetry_packet(payload)
                    self.dump_state(payload)
                    self.render_console_dashboard(payload)
                else:
                    payload = self.fetch_live_telemetry()
                    game_connected = False
                    if payload is not None:
                        game_connected = bool(payload.get("conn", 0) == 1)

                    if game_connected and payload is not None:
                        # Vao duoc man hinh chinh cua game
                        if not self.game_connected_prev:
                            print(f"\n[GAME CONNECTED] ETS2 da vao man hinh chinh! ({self.source_name}) -> Chuyen man hinh Cockpit...")
                            self.game_connected_prev = True
                            self.send_command("MODE=GAME")
                            time.sleep(0.05)
                        self.process_events(payload)
                        self.send_telemetry_packet(payload)
                        self.dump_state(payload)
                        self.render_console_dashboard(payload)
                    else:
                        # Game chua bat hoac chua ket noi
                        self.dump_state(None)
                        if self.game_connected_prev:
                            print("\n[GAME DISCONNECTED] Game da thoat -> Chuyen robot ve mat RoboEyes")
                            self.send_command("MODE=EYES")
                            self.game_connected_prev = False

                        sys.stdout.write(
                            "\r[DANG CHO GAME] Dang doi ETS2 khoi dong (Shared Memory)... | Robot van giu man hinh Mat RoboEyes   "
                        )
                        sys.stdout.flush()
                        time.sleep(0.3)
                        continue

                # Duy tri tan so truyen mong muon
                elapsed = time.time() - loop_start
                sleep_time = self.target_interval - elapsed
                if sleep_time > 0:
                    time.sleep(sleep_time)

        except KeyboardInterrupt:
            print("\n[STOP] Nguoi dung yeu cau dung chuong trinh...")
        except Exception as e:
            import traceback
            print(f"\n[UNHANDLED ERROR] Loi khong mong muon trong run(): {e}")
            traceback.print_exc()
        finally:
            self.running = False
            self.close_serial()
            try:
                state_file = os.path.join(os.path.dirname(__file__), "telemetry_state.json")
                with open(state_file, "w", encoding="utf-8") as f:
                    json.dump({"daemon_running": False, "timestamp": time.time()}, f)
            except Exception:
                pass


def main():
    parser = argparse.ArgumentParser(description="Miu Eilik ETS2 Dashboard Companion")
    parser.add_argument("--port", "-p", default=None, help="Cong COM (vi du: COM4, COM5)")
    parser.add_argument("--baud", "-b", type=int, default=DEFAULT_BAUD, help="Baud rate (mac dinh: 115200)")
    parser.add_argument("--server", "-s", default=None, help="URL Telemetry Server (mac dinh: 25555 cho ETS2)")
    parser.add_argument("--fps", "-f", type=int, default=30, help="Toc do cap nhat FPS (mac dinh: 30)")
    parser.add_argument("--demo", "-d", action="store_true", help="Che do mo phong demo khong can bat game")
    parser.add_argument("--test-file", help="Duong dan den file Ets2TestTelemetry.json de test")

    args = parser.parse_args()

    port = args.port
    if not port:
        port = find_default_port()
        if port:
            print(f"[AUTO-DETECT] Tim thay cong: {port}")
        else:
            print("[AUTO-DETECT] Chua tim thay cong COM, he thong se tu dong cho robot ket noi...")

    companion = Ets2DesktopCompanion(
        port=port,
        baud=args.baud,
        server_url=args.server,
        fps=args.fps,
        demo=args.demo,
        test_file=args.test_file,
    )
    companion.run()


if __name__ == "__main__":
    main()
