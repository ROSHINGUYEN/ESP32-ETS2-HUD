@echo off
chcp 65001 >nul
title MIU EILIK - Tắt chạy ngầm
echo ======================================================================
echo    MIU EILIK ROBOT - DUNG TIEN TRINH CHAY NGAM
echo ======================================================================
echo.
echo [*] Dang huy tu dong khoi dong Registry HKCU\Run...
python "%~dp0tools\setup_autostart.py" disable

echo [*] Dang dung tien trinh pythonw.exe...
taskkill /F /IM pythonw.exe >nul 2>&1

echo.
echo [OK] Da dung toan bo tien trinh cau noi Telemetry.
echo [*] Robot se tu dong tro ve man hinh mat robot sau vai giay.
echo [*] Cua so se tu dong dong sau 2 giay...
timeout /t 2 >nul
