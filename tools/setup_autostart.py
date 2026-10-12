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

    # Xoa key cu MiuEilikETS2Companion neu co
    try:
        old_k = winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Microsoft\Windows\CurrentVersion\Run", 0, winreg.KEY_SET_VALUE)
        winreg.DeleteValue(old_k, "MiuEilikETS2Companion")
        winreg.CloseKey(old_k)
    except FileNotFoundError:
        pass

    # 1. Dang ky vao Windows Registry (HKCU\Run)
    key = winreg.OpenKey(
        winreg.HKEY_CURRENT_USER,
        r"Software\Microsoft\Windows\CurrentVersion\Run",
        0,
        winreg.KEY_SET_VALUE
    )
    winreg.SetValueEx(key, "MiuETS2Companion", 0, winreg.REG_SZ, cmd_val)
    winreg.CloseKey(key)

    # 2. Dang ky vao Thu muc Windows Startup (shell:startup)
    startup_dir = os.path.join(
        os.environ.get("APPDATA", ""),
        r"Microsoft\Windows\Start Menu\Programs\Startup"
    )
    if os.path.exists(startup_dir):
        old_vbs = os.path.join(startup_dir, "MiuEilikETS2Companion.vbs")
        if os.path.exists(old_vbs):
            try:
                os.remove(old_vbs)
            except Exception:
                pass

        vbs_path = os.path.join(startup_dir, "MiuETS2Companion.vbs")
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
    val, _ = winreg.QueryValueEx(rkey, "MiuETS2Companion")
    winreg.CloseKey(rkey)
    print("[OK] DA DANG KY WINDOWS REGISTRY RUN:", val)

def disable_autostart():
    # 1. Xoa khoi Registry
    for k_name in ["MiuETS2Companion", "MiuEilikETS2Companion"]:
        try:
            key = winreg.OpenKey(
                winreg.HKEY_CURRENT_USER,
                r"Software\Microsoft\Windows\CurrentVersion\Run",
                0,
                winreg.KEY_SET_VALUE
            )
            winreg.DeleteValue(key, k_name)
            winreg.CloseKey(key)
            print(f"[OK] DA XOA {k_name} KHOI WINDOWS REGISTRY")
        except FileNotFoundError:
            pass

    # 2. Xoa khoi Thu muc Startup
    startup_dir = os.path.join(
        os.environ.get("APPDATA", ""),
        r"Microsoft\Windows\Start Menu\Programs\Startup"
    )
    for v_name in ["MiuETS2Companion.vbs", "MiuEilikETS2Companion.vbs"]:
        vbs_path = os.path.join(startup_dir, v_name)
        if os.path.exists(vbs_path):
            try:
                os.remove(vbs_path)
                print(f"[OK] DA XOA {v_name} KHOI THU MUC STARTUP WINDOWS")
            except Exception:
                pass

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "disable":
        disable_autostart()
    else:
        enable_autostart()
