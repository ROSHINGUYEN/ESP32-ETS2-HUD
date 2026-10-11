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
from ctypes import wintypes
from typing import Optional, Dict, Any

class NullWriter:
    def write(self, *args, **kwargs):
        pass
    def flush(self):
        pass

# Kiem tra neu stdout/stderr bi loi handle hoac chay ngam khong co console (SW_HIDE)
try:
    if sys.stdout is None:
        raise OSError("No stdout")
    sys.stdout.write("")
    sys.stdout.flush()
    if hasattr(sys.stdout, "reconfigure"):
        try:
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        except Exception:
            pass
except Exception:
    try:
        log_path = os.path.join(os.path.dirname(__file__), "daemon.log")
        _log_file = open(log_path, "a", encoding="utf-8", buffering=1)
        sys.stdout = _log_file
        sys.stderr = _log_file
    except Exception:
        sys.stdout = NullWriter()
        sys.stderr = NullWriter()

try:
    if sys.stderr is None:
        raise OSError("No stderr")
    sys.stderr.write("")
    sys.stderr.flush()
except Exception:
    sys.stderr = sys.stdout

try:
    import serial
    import serial.tools.list_ports as list_ports
except ImportError:
    print("[ERROR] Thieu thu vien pyserial. Cai dat bang: pip install pyserial")
    sys.exit(1)

try:
    import requests
except ImportError:
    print("[ERROR] Thieu thu vien requests. Cai dat bang: pip install requests")
    sys.exit(1)


