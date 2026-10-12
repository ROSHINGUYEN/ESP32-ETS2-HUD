@echo off
chcp 65001 >nul
title MIU-ETS2 - Kích hoạt chạy ngầm
echo ======================================================================
echo    MIU-ETS2 - KICH HOAT CHAY NGAM (KHONG HIEN CUA SO)
echo ======================================================================
echo.
echo [*] Dang khoi dong MIU-ETS2 Companion duoi nen Windows...

start "" pythonw "%~dp0..\tools\miu_desktop_ets2.py"

echo.
echo [OK] Da kich hoat chay ngam thanh cong!
echo [*] Du lieu telemetry tu ETS2 se tu dong ban sang Robot ESP32.
echo [*] Cua so nay se tu dong dong sau 2 giay...
timeout /t 2 >nul
