@echo off
chcp 65001 >nul
title MIU EILIK - Kích hoạt chạy ngầm
echo ======================================================================
echo    MIU EILIK ROBOT - KICH HOAT CHAY NGAM (KHONG HIEN CUA SO)
echo ======================================================================
echo.
echo [*] Dang khoi dong Miu Desktop Companion duoi nen Windows...

start "" pythonw "%~dp0..\tools\miu_desktop_ets2.py"

echo.
echo [OK] Da kich hoat chay ngam thanh cong!
echo [*] Du lieu telemetry tu ETS2 se tu dong ban sang Robot ESP32.
echo [*] Cua so nay se tu dong dong sau 2 giay...
timeout /t 2 >nul