DEFAULT_BAUD = 115200
DEFAULT_ETS2_URL = "http://127.0.0.1:25555/api/ets2/telemetry"
DEFAULT_FS25_URL = "http://127.0.0.1:8080/api/fs25/telemetry"
DEFAULT_TEST_JSON = os.path.join(
    os.path.dirname(__file__), "..", "API-ETS2", "server", "Ets2TestTelemetry.json"
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

    def connect(self) -> bool:
        if self.pBuf:
            return True
        # File mapping name do ets2-telemetry-server.dll tao trong game
        self.hMap = self.k32.OpenFileMappingW(0x0004, False, "Local\\Ets2TelemetryServer")
        if self.hMap:
            self.pBuf = self.k32.MapViewOfFile(self.hMap, 0x0004, 0, 0, 2048)
            if self.pBuf:
                return True
            self.k32.CloseHandle(self.hMap)
            self.hMap = None
        return False

    def close(self):
        if self.pBuf:
            try:
                self.k32.UnmapViewOfFile(self.pBuf)
            except Exception:
                pass
            self.pBuf = None
        if self.hMap:
            try:
                self.k32.CloseHandle(self.hMap)
            except Exception:
                pass
            self.hMap = None

    def read(self) -> Optional[Dict[str, Any]]:
        if not self.connect():
            return None
        try:
            raw = ctypes.string_at(self.pBuf, 1024)
            time_ms, paused = struct.unpack_from("<II", raw, 0)
            if time_ms == 0:
                return None

            speed_ms = struct.unpack_from("<f", raw, 24)[0]
            gear = struct.unpack_from("<i", raw, 64)[0]
            rpm, rpmMax, fuel, fuelCap = struct.unpack_from("<4f", raw, 80)
            retarder = struct.unpack_from("<i", raw, 564)[0]
            cc_on = raw[580]
            wipers = raw[581]
            park = raw[582]
            mbrk = raw[583]
            eng = raw[585]
            bl_l = raw[586]
            bl_r = raw[587]
            low = raw[591]
            high = raw[592]
            beacon = raw[604]
            air = raw[608]
            fuel_warn = struct.unpack_from("<i", raw, 624)[0]
            wears = struct.unpack_from("<6f", raw, 656)
            total_wear = float(sum(wears))
            cc_spd = struct.unpack_from("<f", raw, 684)[0] * 3.6
            disp_gear = struct.unpack_from("<i", raw, 996)[0]
            nav_d = struct.unpack_from("<f", raw, 1000)[0] / 1000.0
            nav_lim = struct.unpack_from("<f", raw, 1008)[0] * 3.6

            fuel_pct = int(round(100.0 * fuel / fuelCap)) if fuelCap > 0 else 50
            spd_kmh = max(0, int(round(speed_ms * 3.6)))

            return {
                "game": "ets2",
                "spd": spd_kmh,
                "lim": int(round(nav_lim)) if nav_lim > 0 else 0,
                "cc": int(round(cc_spd)) if cc_spd > 0 else 0,
                "cc_on": 1 if cc_on else 0,
                "gear": int(disp_gear if disp_gear != 0 else gear),
                "rpm": int(round(rpm)),
                "rpm_max": int(round(rpmMax if rpmMax > 500 else 2500)),
                "fuel": fuel_pct,
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
                "eta_d": int(round(nav_d)) if nav_d > 0 else 0,
                "conn": 1,
                "pause": 1 if (paused != 0) else 0,
            }
        except Exception:
            self.close()
            return None


def find_default_port() -> Optional[str]:
    """Tự động tìm cổng COM của ESP32-S3 hoặc CH340/CH343."""
    ports = list(list_ports.comports())
    if not ports:
        return None

    # Ưu tiên các cổng USB có chữ ký ESP32 / CH34x / USB Serial
    for p in ports:
        desc = (p.description or "").lower()
        hwid = (p.hwid or "").lower()
        if "ch34" in desc or "ch34" in hwid or "esp32" in desc or "usb serial" in desc:
            return p.device

    # Nếu chỉ có 1 cổng không phải COM1
    non_com1 = [p.device for p in ports if p.device != "COM1"]
    if non_com1:
        return non_com1[0]

    return ports[0].device


class Ets2DesktopCompanion:
    def __init__(
        self,
        port: str,
        baud: int = DEFAULT_BAUD,
        server_url: Optional[str] = None,
        fps: int = 25,
        demo: bool = False,
        game: str = "ets2",
        test_file: Optional[str] = None,
    ):
        self.port_name = port
        self.baud = baud
        self.game = game.lower()
        if server_url:
            self.server_url = server_url
        else:
            self.server_url = DEFAULT_FS25_URL if self.game == "fs25" else DEFAULT_ETS2_URL

        self.target_interval = 1.0 / max(1, fps)
        self.demo = demo
        self.test_file = test_file or DEFAULT_TEST_JSON
        self.ser: Optional[serial.Serial] = None
        self.running = False

        # Session HTTP tai su dung TCP socket keep-alive (giam do tre < 2ms)
        self.session = requests.Session()

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
            Ets2SharedMemoryReader() if (self.game == "ets2" and sys.platform == "win32") else None
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

        try:
            print(f"[SERIAL] Dang mo cong {self.port_name} ({self.baud} baud)...")
            self.ser = serial.Serial()
            self.ser.port = self.port_name
            self.ser.baudrate = self.baud
            self.ser.timeout = 0.1
            self.ser.write_timeout = 0.2
            self.ser.dtr = False
            self.ser.rts = False
            self.ser.open()
            time.sleep(0.3)
            # Neu o che do DEMO thi bat che do GAME ngay
            if self.demo:
                if self.game == "fs25":
                    self.send_command("MODE=FS25")
                else:
                    self.send_command("MODE=GAME")
            else:
                # Khi chua vao game, de robot o che do bieu cam mat RoboEyes mac dinh
                self.send_command("MODE=EYES")
            print(f"[SERIAL] Da ket noi thanh cong voi {self.port_name} (Che do: {self.game.upper()})!")
            return True
        except Exception as e:
            self.ser = None
            return False

    def close_serial(self):
        if self.shm_reader:
            self.shm_reader.close()
        if self.ser:
            try:
                if self.ser.is_open:
                    # Tra man hinh ve mat RoboEyes truoc khi dong
                    self.ser.write(b"MODE=EYES\n")
                    time.sleep(0.1)
                    self.ser.close()
            except Exception:
                pass
            self.ser = None
            print("[SERIAL] Da dong cong Serial an toan.")

    def ensure_serial(self) -> bool:
        """Tu dong kiem tra va ket noi lai cong Serial neu bi ngat."""
        if self.ser and self.ser.is_open:
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

    def fetch_server_telemetry(self) -> Optional[Dict[str, Any]]:
        """Lay telemetry tu game server qua HTTP REST (ETS2 port 25555 hoac FS25 port 8080/SimHub)."""
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
            else:
                self.shm_reader.close()

        # 2. Fallback sang HTTP Telemetry Server (Port 25555 cho ETS2, Port 8080 cho FS25)
        raw = self.fetch_server_telemetry()
        if raw:
            self.server_ok = True
            self.source_name = f"HTTP REST ({self.server_url})"
            if self.game == "fs25":
                return self.extract_fs25_telemetry_payload(raw)
            else:
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
            "eta_d": max(5, int(145 - self._sim_t * 0.5)),
            "eta_t": max(5, int(110 - self._sim_t * 0.4)),
            "conn": 1,
            "pause": 0,
        }
        if demo_evt:
            res["evt"] = demo_evt
        return res

    def generate_fs25_demo_telemetry(self) -> Dict[str, Any]:
        """Mo phong may cay / may gat Farming Simulator 25 tren dong."""
        self._sim_t += self.target_interval

        # Chu ky 45s: Ra dong -> Ha dan cay & Bat may gat -> Lam viec -> Day thung -> Xa lua
        cycle = (self._sim_t % 45.0) / 45.0
        beacon = True  # Bat den chop xoay
        work_light = (int(self._sim_t / 15.0) % 2 == 0)

        if cycle < 0.15:
            # Di chuyen tren duong: 24 km/h, nang dan, tat nong cu
            spd = 24.0 + math.sin(self._sim_t) * 2.0
            cru = 24
            cru_on = True
            down = False
            pto = False
            pipe = False
            fill = 30
        elif cycle < 0.70:
            # Dang cay / gat tren dong: 12-14 km/h, ha dan nong cu, bat may moc (PTO)
            spd = 13.0 + math.sin(self._sim_t * 2.0) * 1.5
            cru = 14
            cru_on = True
            down = True
            pto = True
            pipe = False
            # Thung lua day dan tu 30% -> 98%
            progress = (cycle - 0.15) / 0.55
            fill = int(30 + progress * 68)
        else:
            # Thung day: Dung may, mo ong xa (Pipe Out) de xa lua vao xe keo
            spd = 0.0
            cru = 14
            cru_on = False
            down = False
            pto = False
            pipe = True
            progress = (cycle - 0.70) / 0.30
            fill = max(5, int(98 - progress * 93))

        turn_l = (0.65 < cycle < 0.70)
        self._sim_fuel = max(10.0, 95.0 - (self._sim_t * 0.02))

        return {
            "game": "fs25",
            "speed": int(round(spd)),
            "maxSpd": 45,
            "rpm": 1850 if spd > 0 else 850,
            "rpmMax": 2200,
            "cru": cru,
            "cruOn": cru_on,
            "fuel": int(self._sim_fuel),
            "fill": fill,
            "dmg": 4,
            "hr": round(14.2 + self._sim_t / 3600.0, 1),
            "bcn": beacon,
            "wrk": work_light,
            "turnL": turn_l,
            "turnR": False,
            "down": down,
            "pto": pto,
            "pipe": pipe,
            "motor": True,
        }

    def extract_telemetry_payload(self, raw: Dict[str, Any]) -> Dict[str, Any]:
        """Trich xuat cac truong can thiet tu JSON goc cua Telemetry Server."""
        game = raw.get("game", {})
        truck = raw.get("truck", {})
        nav = raw.get("navigation", {})

        fuel = truck.get("fuel", 500.0)
        fuel_cap = truck.get("fuelCapacity", 700.0)
        fuel_pct = int(round(100.0 * fuel / fuel_cap)) if fuel_cap > 0 else 50

        # Gear: displayedGear hoac gear
        gear = truck.get("displayedGear", truck.get("gear", 0))

        # ETA
        eta_d = int(round(nav.get("estimatedDistance", 0) / 1000.0))

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
            "eta_d": eta_d,
            "conn": 1 if game.get("connected", False) else 0,
            "pause": 1 if game.get("paused", False) else 0,
        }
        return payload

    def extract_fs25_telemetry_payload(self, raw: Dict[str, Any]) -> Dict[str, Any]:
        """Trich xuat du lieu that tu Farming Simulator 25 Telemetry Mod / SimHub REST."""
        vehicle = raw.get("vehicle", raw.get("drivetrain", raw))
        lights = raw.get("lights", vehicle.get("lights", {}))

        spd = vehicle.get("speed", raw.get("SpeedKmh", raw.get("Speed", 0.0)))
        max_spd = vehicle.get("maxSpeed", raw.get("MaxSpeed", 50.0))
        rpm = vehicle.get("rpm", raw.get("Rpm", raw.get("EngineRpm", 0.0)))
        rpm_max = vehicle.get("rpmMax", 2200.0)
        fuel_pct = vehicle.get("fuelPercentage", raw.get("Fuel", 100.0))
        fill_pct = raw.get("fillLevelPct", raw.get("fillPercentage", 0.0))

        return {
            "game": "fs25",
            "speed": int(round(spd)),
            "maxSpd": int(round(max_spd)),
            "rpm": int(round(rpm)),
            "rpmMax": int(round(rpm_max)),
            "cru": int(round(vehicle.get("cruiseControlSpeed", 0))),
            "cruOn": bool(vehicle.get("cruiseControlActive", False)),
            "fuel": int(round(fuel_pct)),
            "fill": int(round(fill_pct)),
            "dmg": int(round(vehicle.get("damagePercentage", 0))),
            "hr": round(vehicle.get("operatingTimeHours", 0.0), 1),
            "bcn": bool(lights.get("beaconLights", raw.get("BeaconLight", False))),
            "wrk": bool(lights.get("workLights", raw.get("WorkLight", False))),
            "turnL": bool(lights.get("turnLightLeft", False)),
            "turnR": bool(lights.get("turnLightRight", False)),
            "down": bool(raw.get("implementLowered", raw.get("ToolLowered", False))),
            "pto": bool(raw.get("implementTurnedOn", raw.get("PtoActive", False))),
            "pipe": bool(raw.get("pipeUnfolded", raw.get("PipeOpen", False))),
            "motor": bool(vehicle.get("motorRunning", True)),
        }

    def send_telemetry_packet(self, payload: Dict[str, Any]):
        """Dong goi va truyen sang ESP32 duoi dang JSON gon nhe."""
        if not self.ser or not self.ser.is_open:
            return

        json_bytes = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        if self.game == "fs25":
            line = b"FS25=" + json_bytes + b"\n"
        else:
            line = b"ETS2=" + json_bytes + b"\n"

        try:
            self.ser.write(line)
            self.packets_sent += 1
            self.fps_counter += 1
        except Exception as e:
            print(f"[WRITE ERROR] {e}")
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
        if self.game == "fs25":
            spd = p.get("speed", 0)
            max_s = p.get("maxSpd", 45)
            fill = p.get("fill", 0)
            fuel = p.get("fuel", 0)
            cru = p.get("cru", 0)
            cru_str = f"CRU:{cru}k" if p.get("cruOn") else "CRU:--"
            bcn = "[BCN]" if p.get("bcn") else "     "
            wrk = "[WRK]" if p.get("wrk") else "     "
            pto = "[PTO:ON]" if p.get("pto") else "[PTO:OFF]"
            down = "[DOWN]" if p.get("down") else "[UP]  "
            pipe = "[PIPE]" if p.get("pipe") else "      "

            bar_len = 10
            filled = min(bar_len, max(0, int(spd / max_s * bar_len)))
            bar_str = "#" * filled + "-" * (bar_len - filled)

            sys.stdout.write(
                f"\r| FS25 | SPD:{spd:2d} km/h [{bar_str}] | {bcn} {wrk} | {pto} {down} {pipe} | FILL:{fill:2d}% | OIL:{fuel:2d}% | {cru_str} | TX:{self.current_fps:4.1f} FPS "
            )
            sys.stdout.flush()
        else:
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

            sys.stdout.write(
                f"\r| {bl_l} {gear_str:3s} {bl_r} | SPEED: {spd:3d} km/h {lim_str} {warn_str} | RPM: {rpm:4d} | FUEL: {fuel:2d}% | {ret_str} {wip_str} {bcn_str} | TX: {self.current_fps:4.1f} FPS {evt_str} "
            )
            sys.stdout.flush()

    def run(self):
        self.running = True
        game_name = "FARMING SIMULATOR 25" if self.game == "fs25" else "EURO TRUCK SIMULATOR 2"
        print("\n" + "=" * 70)
        print(f"  MIU EILIK - {game_name} DASHBOARD COMPANION")
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

                if not self.ensure_serial():
                    source = "Windows Shared Memory" if self.game == "ets2" else f"Port 8080 ({self.server_url})"
                    sys.stdout.write(
                        f"\r[DANG DOI ROBOT] Dang doi ket noi robot ESP32 qua USB... | {source}   "
                    )
                    sys.stdout.flush()
                    time.sleep(1.0)
                    continue

                if self.demo:
                    if self.game == "fs25":
                        payload = self.generate_fs25_demo_telemetry()
                    else:
                        payload = self.generate_demo_telemetry()
                    self.send_telemetry_packet(payload)
                    self.render_console_dashboard(payload)
                else:
                    payload = self.fetch_live_telemetry()
                    game_connected = False
                    if payload is not None:
                        if self.game == "fs25":
                            game_connected = bool(payload.get("motor") or payload.get("speed", 0) > 0)
                        else:
                            game_connected = bool(payload.get("conn", 0) == 1)

                    if game_connected and payload is not None:
                        # Vao duoc man hinh chinh cua game
                        if not self.game_connected_prev:
                            print(f"\n[GAME CONNECTED] {self.game.upper()} da vao man hinh chinh! ({self.source_name}) -> Robot dang hien Loading Splash...")
                            self.game_connected_prev = True
                            if self.game == "fs25":
                                self.send_command("MODE=FS25")
                            else:
                                self.send_command("MODE=GAME")
                        if self.game == "ets2":
                            self.process_events(payload)
                        self.send_telemetry_packet(payload)
                        self.render_console_dashboard(payload)
                    else:
                        # Game chua bat hoac chua ket noi
                        if self.game_connected_prev:
                            print(f"\n[GAME DISCONNECTED] Game da thoat -> Chuyen robot ve mat RoboEyes")
                            self.send_command("MODE=EYES")
                            self.game_connected_prev = False

                        source = "Shared Memory hoac Port 25555" if self.game == "ets2" else f"Port 8080 ({self.server_url})"
                        sys.stdout.write(
                            f"\r[DANG CHO GAME] Dang doi ETS2 khoi dong ({source})... | Robot van giu man hinh Mat RoboEyes   "
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
        finally:
            self.running = False
            self.close_serial()


def main():
    parser = argparse.ArgumentParser(description="Miu Eilik Sim Dashboard Companion (ETS2 & FS25)")
    parser.add_argument("--port", "-p", default=None, help="Cong COM (vi du: COM4, COM5)")
    parser.add_argument("--game", "-g", choices=["ets2", "fs25"], default="ets2", help="Game sim: ets2 hoac fs25 (mac dinh: ets2)")
    parser.add_argument("--baud", "-b", type=int, default=DEFAULT_BAUD, help="Baud rate (mac dinh: 115200)")
    parser.add_argument("--server", "-s", default=None, help="URL Telemetry Server (mac dinh: 25555 cho ETS2, 8080 cho FS25)")
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
        game=args.game,
        test_file=args.test_file,
    )
    companion.run()


if __name__ == "__main__":
    main()
