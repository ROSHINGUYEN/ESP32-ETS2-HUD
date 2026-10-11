import ctypes, struct

k32 = ctypes.windll.kernel32
k32.MapViewOfFile.restype = ctypes.c_void_p
h = k32.OpenFileMappingW(4, False, 'Local\\Ets2TelemetryServer')
if h:
    p = k32.MapViewOfFile(h, 4, 0, 0, 4096)
    raw = ctypes.string_at(p, 4096)
    print("Mapped size:", len(raw))
    # Look for strings and numbers
    # Let's inspect strings at various offsets
    for off in range(0, 4000, 4):
        # check float
        f = struct.unpack_from('<f', raw, off)[0]
        i = struct.unpack_from('<i', raw, off)[0]
        if 10.0 <= f <= 500000.0 and off in (24, 76, 80, 84, 88, 684, 1000, 1004, 1008):
            print(f"Known Float @ {off}: {f:.2f}")
        # search string
        s = raw[off:off+32]
        if any(c > 32 and c < 127 for c in s[:4]) and b'\x00' in s:
            try:
                txt = s.split(b'\x00')[0].decode('utf-8', errors='ignore')
                if len(txt) > 3 and all(32 <= ord(c) < 127 for c in txt):
                    if off not in (0,):
                        print(f"String @ {off}: '{txt}'")
            except:
                pass
else:
    print("Cannot open Local\\Ets2TelemetryServer")
