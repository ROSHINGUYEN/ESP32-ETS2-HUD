#!/usr/bin/env python3
"""
ESP32 MIU-ETS2 Model Context Protocol (MCP) Server
Cung cap cong giao tiep JSON-RPC 2.0 (stdio) cho AI IDE (Antigravity/Cursor/Claude)
de theo doi Telemetry Euro Truck Simulator 2, xem logs he thong va dieu khien ESP32-S3.
"""

import sys
import os
import json
import time
import struct
import ctypes
from ctypes import wintypes
from typing import Dict, Any, Optional, List

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8")
except Exception:
    pass

SERVER_NAME = "esp32-eilik"
SERVER_VERSION = "1.0.0"

REPO_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TOOLS_DIR = os.path.join(REPO_DIR, "tools")
STATE_FILE = os.path.join(TOOLS_DIR, "telemetry_state.json")
LOG_FILE = os.path.join(TOOLS_DIR, "daemon.log")
CMD_FILE = os.path.join(TOOLS_DIR, "mcp_command.txt")


class DirectShmReader:
    """Doc truc tiep du lieu Telemetry ETS2 tu Windows Shared Memory (Fallback khi daemon chua bat)."""
    def __init__(self):
        if sys.platform != "win32":
            return
        self.k32 = ctypes.windll.kernel32
        self.k32.OpenFileMappingW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
        self.k32.OpenFileMappingW.restype = wintypes.HANDLE
        self.k32.MapViewOfFile.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD, ctypes.c_size_t]
        self.k32.MapViewOfFile.restype = ctypes.c_void_p
        self.k32.CloseHandle.argtypes = [wintypes.HANDLE]
        self.k32.UnmapViewOfFile.argtypes = [ctypes.c_void_p]

    def read_telemetry(self) -> Optional[Dict[str, Any]]:
        if sys.platform != "win32":
            return None
        h_map = self.k32.OpenFileMappingW(0x0004, False, "Local\\Ets2TelemetryServer")
        if not h_map:
            return None
        p_buf = self.k32.MapViewOfFile(h_map, 0x0004, 0, 0, 1024)
        if not p_buf:
            self.k32.CloseHandle(h_map)
            return None
        try:
            raw = ctypes.string_at(p_buf, 1024)
            time_ms, paused = struct.unpack_from("<II", raw, 0)
            speed_ms = struct.unpack_from("<f", raw, 24)[0]
            gear = struct.unpack_from("<i", raw, 64)[0]
            rpm, rpmMax, fuel, fuelCap = struct.unpack_from("<4f", raw, 80)
            eng = raw[585] if len(raw) > 585 else 0

            # Ho tro ca Steam va TruckersMP (time_ms co the = 0 khi choi multiplayer)
            if time_ms == 0 and eng == 0 and rpm < 100 and abs(speed_ms) < 0.05 and gear == 0:
                return None

            t_game = struct.unpack_from("<i", raw, 160)[0]
            if t_game > 0:
                game_h = (t_game // 60) % 24
                game_m = t_game % 60
            else:
                now_lt = time.localtime()
                game_h = now_lt.tm_hour
                game_m = now_lt.tm_min

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

            trailer_on = 1 if raw[21] != 0 else 0
            wears = struct.unpack_from("<6f", raw, 656)
            truck_wear_pct = int(round(100.0 * (sum(wears[:5]) / 5.0)))
            cargo_wear_pct = int(round(wears[5] * 100.0)) if trailer_on else -1

            odometer = struct.unpack_from("<f", raw, 680)[0]
            cc_spd = struct.unpack_from("<f", raw, 684)[0] * 3.6
            disp_gear = struct.unpack_from("<i", raw, 996)[0]
            fwd_gears = struct.unpack_from("<i", raw, 68)[0]
            nav_d = struct.unpack_from("<f", raw, 1000)[0] / 1000.0
            nav_lim = struct.unpack_from("<f", raw, 1008)[0] * 3.6
            spd_kmh = int(round(abs(speed_ms) * 3.6))

            # Doc gearDashboard tu Local\SCSTelemetry neu co
            final_gear = None
            try:
                h_scs = self.k32.OpenFileMappingW(0x0004, False, "Local\\SCSTelemetry")
                if h_scs:
                    p_scs = self.k32.MapViewOfFile(h_scs, 0x0004, 0, 0, 1024)
                    if p_scs:
                        raw_scs = ctypes.string_at(p_scs, 1024)
                        scs_dash_gear = struct.unpack_from("<i", raw_scs, 508)[0]
                        scs_mech_gear = struct.unpack_from("<i", raw_scs, 504)[0]
                        if scs_dash_gear != 0:
                            final_gear = scs_dash_gear
                        elif scs_mech_gear == 0:
                            final_gear = 0
                        self.k32.UnmapViewOfFile(ctypes.c_void_p(p_scs))
                    self.k32.CloseHandle(h_scs)
            except Exception:
                pass

            if final_gear is None:
                raw_g = disp_gear if disp_gear != 0 else gear
                if fwd_gears > 12 and raw_g > 0:
                    crawlers = fwd_gears - 12
                    final_gear = (raw_g - crawlers) if raw_g > crawlers else raw_g
                else:
                    final_gear = raw_g

            return {
                "game": "ets2",
                "speed_kmh": spd_kmh,
                "speed_limit_kmh": int(round(nav_lim)) if nav_lim > 0 else 0,
                "gear": int(final_gear),
                "gear_str": f"D{final_gear}" if final_gear > 0 else ("N" if final_gear == 0 else f"R{abs(final_gear)}"),
                "rpm": int(round(rpm)),
                "rpm_max": int(round(rpmMax if rpmMax > 500 else 2500)),
                "fuel_pct": int(round(100.0 * fuel / fuelCap)) if fuelCap > 0 else 0,
                "fuel_liters": int(round(fuel)),
                "fuel_capacity": int(round(fuelCap)),
                "fuel_warning": bool(fuel_warn > 0 or (fuelCap > 0 and fuel / fuelCap < 0.15)),
                "truck_wear_pct": truck_wear_pct,
                "cargo_wear_pct": cargo_wear_pct,
                "trailer_attached": bool(trailer_on),
                "odometer_km": int(round(odometer)),
                "eta_distance_km": int(round(nav_d)),
                "game_time": f"{game_h:02d}:{game_m:02d}",
                "cruise_control_on": bool(cc_on),
                "cruise_speed": int(round(cc_spd)) if cc_spd > 0 else 0,
                "retarder_level": int(retarder),
                "wipers_on": bool(wipers),
                "park_brake_on": bool(park),
                "motor_brake_on": bool(mbrk),
                "low_beam_on": bool(low),
                "high_beam_on": bool(high),
                "beacon_on": bool(beacon),
                "blinker_left": bool(bl_l),
                "blinker_right": bool(bl_r),
                "engine_running": bool(eng),
                "paused": bool(paused != 0),
                "source": "Windows Shared Memory (Direct SHM)"
            }
        except Exception:
            return None
        finally:
            self.k32.UnmapViewOfFile(p_buf)
            self.k32.CloseHandle(h_map)


def is_process_running(pid: int) -> bool:
    """Kiem tra PID con song tren Windows."""
    if not pid or pid <= 0:
        return False
    try:
        k32 = ctypes.windll.kernel32
        h_proc = k32.OpenProcess(0x0400, False, pid) # PROCESS_QUERY_INFORMATION
        if h_proc:
            k32.CloseHandle(h_proc)
            return True
    except Exception:
        pass
    return False


def get_system_status() -> Dict[str, Any]:
    """Lay tong quan trang thai he thong ESP32, Daemon va ETS2."""
    state = {}
    if os.path.exists(STATE_FILE):
        try:
            with open(STATE_FILE, "r", encoding="utf-8") as f:
                state = json.load(f)
        except Exception:
            pass

    daemon_running = False
    pid = state.get("pid")
    if pid and is_process_running(pid):
        daemon_running = bool(state.get("daemon_running", False))

    now = time.time()
    last_update = state.get("timestamp", 0)
    data_fresh = (now - last_update) < 3.0 if last_update > 0 else False

    shm_reader = DirectShmReader()
    shm_live = shm_reader.read_telemetry()

    return {
        "status": "ONLINE" if (daemon_running or shm_live) else "IDLE",
        "companion_daemon": {
            "running": daemon_running,
            "pid": pid if daemon_running else None,
            "port": state.get("port", "COM5"),
            "baud": state.get("baud", 115200),
            "serial_connected": state.get("serial_connected", False) if daemon_running else False,
            "fps": state.get("fps", 0.0) if daemon_running else 0.0,
            "packets_sent": state.get("packets_sent", 0) if daemon_running else 0,
            "last_seen_sec_ago": round(now - last_update, 1) if last_update else None
        },
        "game_telemetry": {
            "game": state.get("game", "ets2"),
            "game_active": bool(state.get("game_connected", False) or shm_live),
            "source": state.get("source", "Direct SHM" if shm_live else "None"),
            "active_cockpit_event": state.get("active_event", "")
        },
        "hardware": {
            "target": "ESP32-S3 (Dual-Core 240MHz, 16MB Flash, 8MB PSRAM)",
            "display": "SSD1306 OLED 128x64 (I2C)",
            "firmware_hud": "Euro Truck Full Taplo (Font 3, RPM Dải Kinh Tế, T:%, C:%, Lít Xăng, Biển Báo Chớp, Toast)"
        }
    }


def get_current_telemetry() -> Dict[str, Any]:
    """Lay luong telemetry chi tiet nhat."""
    # 1. Kiem tra telemetry tu state file
    if os.path.exists(STATE_FILE):
        try:
            with open(STATE_FILE, "r", encoding="utf-8") as f:
                state = json.load(f)
            telem = state.get("telemetry")
            if telem and (time.time() - state.get("timestamp", 0) < 3.0):
                return {
                    "source": "Companion Daemon State File",
                    "timestamp": state.get("timestamp"),
                    "data": telem
                }
        except Exception:
            pass

    # 2. Fallback: Doc truc tiep Windows Shared Memory cua game ETS2
    shm_reader = DirectShmReader()
    direct = shm_reader.read_telemetry()
    if direct:
        return {
            "source": "Direct Windows Shared Memory (Local\\Ets2TelemetryServer)",
            "timestamp": time.time(),
            "data": direct
        }

    return {
        "source": "None",
        "message": "ETS2 hien tai chua bat hoac xe chua no may vao man hinh lai xe.",
        "data": None
    }


def get_recent_logs(lines: int = 30) -> List[str]:
    """Doc N dong log gan nhat tu tools/daemon.log."""
    if not os.path.exists(LOG_FILE):
        return ["Chua co file log (daemon.log chua duoc tao)."]
    try:
        with open(LOG_FILE, "r", encoding="utf-8", errors="ignore") as f:
            all_lines = f.readlines()
        tail = all_lines[-max(1, lines):]
        return [l.strip() for l in tail if l.strip()]
    except Exception as e:
        return [f"Loi khi doc file log: {e}"]


def send_esp32_command(command: str) -> Dict[str, Any]:
    """Gui lenh dieu khien toi ESP32 qua Daemon IPC hoac Serial truc tiep."""
    cmd = command.strip()
    if not cmd:
        return {"success": False, "error": "Command rong"}

    # Neu daemon dang chay, gui qua file IPC mcp_command.txt
    status = get_system_status()
    if status["companion_daemon"]["running"]:
        try:
            with open(CMD_FILE, "w", encoding="utf-8") as f:
                f.write(cmd + "\n")
            return {
                "success": True,
                "method": "Forwarded via Companion Daemon IPC",
                "command": cmd
            }
        except Exception as e:
            return {"success": False, "error": str(e)}

    # Neu daemon khong chay, co the mo Serial COM5 truc tiep de gui
    try:
        import serial
        port = status["companion_daemon"]["port"] or "COM5"
        ser = serial.Serial(port, 115200, timeout=1.0)
        ser.write((cmd + "\n").encode("utf-8"))
        time.sleep(0.05)
        ser.close()
        return {
            "success": True,
            "method": f"Direct Serial ({port})",
            "command": cmd
        }
    except Exception as e:
        return {
            "success": False,
            "error": f"Khong the mo cong Serial truc tiep: {e}. Hay bat BAT_TU_DONG_CHAY_NGAM.bat de daemon chay ngam quan ly cong COM."
        }


def trigger_event_toast(message: str) -> Dict[str, Any]:
    """Ban popup thong bao len man hinh robot OLED."""
    msg = message.strip()
    if not msg:
        return {"success": False, "error": "Message rong"}
    # Lệnh EVENT tren ESP32
    return send_esp32_command(f"EVENT={msg}")


# =====================================================================================
# MCP JSON-RPC 2.0 PROTOCOL HANDLER
# =====================================================================================

TOOLS_DEFINITIONS = [
    {
        "name": "esp32_get_status",
        "description": "Lấy trạng thái kết nối ESP32-S3, cổng Serial COM5, tiến trình daemon chạy ngầm và trạng thái game ETS2.",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "additionalProperties": False
        }
    },
    {
        "name": "esp32_get_telemetry",
        "description": "Đọc toàn bộ luồng telemetry thời gian thực của Euro Truck Simulator 2 (tốc độ, số, RPM, xăng lít/%, độ bền xe/hàng, cự ly, đèn, sự kiện).",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "additionalProperties": False
        }
    },
    {
        "name": "esp32_get_logs",
        "description": "Lấy các dòng log mới nhất từ daemon cầu nối ESP32 (lịch sử sự kiện buồng lái, phạt tốc độ, thu phí, kết nối serial).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "lines": {
                    "type": "integer",
                    "description": "Số dòng log cần lấy (mặc định 30 dòng)"
                }
            },
            "additionalProperties": False
        }
    },
    {
        "name": "esp32_send_command",
        "description": "Gửi lệnh điều khiển chế độ màn hình hoặc cấu hình trực tiếp tới robot ESP32-S3 (MODE=GAME, MODE=EYES, MODE=CLOCK, MODE=OFF).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "command": {
                    "type": "string",
                    "description": "Lệnh điều khiển, ví dụ: MODE=GAME, MODE=EYES, MODE=CLOCK"
                }
            },
            "required": ["command"],
            "additionalProperties": False
        }
    },
    {
        "name": "esp32_trigger_event",
        "description": "Bắn thông báo popup/toast tức thời lên màn hình OLED robot (ví dụ cảnh báo phạt tiền, va chạm, hoặc tin nhắn tùy chọn).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "message": {
                    "type": "string",
                    "description": "Nội dung thông báo hiển thị trên màn hình"
                }
            },
            "required": ["message"],
            "additionalProperties": False
        }
    }
]


