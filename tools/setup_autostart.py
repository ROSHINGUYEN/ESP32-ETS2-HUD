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

    # 1. Dang ky vao Windows Registry (HKCU\Run)
    key = winreg.OpenKey(
        winreg.HKEY_CURRENT_USER,
        r"Software\Microsoft\Windows\CurrentVersion\Run",
        0,
        winreg.KEY_SET_VALUE
    )
    winreg.SetValueEx(key, "MiuEilikETS2Companion", 0, winreg.REG_SZ, cmd_val)
    winreg.CloseKey(key)

    # 2. Dang ky vao Thu muc Windows Startup (shell:startup)
    startup_dir = os.path.join(
        os.environ.get("APPDATA", ""),
        r"Microsoft\Windows\Start Menu\Programs\Startup"
    )
    if os.path.exists(startup_dir):
        vbs_path = os.path.join(startup_dir, "MiuEilikETS2Companion.vbs")
        vbs_content = (
            'Set WshShell = CreateObject("WScript.Shell")\n'
            f'WshShell.Run """{pythonw}"" ""{script}""", 0, False\n'
        )
        try:
            with open(vbs_path, "w", encoding="utf-8") as f:
                f.write(vbs_content)
            print("[OK] DA THEM VAO THU MUC STARTUP WINDOWS:", vbs_path)
        except Exception as e:
            print("[WARN] Khong the ghi thu muc Startup:", e)

    # Verify Registry
    rkey = winreg.OpenKey(
        winreg.HKEY_CURRENT_USER,
        r"Software\Microsoft\Windows\CurrentVersion\Run",
        0,
        winreg.KEY_READ
    )
    val, _ = winreg.QueryValueEx(rkey, "MiuEilikETS2Companion")
    winreg.CloseKey(rkey)
    print("[OK] DA DANG KY WINDOWS REGISTRY RUN:", val)

def disable_autostart():
    # 1. Xoa khoi Registry
    try:
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            r"Software\Microsoft\Windows\CurrentVersion\Run",
            0,
            winreg.KEY_SET_VALUE
        )
        winreg.DeleteValue(key, "MiuEilikETS2Companion")
        winreg.CloseKey(key)
        print("[OK] DA XOA KHOI WINDOWS REGISTRY")
    except FileNotFoundError:
        pass

    # 2. Xoa khoi Thu muc Startup
    startup_dir = os.path.join(
        os.environ.get("APPDATA", ""),
        r"Microsoft\Windows\Start Menu\Programs\Startup"
    )
    vbs_path = os.path.join(startup_dir, "MiuEilikETS2Companion.vbs")
    if os.path.exists(vbs_path):
        try:
            os.remove(vbs_path)
            print("[OK] DA XOA KHOI THU MUC STARTUP WINDOWS")
        except Exception:
            pass

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "disable":
        disable_autostart()
    else:
        enable_autostart()
