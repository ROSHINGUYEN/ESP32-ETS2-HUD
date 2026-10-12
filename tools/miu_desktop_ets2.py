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


DEFAULT_BAUD = 460800
DEFAULT_ETS2_URL = "http://127.0.0.1:25555/api/ets2/telemetry"
DEFAULT_TEST_JSON = os.path.join(
    os.path.dirname(__file__), "mock_telemetry.json"
)


def decode_c_string(raw_bytes: bytes, offset: int, max_len: int = 64) -> str:
    """Giai ma chuoi C-style null-terminated tu bo nho Shared Memory an toan."""
    if offset >= len(raw_bytes):
        return ""
    chunk = raw_bytes[offset:offset + max_len]
    null_idx = chunk.find(b'\x00')
    if null_idx != -1:
        chunk = chunk[:null_idx]
    try:
        return chunk.decode('utf-8', errors='ignore').strip()
    except Exception:
        return ""


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
        # File mapping name do ets2-telemetry-server.dll tao trong game (map 4096 de du het struct fields)
        self.hMap = self.k32.OpenFileMappingW(0x0004, False, "Local\\Ets2TelemetryServer")
        if self.hMap:
            self.pBuf = self.k32.MapViewOfFile(self.hMap, 0x0004, 0, 0, 4096)
            if self.pBuf:
                self.ets2_pid = self._find_ets2_pid()
                self._ensure_scs_telemetry()
                return True
            self.k32.CloseHandle(self.hMap)
            self.hMap = None

        # Neu chua mo duoc SHM (game dang khoi dong trong doan intro), kiem tra tien trinh eurotrucks2
        if _PSUTIL_OK:
            if self.ets2_pid and psutil.pid_exists(self.ets2_pid):
                return True
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

            # Tinh toan tien do loading % muot ma khi game dang o intro / loading
            if getattr(self, "loading_start_time", 0.0) == 0.0:
                self.loading_start_time = time.time()
            elapsed_load = time.time() - self.loading_start_time
            cur_load_pct = min(98, max(15, int(15 + elapsed_load * 12.0)))

            # Neu chua co pBuf (game dang trong doan intro khoi dong chua kip nap plugin):
            if not self.pBuf:
                # Thu ket noi lai SHM neu plugin vua tao xong
                self.hMap = self.k32.OpenFileMappingW(0x0004, False, "Local\\Ets2TelemetryServer")
                if self.hMap:
                    self.pBuf = self.k32.MapViewOfFile(self.hMap, 0x0004, 0, 4096)
                if not self.pBuf:
                    # Tra ve goi connected + loading kem load_pct tang deu de ESP32 hien thi Loading Splash Scania!
                    return {
                        "game": "ets2",
                        "spd": 0, "lim": 0, "cc": 0, "cc_on": 0,
                        "gear": 0, "rpm": 0, "rpm_max": 2500,
                        "fuel": 100, "fuel_l": 0, "fuel_cap": 0, "fuel_warn": 0, "fuel_r": 0.0,
                        "bl_l": 0, "bl_r": 0, "low": 0, "high": 0, "park": 1,
                        "air": 0, "ret": 0, "wip": 0, "bcn": 0, "mbrk": 0,
                        "brk_l": 0, "rev_l": 0, "diff_l": 0, "lift_a": 0,
                        "oil_t": 0.0, "water_t": 0.0, "air_p": 0.0, "brake_t": 0.0, "crg_wgt": 0.0,
                        "wear": 0.0, "trk_w": 0, "crg_w": -1, "trl_on": 0,
                        "odo": 0, "eta_d": 0, "eta_t": 0, "gh": -1, "gm": -1,
                        "conn": 1, "pause": 1, "load": 1, "load_pct": cur_load_pct,
                        "route": "", "city_src": "", "city_dst": "", "comp_src": "", "comp_dst": "", "cargo": "",
                        "income": 0, "truck_make": "", "truck_model": "",
                        "coords": [0.0, 0.0, 0.0], "accel": [0.0, 0.0, 0.0],
                        "damage": {"engine": 0.0, "trans": 0.0, "cabin": 0.0, "chassis": 0.0, "wheels": 0.0, "trailer": 0.0},
                        "raw_wear": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
                    }

            raw = ctypes.string_at(self.pBuf, 2048)

            # Offset 8: pluginVersion (phai >= 1 neu DLL telemetry da duoc nap trong game)
            plugin_ver = struct.unpack_from("<I", raw, 8)[0]
            if plugin_ver < 1:
                return {
                    "game": "ets2",
                    "spd": 0, "lim": 0, "cc": 0, "cc_on": 0,
                    "gear": 0, "rpm": 0, "rpm_max": 2500,
                    "fuel": 100, "fuel_l": 0, "fuel_cap": 0, "fuel_warn": 0, "fuel_r": 0.0,
                    "bl_l": 0, "bl_r": 0, "low": 0, "high": 0, "park": 1,
                    "air": 0, "ret": 0, "wip": 0, "bcn": 0, "mbrk": 0,
                    "brk_l": 0, "rev_l": 0, "diff_l": 0, "lift_a": 0,
                    "oil_t": 0.0, "water_t": 0.0, "air_p": 0.0, "brake_t": 0.0, "crg_wgt": 0.0,
                    "wear": 0.0, "trk_w": 0, "crg_w": -1, "trl_on": 0,
                    "odo": 0, "eta_d": 0, "eta_t": 0, "gh": -1, "gm": -1,
                    "conn": 1, "pause": 1, "load": 1, "load_pct": cur_load_pct,
                    "route": "", "city_src": "", "city_dst": "", "comp_src": "", "comp_dst": "", "cargo": "",
                    "income": 0, "truck_make": "", "truck_model": "",
                    "coords": [0.0, 0.0, 0.0], "accel": [0.0, 0.0, 0.0],
                    "damage": {"engine": 0.0, "trans": 0.0, "cabin": 0.0, "chassis": 0.0, "wheels": 0.0, "trailer": 0.0},
                    "raw_wear": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
                }

            # Offset 4: paused flag
            paused = struct.unpack_from("<I", raw, 4)[0]

            # Offset 160: timeAbsolute (minutes elapsed in game world, non-zero = in-game)
            time_absolute = struct.unpack_from("<i", raw, 160)[0]

            speed_ms = struct.unpack_from("<f", raw, 24)[0]
            gear = struct.unpack_from("<i", raw, 64)[0]
            rpm, rpmMax, fuel, fuelCap = struct.unpack_from("<4f", raw, 80)
            electric_on = raw[584] if len(raw) > 584 else 0
            engine_on = raw[585] if len(raw) > 585 else 0

            # Kiem tra xem nguoi choi da thuc su vao buong lai lai xe hay chua:
            # - Voi Steam Singleplayer: time_absolute > 0
            # - Voi TruckersMP (Multiplayer): time_absolute co the = 0 vi server dong bo gio rieng,
            #   nhung xe da vao buong lai khi: engine_on=1, electric_on=1, rpm > 100, gear != 0, hoac abs(speed_ms) > 0.05
            is_driving = (
                time_absolute > 0 or
                engine_on != 0 or
                electric_on != 0 or
                rpm > 100.0 or
                abs(speed_ms) > 0.05 or
                gear != 0
            )

            # Neu game dang o man hinh menu / login TruckersMP / chon server / loading map:
            if not is_driving:
                return {
                    "game": "ets2",
                    "spd": 0, "lim": 0, "cc": 0, "cc_on": 0,
                    "gear": 0, "rpm": 0, "rpm_max": 2500,
                    "fuel": 100, "fuel_l": 0, "fuel_cap": 0, "fuel_warn": 0, "fuel_r": 0.0,
                    "bl_l": 0, "bl_r": 0, "low": 0, "high": 0, "park": 1,
                    "air": 0, "ret": 0, "wip": 0, "bcn": 0, "mbrk": 0,
                    "brk_l": 0, "rev_l": 0, "diff_l": 0, "lift_a": 0,
                    "oil_t": 0.0, "water_t": 0.0, "air_p": 0.0, "brake_t": 0.0, "crg_wgt": 0.0,
                    "wear": 0.0, "trk_w": 0, "crg_w": -1, "trl_on": 0,
                    "odo": 0, "eta_d": 0, "eta_t": 0, "gh": -1, "gm": -1,
                    "conn": 1, "pause": 1, "load": 1, "load_pct": cur_load_pct,
                    "route": "", "city_src": "", "city_dst": "", "comp_src": "", "comp_dst": "", "cargo": "",
                    "income": 0, "truck_make": "", "truck_model": "",
                    "coords": [0.0, 0.0, 0.0], "accel": [0.0, 0.0, 0.0],
                    "damage": {"engine": 0.0, "trans": 0.0, "cabin": 0.0, "chassis": 0.0, "wheels": 0.0, "trailer": 0.0},
                    "raw_wear": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
                }

            # Khi da vao buong lai lai xe: Reset loading_start_time
            self.loading_start_time = 0.0

            # Decode gio cabin:
            if time_absolute > 0:
                game_h = (time_absolute // 60) % 24
                game_m = time_absolute % 60
            else:
                # TruckersMP (Multiplayer dong bo gio server): fallback gio thuc he thong Windows
                local_now = time.localtime()
                game_h = local_now.tm_hour
                game_m = local_now.tm_min

            ax, ay, az = struct.unpack_from("<3f", raw, 28)
            cx, cy, cz = struct.unpack_from("<3f", raw, 40)
            fuel_rate = struct.unpack_from("<f", raw, 96)[0] * 100.0 # L/100km
            retarder = struct.unpack_from("<i", raw, 564)[0]
            cc_on = raw[580]
            wipers = raw[581]
            park = raw[582]
            mbrk = raw[583]
            bl_l = raw[586]
            bl_r = raw[587]
            parking_light = raw[590]
            low = raw[591]
            high = raw[592]
            brk_l = 1 if raw[597] else 0
            rev_l = 1 if raw[598] else 0
            beacon = raw[604]
            air = raw[608]
            fuel_warn = struct.unpack_from("<i", raw, 624)[0]

            # Kiem tra cua so game active de bat phim ken (H/N) va cap do gat mua (P)
            is_ets2_fg = False
            try:
                hwnd = ctypes.windll.user32.GetForegroundWindow()
                fg_pid = wintypes.DWORD()
                ctypes.windll.user32.GetWindowThreadProcessId(hwnd, ctypes.byref(fg_pid))
                is_ets2_fg = bool(self.ets2_pid and fg_pid.value == self.ets2_pid)
            except Exception:
                pass

            # Theo doi cap do gat mua (1..3) qua phim P (0x50) & dong bo voi wipers SHM
            wip_on = 1 if wipers else 0
            if wip_on == 0:
                self.wiper_lvl = 0
                self.prev_p_down = False
            else:
                if getattr(self, "wiper_lvl", 0) <= 0:
                    self.wiper_lvl = 1
                try:
                    p_down = bool(ctypes.windll.user32.GetAsyncKeyState(0x50) & 0x8000)
                    if p_down and not getattr(self, "prev_p_down", False):
                        # P vua duoc bam xuong (Rising Edge) -> chuyen nac: 1 -> 2 -> 3 -> 1
                        self.wiper_lvl = (self.wiper_lvl % 3) + 1
                    self.prev_p_down = p_down
                except Exception:
                    pass
            wiper_lvl = self.wiper_lvl if wip_on else 0

            # Kiem tra bam ken thuong (H: 0x48) hoac coi hoi (N: 0x4E) kem giu coi (horn latch)
            h_pressed = False
            try:
                if (ctypes.windll.user32.GetAsyncKeyState(0x48) & 0x8000) or (ctypes.windll.user32.GetAsyncKeyState(0x4E) & 0x8000):
                    h_pressed = True
            except Exception:
                pass

            now_t = time.time()
            if h_pressed:
                self.horn_until = now_t + 0.35 # Giu it nhat 350ms de dam bao Serial truyen va OLED render sang ro net
            horn_on = 1 if (getattr(self, "horn_until", 0) > now_t) else 0

            # Thong so chi tiet ap suat, nhiet do tu SCS SDK Telemetry
            air_psi = struct.unpack_from("<f", raw, 616)[0]
            air_bar = round(air_psi * 0.0689476, 1) if air_psi > 0 else 0.0
            brake_temp = round(struct.unpack_from("<f", raw, 620)[0], 1)
            oil_temp = round(struct.unpack_from("<f", raw, 636)[0], 1)
            water_temp = round(struct.unpack_from("<f", raw, 640)[0], 1)

            trailer_on = 1 if raw[21] != 0 else 0
            wears = struct.unpack_from("<6f", raw, 656)
            w_eng, w_tra, w_cab, w_cha, w_whl, w_trl = wears

            pct_eng = round(w_eng * 100.0, 2)
            pct_tra = round(w_tra * 100.0, 2)
            pct_cab = round(w_cab * 100.0, 2)
            pct_cha = round(w_cha * 100.0, 2)
            pct_whl = round(w_whl * 100.0, 2)
            pct_trl = round(w_trl * 100.0, 2)

            # Game ETS2 Route Advisor (F7) hien thi do hu hai dau keo theo BO PHAN HONG NANG NHAT (Max Component Wear)
            max_truck_wear = max(w_eng, w_tra, w_cab, w_cha, w_whl)
            truck_wear_pct = int(max_truck_wear * 100.0) # Dung int(floor) dong bo 100% voi game ETS2 hien thi
            trailer_wear_pct = int(w_trl * 100.0) if trailer_on else -1

            # Doc chinh xac do hu hai hang hoa (cargoDamage) tu SCS SDK Shared Memory (offset 1468)
            cargo_damage_pct = -1
            if self.pBufScs and trailer_on:
                try:
                    raw_scs = ctypes.string_at(self.pBufScs, 2048)
                    scs_cargo_dmg = struct.unpack_from("<f", raw_scs, 1468)[0]
                    if 0.0 <= scs_cargo_dmg <= 1.0:
                        cargo_damage_pct = int(scs_cargo_dmg * 100.0)
                except Exception:
                    pass
            if cargo_damage_pct < 0 and trailer_on:
                cargo_damage_pct = trailer_wear_pct
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
            nav_t = struct.unpack_from("<f", raw, 1004)[0]
            nav_lim = struct.unpack_from("<f", raw, 1008)[0] * 3.6
            spd_kmh = int(round(abs(speed_ms) * 3.6))
            eta_d_km = int(round(nav_d)) if (0 < nav_d < 50000.0) else 0
            eta_t_min = int(round(nav_t / 60.0)) if nav_t > 0 else 0

            # Trich xuat thong tin ban do, cong ty, hang hoa va lo trinh
            cargo = decode_c_string(raw, 236, 64)
            cargo_weight_t = 0.0
            try:
                cargo_mass_kg = struct.unpack_from("<f", raw, 232)[0]
                if 0 < cargo_mass_kg < 100000.0:
                    cargo_weight_t = round(cargo_mass_kg / 1000.0, 1)
            except Exception:
                pass

            job_income = struct.unpack_from("<i", raw, 300)[0]
            city_src = decode_c_string(raw, 308, 64)
            city_dst = decode_c_string(raw, 372, 64)
            comp_src = decode_c_string(raw, 436, 64)
            comp_dst = decode_c_string(raw, 500, 64)
            truck_make = decode_c_string(raw, 688, 64)
            truck_model = decode_c_string(raw, 816, 64)

            route_str = ""
            if city_src and city_dst:
                route_str = f"{city_src} -> {city_dst}"
            elif city_dst:
                route_str = f"Den: {city_dst}"

            # Xac dinh cap so hien thi (D1..D16, C1..C2, N, R1..R2) dong bo 100% voi taplo trong game:
            diff_lock = 0
            lift_axle = 0
            raw_gear = 0

            # Uu tien 1: Doc tu Funbit displayedGear hoac gear
            if disp_gear != 0:
                raw_gear = disp_gear
            elif gear != 0:
                raw_gear = gear

            # Uu tien 2: Neu ca 2 deu = 0, kiem tra SCSTelemetry
            if raw_gear == 0 and self.pBufScs:
                try:
                    raw_scs = ctypes.string_at(self.pBufScs, 2048)
                    scs_dash_gear = struct.unpack_from("<i", raw_scs, 508)[0]
                    scs_mech_gear = struct.unpack_from("<i", raw_scs, 504)[0]
                    if scs_dash_gear != 0:
                        raw_gear = scs_dash_gear
                    elif scs_mech_gear != 0:
                        raw_gear = scs_mech_gear
                except Exception:
                    pass

            # Thuat toan bu tru Crawler Gears cho hop so 14/16 cap (Scania, Volvo...):
            crawlers = max(0, fwd_gears - 12) if fwd_gears > 12 else 0
            if raw_gear > 0:
                if crawlers > 0 and raw_gear <= crawlers:
                    gear_type = "C"
                    gear_val = raw_gear
                    gear_str = f"C{raw_gear}"
                else:
                    d_num = raw_gear - crawlers if crawlers > 0 else raw_gear
                    gear_type = "D"
                    gear_val = d_num
                    gear_str = f"D{d_num}"
            elif raw_gear < 0:
                r_num = abs(raw_gear)
                gear_type = "R"
                gear_val = -r_num
                gear_str = f"R{r_num}" if r_num > 1 else "R"
            else:
                gear_type = "N"
                gear_val = 0
                gear_str = "N"

            return {
                "game": "ets2",
                "spd": spd_kmh,
                "lim": int(round(nav_lim)) if nav_lim > 0 else 0,
                "cc": int(round(cc_spd)) if cc_spd > 0 else 0,
                "cc_on": 1 if cc_on else 0,
                "gear": int(gear_val),
                "g_str": gear_str,
                "g_type": gear_type,
                "rpm": int(round(rpm)),
                "rpm_max": int(round(rpmMax if rpmMax > 500 else 2500)),
                "fuel": fuel_pct,
                "fuel_l": fuel_l,
                "fuel_cap": fuel_cap,
                "fuel_warn": 1 if (fuel_warn > 0 or fuel_pct < 15) else 0,
                "fuel_r": round(fuel_rate, 1) if (0 <= fuel_rate < 200.0) else 0.0,
                "bl_l": 1 if bl_l else 0,
                "bl_r": 1 if bl_r else 0,
                "low": 1 if low else 0,
                "high": 1 if high else 0,
                "park_l": 1 if parking_light else 0,
                "eng": 1 if engine_on else 0,
                "horn": horn_on,
                "park": 1 if park else 0,
                "air": 1 if air else 0,
                "ret": int(retarder),
                "wip": 1 if wipers else 0,
                "wip_lvl": wiper_lvl,
                "bcn": 1 if beacon else 0,
                "mbrk": 1 if mbrk else 0,
                "brk_l": brk_l,
                "rev_l": rev_l,
                "diff_l": diff_lock,
                "lift_a": lift_axle,
                "air_p": air_bar,
                "oil_t": oil_temp,
                "water_t": water_temp,
                "brake_t": brake_temp,
                "crg_wgt": cargo_weight_t,
                "wear": total_wear,
                "trk_w": truck_wear_pct,
                "trl_w": trailer_wear_pct,
                "crg_w": cargo_damage_pct,
                "trl_on": trailer_on,
                "odo": odo_km,
                "eta_d": eta_d_km,
                "eta_t": eta_t_min,
                "gh": game_h,
                "gm": game_m,
                "conn": 1,
                "load": 0,
                "load_pct": 100,
                "pause": 1 if (paused != 0) else 0,
                # Thong tin ban do, lo trinh & dieu huong GPS
                "route": route_str,
                "city_src": city_src,
                "city_dst": city_dst,
                "comp_src": comp_src,
                "comp_dst": comp_dst,
                "cargo": cargo,
                "income": job_income,
                "truck_make": truck_make,
                "truck_model": truck_model,
                "coords": [cx, cy, cz],
                "accel": [ax, ay, az],
                # Chi tiet hu hai 6 bo phan xe & ro-mooc
                "damage": {
                    "engine": pct_eng,
                    "trans": pct_tra,
                    "cabin": pct_cab,
                    "chassis": pct_cha,
                    "wheels": pct_whl,
                    "trailer": pct_trl,
                },
                "raw_wear": list(wears),
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


class CollisionDetector:
    """He thong phat hien, phan tich hu hai tung linh kien va ghi log va cham chinh xac 100% tu API goc ETS2."""

    COMPONENT_NAMES = {
        0: ("Động cơ", "Engine"),
        1: ("Hộp số", "Transmission"),
        2: ("Cabin", "Cabin"),
        3: ("Khung gầm", "Chassis"),
        4: ("Bánh xe", "Wheels"),
        5: ("Rơ-moóc / Hàng hóa", "Trailer/Cargo"),
    }

    def __init__(self, log_path: Optional[str] = None):
        self.log_path = log_path or os.path.join(os.path.dirname(__file__), "collision_history.log")
        self.prev_wears: Optional[list] = None
        self.prev_speed = 0
        self.last_collision_time = 0.0
        self.total_collisions = 0
        self.last_collision_event: Optional[Dict[str, Any]] = None

    def check(self, payload: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        raw_wears = payload.get("raw_wear")
        if not raw_wears or len(raw_wears) < 6:
            return None

        speed = payload.get("spd", 0)
        now = time.time()

        if self.prev_wears is None:
            self.prev_wears = list(raw_wears)
            self.prev_speed = speed
            return None

        # Tinh bien thien (delta) tung thanh phan
        deltas = [raw_wears[i] - self.prev_wears[i] for i in range(6)]

        # Truong hop xe vao gara sua (wear giam xuong)
        if any(d < -0.01 for d in deltas):
            self.prev_wears = list(raw_wears)
            self.prev_speed = speed
            return None

        # Chi tinh bien thien duong (hu hong tang them)
        pos_deltas = [max(0.0, d) for d in deltas]
        truck_jump = sum(pos_deltas[:5])
        trailer_jump = pos_deltas[5]
        total_jump = truck_jump + trailer_jump

        # Nguong phat hien va cham:
        # Khi xe chay binh thuong, wear tang cuc ky cham (< 0.00005/frame).
        # Khi xay ra va cham, it nhat mot bo phan tang >= 0.1% (0.001) hoac tong jump >= 0.15% (0.0015).
        is_collision = (any(d >= 0.001 for d in pos_deltas) or total_jump >= 0.0015)

        if not is_collision:
            self.prev_wears = list(raw_wears)
            self.prev_speed = speed
            return None

        # Tranh ghi log trung lap neu cung mot cu dam keo dai qua 2 khung hinh lien tiep (cooldown 0.6s)
        if (now - self.last_collision_time < 0.6) and (total_jump < 0.005):
            self.prev_wears = list(raw_wears)
            self.prev_speed = speed
            return None

        self.last_collision_time = now
        self.total_collisions += 1

        # Tinh toc do sut giam va luc chan dong G-force
        speed_at_impact = max(speed, self.prev_speed)
        speed_drop = max(0, self.prev_speed - speed)
        ax, ay, az = payload.get("accel", [0.0, 0.0, 0.0])
        accel_mag = math.sqrt(ax * ax + ay * ay + az * az)
        g_force = round(accel_mag / 9.80665, 1) if accel_mag > 0 else 0.0

        # Phan loai muc do nghiem trong
        if total_jump >= 0.08:
            severity = "TAI NAN RAT NANG"
            severity_en = "CATASTROPHIC"
        elif total_jump >= 0.03:
            severity = "DAM MANH"
            severity_en = "SEVERE"
        elif total_jump >= 0.008:
            severity = "VA CHAM VUA"
            severity_en = "MODERATE"
        else:
            severity = "QUET NHE"
            severity_en = "MINOR"

        # Danh sach cac linh kien bi ton hai
        damaged_components = []
        for i, d in enumerate(pos_deltas):
            if d >= 0.0005:  # Tang >= 0.05%
                vn_name, en_name = self.COMPONENT_NAMES.get(i, ("Khác", "Other"))
                damaged_components.append({
                    "component": vn_name,
                    "component_en": en_name,
                    "delta_pct": round(d * 100.0, 2),
                    "current_pct": round(raw_wears[i] * 100.0, 2),
                })

        # Thong tin ban do va lo trinh tai thoi diem va cham
        route = payload.get("route", "")
        city_dst = payload.get("city_dst", "")
        eta_d = payload.get("eta_d", 0)
        coords = payload.get("coords", [0.0, 0.0, 0.0])

        loc_str = route if route else (city_dst if city_dst else "Khong xac dinh")
        if eta_d > 0:
            loc_str += f" (Con {eta_d} km)"

        record = {
            "timestamp": now,
            "time_str": time.strftime("%Y-%m-%d %H:%M:%S"),
            "game_time": f"{payload.get('gh', 0):02d}:{payload.get('gm', 0):02d}",
            "severity": severity,
            "severity_en": severity_en,
            "total_damage_increase_pct": round(total_jump * 100.0, 2),
            "speed_kmh": speed_at_impact,
            "speed_drop_kmh": speed_drop,
            "g_force": g_force,
            "route": route,
            "location": loc_str,
            "coordinates": {"x": round(coords[0], 1), "y": round(coords[1], 1), "z": round(coords[2], 1)},
            "damaged_components": damaged_components,
            "current_truck_wear_pct": payload.get("trk_w", 0),
            "current_cargo_wear_pct": payload.get("crg_w", -1),
        }

        self.last_collision_event = record
        self._write_collision_log(record)

        self.prev_wears = list(raw_wears)
        self.prev_speed = speed
        return record

    def _write_collision_log(self, rec: Dict[str, Any]):
        """Ghi nhat ky va cham chi tiet ra file collision_history.log."""
        try:
            with open(self.log_path, "a", encoding="utf-8") as f:
                f.write("\n" + "=" * 78 + "\n")
                f.write(f"[{rec['time_str']}] VA CHAM: {rec['severity']} (+{rec['total_damage_increase_pct']}%) | Game Time: {rec['game_time']}\n")
                f.write(f"  Vận tốc lúc đâm: {rec['speed_kmh']} km/h (Sụt giảm: {rec['speed_drop_kmh']} km/h) | Lực G: {rec['g_force']}G\n")
                f.write(f"  Vị trí bản đồ:   {rec['location']} | Toạ độ: X={rec['coordinates']['x']}, Y={rec['coordinates']['y']}, Z={rec['coordinates']['z']}\n")
                f.write("  Chi tiết linh kiện bị hư hại phát sinh:\n")
                for comp in rec['damaged_components']:
                    f.write(f"    - {comp['component']:18s}: +{comp['delta_pct']:4.2f}%  (Tổng hiện tại: {comp['current_pct']:4.2f}%)\n")
                f.write(f"  Tổng hư hại xe hiện tại: {rec['current_truck_wear_pct']}% | Rơ-moóc: {rec['current_cargo_wear_pct']}%\n")
                f.write("=" * 78 + "\n")
        except Exception as e:
            print(f"[COLLISION LOG ERR] {e}")

        # In thong bao noi bat tren console
        print(f"\n\n{'!'*20} [CANH BAO VA CHAM / COLLISION] {'!'*20}")
        print(f"  Muc do: {rec['severity']} (+{rec['total_damage_increase_pct']}%) | Van toc: {rec['speed_kmh']} km/h | Luc G: {rec['g_force']}G")
        print(f"  Lo trinh: {rec['location']}")
        for comp in rec['damaged_components']:
            print(f"    + {comp['component']}: +{comp['delta_pct']}% (Hien tai: {comp['current_pct']}%)")
        print(f"{'!'*65}\n")


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
        self.collision_detector = CollisionDetector()

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

        # 1. Va cham tai nan (Collision / Crash voi CollisionDetector chinh xac 100%)
        col = self.collision_detector.check(p)
        if col:
            new_event = f"VA CHAM: +{col['total_damage_increase_pct']}%"
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
            p = payload or {}
            data = {
                "daemon_running": True,
                "pid": os.getpid(),
                "port": self.port_name,
                "baud": self.baud,
                "serial_connected": bool(self.ser and self.ser.is_open),
                "game": p.get("game", "ets2"),
                "game_connected": bool(self.game_connected_prev),
                "source": self.source_name,
                "fps": round(self.current_fps, 1),
                "packets_sent": self.packets_sent,
                "active_event": self.active_event_text if now < self.event_expire_time else "",
                "timestamp": now,
                "map": {
                    "route": p.get("route", ""),
                    "source_city": p.get("city_src", ""),
                    "dest_city": p.get("city_dst", ""),
                    "source_company": p.get("comp_src", ""),
                    "dest_company": p.get("comp_dst", ""),
                    "cargo": p.get("cargo", ""),
                    "income": p.get("income", 0),
                    "eta_km": p.get("eta_d", 0),
                    "eta_min": p.get("eta_t", 0),
                    "truck_make": p.get("truck_make", ""),
                    "truck_model": p.get("truck_model", ""),
                    "coordinates": p.get("coords", [0.0, 0.0, 0.0]),
                },
                "damage": p.get("damage", {}),
                "collision_count": self.collision_detector.total_collisions,
                "last_collision": self.collision_detector.last_collision_event,
                "telemetry": p,
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
        trailer_wear_pct = int(trailer.get("wear", 0.0) * 100.0) if trailer_on else -1
        cargo_wear_pct = int(trailer.get("cargoDamage", trailer.get("wear", 0.0)) * 100.0) if trailer_on else -1

        # Truck wear (Route Advisor F7: max component wear)
        truck_wear_max = max(
            truck.get("wearEngine", 0.0),
            truck.get("wearTransmission", 0.0),
            truck.get("wearCabin", 0.0),
            truck.get("wearChassis", 0.0),
            truck.get("wearWheels", 0.0)
        )
        truck_wear_pct = int(truck_wear_max * 100.0)
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

        job = raw.get("job", {})
        city_src = job.get("sourceCity", "")
        city_dst = job.get("destinationCity", "")
        comp_src = job.get("sourceCompany", "")
        comp_dst = job.get("destinationCompany", "")
        job_income = job.get("income", 0)
        cargo = trailer.get("name", "")

        route_str = ""
        if city_src and city_dst:
            route_str = f"{city_src} -> {city_dst}"
        elif city_dst:
            route_str = f"Den: {city_dst}"

        w_eng = float(truck.get("wearEngine", 0.0))
        w_tra = float(truck.get("wearTransmission", 0.0))
        w_cab = float(truck.get("wearCabin", 0.0))
        w_cha = float(truck.get("wearChassis", 0.0))
        w_whl = float(truck.get("wearWheels", 0.0))
        w_trl = float(trailer.get("wear", 0.0))

        pct_eng = round(w_eng * 100.0, 2)
        pct_tra = round(w_tra * 100.0, 2)
        pct_cab = round(w_cab * 100.0, 2)
        pct_cha = round(w_cha * 100.0, 2)
        pct_whl = round(w_whl * 100.0, 2)
        pct_trl = round(w_trl * 100.0, 2)

        plc = truck.get("placement", {})
        acc = truck.get("acceleration", {})
        cx = float(plc.get("x", 0.0))
        cy = float(plc.get("y", 0.0))
        cz = float(plc.get("z", 0.0))
        ax = float(acc.get("x", 0.0))
        ay = float(acc.get("y", 0.0))
        az = float(acc.get("z", 0.0))

        # ETA time
        eta_t = 0
        et_raw = nav.get("estimatedTime")
        if isinstance(et_raw, str) and "T" in et_raw:
            try:
                t_str = et_raw.split("T")[1][:8]
                h_p, m_p, _ = map(int, t_str.split(":"))
                eta_t = h_p * 60 + m_p
            except Exception:
                pass
        elif isinstance(et_raw, (int, float)):
            eta_t = int(round(et_raw / 60.0))

        payload = {
            "game": "ets2",
            "spd": int(round(abs(truck.get("speed", 0.0)))),
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
            "park_l": 1 if truck.get("lightsParkingOn", False) else 0,
            "eng": 1 if truck.get("engineOn", True) else 0,
            "horn": 1 if truck.get("hornOn", False) else 0,
            "park": 1 if truck.get("parkBrakeOn", False) else 0,
            "air": 1 if truck.get("airPressureWarningOn", False) else 0,
            "ret": int(truck.get("retarderBrake", 0)),
            "wip": 1 if truck.get("wipersOn", False) else 0,
            "wip_lvl": int(truck.get("wipersMode", 1 if truck.get("wipersOn", False) else 0)),
            "bcn": 1 if (truck.get("lightsBeaconOn", False) or truck.get("beaconOn", False)) else 0,
            "mbrk": 1 if truck.get("motorBrakeOn", False) else 0,
            "brk_l": 1 if truck.get("lightsBrakeOn", False) else 0,
            "rev_l": 1 if truck.get("lightsReverseOn", False) else 0,
            "diff_l": 1 if truck.get("differentialLock", False) else 0,
            "lift_a": 1 if (truck.get("liftAxle", False) or trailer.get("liftAxle", False)) else 0,
            "air_p": round(truck.get("airPressure", 0.0), 1),
            "oil_t": round(truck.get("oilTemperature", 0.0), 1),
            "water_t": round(truck.get("waterTemperature", 0.0), 1),
            "brake_t": round(truck.get("brakeTemperature", 0.0), 1),
            "fuel_r": round(truck.get("fuelRate", 0.0) * 100.0, 1) if truck.get("fuelRate", 0.0) > 0 else 0.0,
            "crg_wgt": round(trailer.get("mass", 0.0) / 1000.0, 1) if trailer.get("mass", 0.0) > 0 else 0.0,
            "wear": float(w_eng + w_tra + w_cab + w_cha + w_whl + w_trl),
            "trk_w": truck_wear_pct,
            "trl_w": trailer_wear_pct,
            "crg_w": cargo_wear_pct,
            "trl_on": trailer_on,
            "odo": odo_km,
            "eta_d": eta_d,
            "eta_t": eta_t,
            "gh": gh,
            "gm": gm,
            "conn": 1 if game.get("connected", False) else 0,
            "load": 0 if game.get("connected", False) else 1,
            "load_pct": 100 if game.get("connected", False) else 50,
            "pause": 1 if game.get("paused", False) else 0,
            # Ban do & Lo trinh
            "route": route_str,
            "city_src": city_src,
            "city_dst": city_dst,
            "comp_src": comp_src,
            "comp_dst": comp_dst,
            "cargo": cargo,
            "income": job_income,
            "truck_make": truck.get("make", ""),
            "truck_model": truck.get("model", ""),
            "coords": [cx, cy, cz],
            "accel": [ax, ay, az],
            # Chi tiet hu hai
            "damage": {
                "engine": pct_eng,
                "trans": pct_tra,
                "cabin": pct_cab,
                "chassis": pct_cha,
                "wheels": pct_whl,
                "trailer": pct_trl,
            },
            "raw_wear": [w_eng, w_tra, w_cab, w_cha, w_whl, w_trl],
        }
        return payload

    def send_telemetry_packet(self, payload: Dict[str, Any]):
        """Dong goi va truyen sang ESP32 duoi dang JSON gon nhe."""
        if not self.ser or not self.ser.is_open:
            return

        # Chi gui cac truong ESP32 Dashboard thuc su can, khong gui coords/accel/damage chi tiet de tranh tran bo dem Serial UART
        esp_keys = {
            "game", "spd", "lim", "cc", "cc_on", "gear", "g_str", "g_type", "rpm", "rpm_max",
            "fuel", "fuel_l", "fuel_cap", "fuel_warn", "fuel_r",
            "trk_w", "trl_w", "crg_w", "crg_wgt", "trl_on", "odo",
            "bl_l", "bl_r", "low", "high", "park",
            "air", "air_p", "oil_t", "water_t", "brake_t",
            "diff_l", "lift_a", "brk_l", "rev_l",
            "ret", "wip", "wip_lvl", "bcn", "mbrk",
            "horn", "eng", "park_l",
            "eta_d", "eta_t", "gh", "gm",
            "conn", "load", "load_pct", "pause", "evt", "evt_dur"
        }
        filtered = {k: payload[k] for k in esp_keys if k in payload}
        json_bytes = json.dumps(filtered, separators=(",", ":")).encode("utf-8")
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
        gear_str = p.get("g_str") or (f"D{p.get('gear', 0)}" if p.get("gear", 0) > 0 else ("N" if p.get("gear", 0) == 0 else f"R{abs(p.get('gear', 0))}"))
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
        wip_lvl = p.get("wip_lvl", 1 if p.get("wip", 0) else 0)
        wip_str = f"WIP:{wip_lvl}" if p.get("wip", 0) else "     "
        horn_str = "HORN!" if p.get("horn", 0) else "     "
        eng_str = "ENG" if p.get("eng", 1) else "!OFF"
        bcn_str = "BCN" if p.get("bcn", 0) else "   "
        evt_str = f"| >> {p['evt']} <<" if p.get("evt") else ""

        trkw = p.get("trk_w", 0)
        trlw = f"{p.get('trl_w', 0)}%" if p.get("trl_on", 0) else "--"
        crgw = f"{p.get('crg_w', 0)}%" if (p.get("trl_on", 0) and p.get("crg_w", -1) >= 0) else "--"
        fuel_l = p.get("fuel_l", 0)
        fuel_str = f"{fuel}% ({fuel_l}L)" if fuel_l > 0 else f"{fuel}%"
        eta_d = p.get("eta_d", 0)
        eta_str = f"| ETA: {eta_d}km " if eta_d > 0 else ""
        route = p.get("route", "")
        route_str = f"| {route} " if route else ""

        try:
            sys.stdout.write(
                f"\r| {bl_l} {gear_str:3s} {bl_r} | SPD: {spd:3d} km/h {lim_str} {warn_str} | RPM: {rpm:4d} | FUEL: {fuel_str} | TRK: {trkw}% | TRL: {trlw} | CRG: {crgw} {eta_str}{route_str}| {eng_str} {horn_str} {wip_str} {ret_str} {bcn_str} | TX: {self.current_fps:4.1f} FPS {evt_str} "
            )
            sys.stdout.flush()
        except Exception:
            pass

    def run(self):
        self.running = True
        print("\n" + "=" * 70)
        print("  MIU-ETS2 - TELEMETRY & GAME DATA DISPLAY COMPANION")
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
    parser = argparse.ArgumentParser(description="MIU-ETS2 - Telemetry & Game Data Display Companion")
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
