@echo off
chcp 65001 >nul
title MIU-ETS2 - Mo phong lai xe ETS2 Demo
cls
echo ======================================================================
echo    MIU-ETS2 - CHE DO MO PHONG DEMO (KHONG CAN BAT GAME)
echo ======================================================================
echo.
echo [*] Mo phong lai xe: Tang toc, Chuyen so, Xi-nhan, RPM, Xang tren Robot.
echo [*] Nhan Ctrl+C de dung.
echo.
echo ======================================================================
echo.

python "%~dp0..\tools\miu_desktop_ets2.py" --demo %*
pause