def handle_tool_call(tool_name: str, arguments: Dict[str, Any]) -> str:
    if tool_name == "esp32_get_status":
        res = get_system_status()
        return json.dumps(res, ensure_ascii=False, indent=2)

    elif tool_name == "esp32_get_telemetry":
        res = get_current_telemetry()
        return json.dumps(res, ensure_ascii=False, indent=2)

    elif tool_name == "esp32_get_logs":
        lines = int(arguments.get("lines", 30))
        logs = get_recent_logs(lines)
        return "\n".join(logs)

    elif tool_name == "esp32_send_command":
        cmd = arguments.get("command", "")
        res = send_esp32_command(cmd)
        return json.dumps(res, ensure_ascii=False, indent=2)

    elif tool_name == "esp32_trigger_event":
        msg = arguments.get("message", "")
        res = trigger_event_toast(msg)
        return json.dumps(res, ensure_ascii=False, indent=2)

    else:
        raise ValueError(f"Unknown tool: {tool_name}")


def run_stdio_server():
    """Vong lap doc ghi JSON-RPC 2.0 stdio cho MCP."""
    sys.stdin.reconfigure(encoding="utf-8")
    sys.stdout.reconfigure(encoding="utf-8")

    while True:
        line = sys.stdin.readline()
        if not line:
            break
        line = line.strip()
        if not line:
            continue

        try:
            req = json.loads(line)
        except Exception:
            continue

        req_id = req.get("id")
        method = req.get("method")
        params = req.get("params", {})

        if method == "initialize":
            resp = {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "protocolVersion": "2024-11-05",
                    "capabilities": {
                        "tools": {}
                    },
                    "serverInfo": {
                        "name": SERVER_NAME,
                        "version": SERVER_VERSION
                    }
                }
            }
            sys.stdout.write(json.dumps(resp) + "\n")
            sys.stdout.flush()

        elif method == "notifications/initialized":
            pass

        elif method == "ping":
            resp = {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {}
            }
            sys.stdout.write(json.dumps(resp) + "\n")
            sys.stdout.flush()

        elif method == "tools/list":
            resp = {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "tools": TOOLS_DEFINITIONS
                }
            }
            sys.stdout.write(json.dumps(resp) + "\n")
            sys.stdout.flush()

        elif method == "tools/call":
            tool_name = params.get("name")
            arguments = params.get("arguments", {})
            try:
                out_text = handle_tool_call(tool_name, arguments)
                resp = {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "result": {
                        "content": [
                            {
                                "type": "text",
                                "text": out_text
                            }
                        ],
                        "isError": False
                    }
                }
            except Exception as e:
                resp = {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "result": {
                        "content": [
                            {
                                "type": "text",
                                "text": f"Error: {e}"
                            }
                        ],
                        "isError": True
                    }
                }
            sys.stdout.write(json.dumps(resp) + "\n")
            sys.stdout.flush()

        else:
            if req_id is not None:
                resp = {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "error": {
                        "code": -32601,
                        "message": f"Method not found: {method}"
                    }
                }
                sys.stdout.write(json.dumps(resp) + "\n")
                sys.stdout.flush()


def main():
    if "--test" in sys.argv:
        print("=== ESP32 MCP SERVER SELF-TEST ===")
        print("\n--- STATUS ---")
        print(json.dumps(get_system_status(), ensure_ascii=False, indent=2))
        print("\n--- TELEMETRY ---")
        print(json.dumps(get_current_telemetry(), ensure_ascii=False, indent=2))
        print("\n--- LOGS (LAST 5 LINES) ---")
        for l in get_recent_logs(5):
            print("  ", l)
        print("\n[OK] MCP Server self-test completed!")
        return

    run_stdio_server()


if __name__ == "__main__":
    main()
