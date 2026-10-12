@echo off
chcp 65001 >nul
title Tat Tu dong Chay ngam MIU-ETS2
cls

echo ======================================================================
echo    MIU-ETS2 - TẮT CHẾ ĐỘ TỰ ĐỘNG CHẠY NGẦM
echo ======================================================================
echo.
echo [*] Dang huy dang ky khoi dong Registry HKCU\Run...
python "%~dp0..\tools\setup_autostart.py" disable

echo [*] Dang dung tien trinh chay ngam...
taskkill /F /IM pythonw.exe >nul 2>&1

echo.
echo ======================================================================
echo  Da tat che do chay ngam hoan toan.
echo ======================================================================
echo.
pause
