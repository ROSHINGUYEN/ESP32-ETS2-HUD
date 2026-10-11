import time, ctypes, struct, json, serial

print("Connecting to COM5...")
ser = serial.Serial('COM5', 115200, timeout=0.1, write_timeout=2.0)
time.sleep(0.5)

print("Sending MODE=GAME...")
ser.write(b"MODE=GAME\n")
time.sleep(0.2)

# Read any initial serial log from ESP32
while ser.in_waiting:
    line = ser.readline().decode('utf-8', errors='ignore').strip()
    if line:
        print("[ESP32 LOG]:", line)

# Open shared memory
k32 = ctypes.windll.kernel32
k32.MapViewOfFile.restype = ctypes.c_void_p
h = k32.OpenFileMappingW(4, False, 'Local\\Ets2TelemetryServer')
if not h:
    print("Cannot open Local\\Ets2TelemetryServer!")
    exit(1)
p = k32.MapViewOfFile(h, 4, 0, 0, 1024)

print("Streaming for 10 seconds...")
start = time.time()
packet_count = 0

try:
    while time.time() - start < 10:
        # Read from ESP32
        while ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(">>> [ESP32 RETURNED]:", line)

        raw = ctypes.string_at(p, 1024)
        t, paused = struct.unpack_from('<II', raw, 0)
        speed_ms = struct.unpack_from('<f', raw, 24)[0]
        gear = struct.unpack_from('<i', raw, 64)[0]
        rpm, rpmMax, fuel, fuelCap = struct.unpack_from('<4f', raw, 80)
        fuel_pct = int(round(100.0 * fuel / fuelCap)) if fuelCap > 0 else 50
        nav_d = struct.unpack_from('<f', raw, 1000)[0] / 1000.0
        nav_lim = struct.unpack_from('<f', raw, 1008)[0] * 3.6

        payload = {
            "game": "ets2",
            "spd": max(0, int(round(speed_ms * 3.6))),
            "lim": int(round(nav_lim)) if nav_lim > 0 else 0,
            "cc": 0, "cc_on": 0,
            "gear": gear,
            "rpm": int(round(rpm)),
            "rpm_max": int(round(rpmMax)),
            "fuel": fuel_pct,
            "fuel_warn": 0, "bl_l": 0, "bl_r": 0, "low": 0, "high": 0,
            "park": 0, "air": 0, "ret": 0, "wip": 0, "bcn": 0, "mbrk": 0,
            "wear": 0.0, "eta_d": int(round(nav_d)),
            "conn": 1, "pause": paused
        }
        msg = b"ETS2=" + json.dumps(payload, separators=(',', ':')).encode('utf-8') + b"\n"
        ser.write(msg)
        packet_count += 1
        time.sleep(0.033)

finally:
    ser.close()
    print(f"Done! Sent {packet_count} packets.")
