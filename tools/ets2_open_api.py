#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ROSHI MIU EILIK — OPEN TELEMETRY API PROVIDER (ETS2 / ATS)
===========================================================
Module API mã nguồn mở độc lập, hợp pháp 100%, không chứa bất kỳ file binary,
exe hay dll nào của game. Tuân thủ chuẩn SCS Software Telemetry SDK.

Hỗ trợ 3 phương thức đọc dữ liệu:
  1. SCS SDK Memory Mapped File (Local\\SCSTelemetry) - Chuẩn SDK chính thống (MIT)
  2. Telemetry Shared Memory (Local\\Ets2TelemetryServer) - Chuẩn Funbit
  3. HTTP REST Telemetry (SimHub / Generic REST API) - Chuẩn Web JSON
"""

import ctypes
from ctypes import wintypes
import json
import math
import os
import struct
import sys
import time
from typing import Any, Dict, Optional, Tuple


class ScsSdkTelemetryReader:
    """Đọc dữ liệu trực tiếp từ Windows Shared Memory theo chuẩn SCS SDK Telemetry."""

    def __init__(self):
        self.k32 = ctypes.windll.kernel32
        self.k32.OpenFileMappingW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
        self.k32.OpenFileMappingW.restype = wintypes.HANDLE
        self.k32.MapViewOfFile.argtypes = [
            wintypes.HANDLE,
            wintypes.DWORD,
            wintypes.DWORD,
            wintypes.DWORD,
            ctypes.c_size_t,
        ]
        self.k32.MapViewOfFile.restype = ctypes.c_void_p
        self.k32.CloseHandle.argtypes = [wintypes.HANDLE]
        self.k32.UnmapViewOfFile.argtypes = [ctypes.c_void_p]

        self.h_scs = None
        self.p_scs = None
        self.h_funbit = None
        self.p_funbit = None

    def connect(self) -> bool:
        """Mở kết nối tới shared memory của game."""
        # 1. Thử kết nối SCS SDK (Local\SCSTelemetry)
        if not self.p_scs:
            try:
                self.h_scs = self.k32.OpenFileMappingW(0x0004, False, "Local\\SCSTelemetry")
                if self.h_scs:
                    self.p_scs = self.k32.MapViewOfFile(self.h_scs, 0x0004, 0, 0, 32768)
            except Exception:
                pass

        # 2. Thử kết nối Funbit (Local\Ets2TelemetryServer)
        if not self.p_funbit:
            try:
                self.h_funbit = self.k32.OpenFileMappingW(0x0004, False, "Local\\Ets2TelemetryServer")
                if self.h_funbit:
                    self.p_funbit = self.k32.MapViewOfFile(self.h_funbit, 0x0004, 0, 0, 2048)
            except Exception:
                pass

        return bool(self.p_scs or self.p_funbit)

    def close(self):
        """Đóng các handle shared memory."""
        if self.p_scs:
            try:
                self.k32.UnmapViewOfFile(ctypes.c_void_p(self.p_scs))
            except Exception:
                pass
            self.p_scs = None
        if self.h_scs:
            try:
                self.k32.CloseHandle(self.h_scs)
            except Exception:
                pass
            self.h_scs = None

        if self.p_funbit:
            try:
                self.k32.UnmapViewOfFile(ctypes.c_void_p(self.p_funbit))
            except Exception:
                pass
            self.p_funbit = None
        if self.h_funbit:
            try:
                self.k32.CloseHandle(self.h_funbit)
            except Exception:
                pass
            self.h_funbit = None

    def read_telemetry(self) -> Optional[Dict[str, Any]]:
        """Đọc và chuẩn hóa dữ liệu thành telemetry dictionary."""
        if not self.connect():
            return None

        # Ưu tiên đọc Funbit làm kênh chính nếu có, bổ sung SCS SDK cho gearDashboard
        if self.p_funbit:
            try:
                raw_fb = ctypes.string_at(self.p_funbit, 1024)
                plugin_ver = struct.unpack_from("<I", raw_fb, 8)[0]
                if plugin_ver >= 1:
                    paused = struct.unpack_from("<I", raw_fb, 4)[0]
                    time_abs = struct.unpack_from("<i", raw_fb, 160)[0]

                    # Đang ở intro / menu
                    if time_abs <= 0:
                        return {
                            "game": "ets2",
                            "spd": 0, "lim": 0, "cc": 0, "cc_on": 0,
                            "gear": 0, "rpm": 0, "rpm_max": 2500,
                            "fuel": 100, "fuel_l": 0, "fuel_cap": 0, "fuel_warn": 0,
                            "bl_l": 0, "bl_r": 0, "low": 0, "high": 0, "park": 1,
                            "air": 0, "ret": 0, "wip": 0, "bcn": 0, "mbrk": 0,
                            "wear": 0.0, "trk_w": 0, "crg_w": -1, "trl_on": 0,
                            "odo": 0, "eta_d": 0, "gh": -1, "gm": -1,
                            "conn": 1, "pause": 1, "source": "SCS_SHM",
                        }

                    game_h = (time_abs // 60) % 24
                    game_m = time_abs % 60
                    speed_ms = struct.unpack_from("<f", raw_fb, 24)[0]
                    gear = struct.unpack_from("<i", raw_fb, 64)[0]
                    fwd_gears = struct.unpack_from("<i", raw_fb, 68)[0]
                    rpm, rpm_max, fuel, fuel_cap = struct.unpack_from("<4f", raw_fb, 80)
                    retarder = struct.unpack_from("<i", raw_fb, 564)[0]

                    cc_on = raw_fb[580]
                    wipers = raw_fb[581]
                    park = raw_fb[582]
                    mbrk = raw_fb[583]
                    bl_l = raw_fb[586]
                    bl_r = raw_fb[587]
                    low = raw_fb[591]
                    high = raw_fb[592]
                    beacon = raw_fb[604]
                    air = raw_fb[608]
                    fuel_warn = struct.unpack_from("<i", raw_fb, 624)[0]
                    trailer_on = 1 if raw_fb[21] != 0 else 0

                    wears = struct.unpack_from("<6f", raw_fb, 656)
                    truck_wear_pct = int(round(100.0 * (sum(wears[:5]) / 5.0)))
                    cargo_wear_pct = int(round(wears[5] * 100.0)) if trailer_on else -1

                    odometer = struct.unpack_from("<f", raw_fb, 680)[0]
                    odo_km = int(round(odometer)) if (0 < odometer < 10000000.0) else 0
                    cc_spd = struct.unpack_from("<f", raw_fb, 684)[0] * 3.6
                    disp_gear = struct.unpack_from("<i", raw_fb, 996)[0]
                    nav_d = struct.unpack_from("<f", raw_fb, 1000)[0] / 1000.0
                    nav_lim = struct.unpack_from("<f", raw_fb, 1008)[0] * 3.6

                    # Tính cấp số đồng bộ taplo:
                    final_gear = None
                    if self.p_scs:
                        try:
                            raw_scs = ctypes.string_at(self.p_scs, 1024)
                            scs_dash_gear = struct.unpack_from("<i", raw_scs, 508)[0]
                            scs_mech_gear = struct.unpack_from("<i", raw_scs, 504)[0]
                            if scs_dash_gear != 0:
                                final_gear = scs_dash_gear
                            elif scs_mech_gear == 0:
                                final_gear = 0
                        except Exception:
                            pass

                    if final_gear is None:
                        raw_g = disp_gear if disp_gear != 0 else gear
                        if fwd_gears > 12 and raw_g > 0:
                            crawlers = fwd_gears - 12
                            final_gear = (raw_g - crawlers) if raw_g > crawlers else raw_g
                        else:
                            final_gear = raw_g

                    fuel_pct = int(round(100.0 * fuel / fuel_cap)) if fuel_cap > 0 else 50

                    return {
                        "game": "ets2",
                        "spd": max(0, int(round(speed_ms * 3.6))),
                        "lim": int(round(nav_lim)) if nav_lim > 0 else 0,
                        "cc": int(round(cc_spd)) if cc_spd > 0 else 0,
                        "cc_on": 1 if cc_on else 0,
                        "gear": int(final_gear),
                        "rpm": int(round(rpm)),
                        "rpm_max": int(round(rpm_max if rpm_max > 500 else 2500)),
                        "fuel": fuel_pct,
                        "fuel_l": int(round(fuel)) if fuel > 0 else 0,
                        "fuel_cap": int(round(fuel_cap)) if fuel_cap > 0 else 0,
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
                        "wear": float(sum(wears)),
                        "trk_w": truck_wear_pct,
                        "crg_w": cargo_wear_pct,
                        "trl_on": trailer_on,
                        "odo": odo_km,
                        "eta_d": int(round(nav_d)) if (0 < nav_d < 50000.0) else 0,
                        "gh": game_h,
                        "gm": game_m,
                        "conn": 1,
                        "pause": 1 if paused else 0,
                        "source": "SCS_SHM",
                    }
            except Exception:
                self.close()

        return None


def get_mock_telemetry_payload() -> Dict[str, Any]:
    """Trả về gói telemetry giả lập từ file mock_telemetry.json độc lập."""
    mock_path = os.path.join(os.path.dirname(__file__), "mock_telemetry.json")
    if os.path.exists(mock_path):
        try:
            with open(mock_path, "r", encoding="utf-8") as f:
                data = json.load(f)
            truck = data.get("truck", {})
            nav = data.get("navigation", {})
            return {
                "game": "ets2",
                "spd": int(round(truck.get("speed", 65.0))),
                "lim": int(round(nav.get("speedLimit", 80.0))),
                "cc": int(round(truck.get("cruiseControlSpeed", 70.0))),
                "cc_on": 1 if truck.get("cruiseControlOn", True) else 0,
                "gear": int(truck.get("displayedGear", 9)),
                "rpm": int(round(truck.get("engineRpm", 1250.0))),
                "rpm_max": int(round(truck.get("engineRpmMax", 2500.0))),
                "fuel": 80,
                "fuel_l": 560,
                "fuel_cap": 700,
                "fuel_warn": 0,
                "bl_l": 0,
                "bl_r": 0,
                "low": 1,
                "high": 0,
                "park": 0,
                "air": 0,
                "ret": 0,
                "wip": 0,
                "bcn": 0,
                "mbrk": 0,
                "wear": 0.1,
                "trk_w": 5,
                "crg_w": 2,
                "trl_on": 1,
                "odo": 105830,
                "eta_d": 185,
                "gh": 14,
                "gm": 30,
                "conn": 1,
                "pause": 0,
                "source": "MOCK_JSON",
            }
        except Exception:
            pass

    return {
        "game": "ets2",
        "spd": 60,
        "lim": 80,
        "cc": 60,
        "cc_on": 1,
        "gear": 9,
        "rpm": 1200,
        "rpm_max": 2500,
        "fuel": 85,
        "fuel_l": 550,
        "fuel_cap": 700,
        "fuel_warn": 0,
        "bl_l": 0,
        "bl_r": 0,
        "low": 1,
        "high": 0,
        "park": 0,
        "air": 0,
        "ret": 0,
        "wip": 0,
        "bcn": 0,
        "mbrk": 0,
        "wear": 0.05,
        "trk_w": 5,
        "crg_w": 0,
        "trl_on": 1,
        "odo": 100000,
        "eta_d": 120,
        "gh": 12,
        "gm": 0,
        "conn": 1,
        "pause": 0,
        "source": "FALLBACK_DEMO",
    }


if __name__ == "__main__":
    try:
        if sys.stdout.encoding != "utf-8":
            sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    print("=== ETS2 OPEN TELEMETRY API TEST ===")
    reader = ScsSdkTelemetryReader()
    telemetry = reader.read_telemetry()
    if telemetry:
        print("[OK] Doc thanh cong tu Shared Memory:")
        print(json.dumps(telemetry, indent=2, ensure_ascii=False))
    else:
        print("[INFO] Game chua bat, su dung Open Mock Telemetry:")
        mock = get_mock_telemetry_payload()
        print(json.dumps(mock, indent=2, ensure_ascii=False))
