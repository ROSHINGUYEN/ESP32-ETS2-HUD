"""Kiem tra truc tiep Shared Memory cua ETS2 Telemetry Plugin."""
import ctypes, struct
from ctypes import wintypes

k32 = ctypes.windll.kernel32
k32.OpenFileMappingW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
k32.OpenFileMappingW.restype = wintypes.HANDLE
k32.MapViewOfFile.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD, ctypes.c_size_t]
k32.MapViewOfFile.restype = ctypes.c_void_p

hMap = k32.OpenFileMappingW(0x0004, False, "Local\\Ets2TelemetryServer")
if not hMap:
    print("[SHM] KHONG CO Shared Memory")
    print("  => ETS2 chua chay HOAC plugin DLL chua duoc load")
    print("  => Kiem tra: D:\\SteamLibrary\\...\\bin\\win_x64\\plugins\\ets2-telemetry-server.dll")
else:
    pBuf = k32.MapViewOfFile(hMap, 0x0004, 0, 0, 512)
    if pBuf:
        raw = ctypes.string_at(pBuf, 512)
        game_time_min, paused = struct.unpack_from("<II", raw, 0)
        plugin_ver = struct.unpack_from("<I", raw, 8)[0]
        time_abs   = struct.unpack_from("<I", raw, 160)[0]
        speed_ms   = struct.unpack_from("<f", raw, 24)[0]
        gear       = struct.unpack_from("<i", raw, 64)[0]
        rpm        = struct.unpack_from("<f", raw, 80)[0]

        print("[SHM] CO Shared Memory - ETS2 dang chay!")
        print(f"  offset   0  game_time_min = {game_time_min} => {game_time_min//60%24:02d}:{game_time_min%60:02d}")
        print(f"  offset   4  paused        = {paused}")
        print(f"  offset   8  plugin_ver    = {plugin_ver}")
        print(f"  offset  24  speed_ms      = {speed_ms:.2f} m/s ({speed_ms*3.6:.1f} km/h)")
        print(f"  offset  64  gear          = {gear}")
        print(f"  offset  80  rpm           = {rpm:.0f}")
        in_game = "VAO GAME ROI!" if time_abs != 0 else "DANG O MENU/LOADING"
        print(f"  offset 160  timeAbsolute  = {time_abs}  => {in_game}")
        k32.UnmapViewOfFile(ctypes.c_void_p(pBuf))
    k32.CloseHandle(hMap)
