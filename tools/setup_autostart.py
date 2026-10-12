import winreg
import os
import sys

def enable_autostart():
    # Tu dong xac dinh duong dan pythonw va script tren moi may tinh
    python_exe = sys.executable
    pythonw_candidate = os.path.join(os.path.dirname(python_exe), "pythonw.exe")
    pythonw = pythonw_candidate if os.path.exists(pythonw_candidate) else python_exe
    script = os.path.abspath(os.path.join(os.path.dirname(__file__), "miu_desktop_ets2.py"))
    cmd_val = f'"{pythonw}" "{script}"'

    key = winreg.OpenKey(
        winreg.HKEY_CURRENT_USER,
        r"Software\Microsoft\Windows\CurrentVersion\Run",
        0,
        winreg.KEY_SET_VALUE
    )
    winreg.SetValueEx(key, "MiuEilikETS2Companion", 0, winreg.REG_SZ, cmd_val)
    winreg.CloseKey(key)

    # Verify
    rkey = winreg.OpenKey(
        winreg.HKEY_CURRENT_USER,
        r"Software\Microsoft\Windows\CurrentVersion\Run",
        0,
        winreg.KEY_READ
    )
    val, _ = winreg.QueryValueEx(rkey, "MiuEilikETS2Companion")
    winreg.CloseKey(rkey)
    print("AUTOSTART REGISTERED:", val)

def disable_autostart():
    try:
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            r"Software\Microsoft\Windows\CurrentVersion\Run",
            0,
            winreg.KEY_SET_VALUE
        )
        winreg.DeleteValue(key, "MiuEilikETS2Companion")
        winreg.CloseKey(key)
        print("AUTOSTART UNREGISTERED")
    except FileNotFoundError:
        print("NOT FOUND")

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "disable":
        disable_autostart()
    else:
        enable_autostart()
