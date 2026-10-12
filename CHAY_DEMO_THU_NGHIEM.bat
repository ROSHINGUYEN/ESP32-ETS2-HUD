@echo off
chcp 65001 >nul
title MIU EILIK - Mo phong lai xe ETS2 Demo
cls
echo ======================================================================
echo    MIU EILIK ROBOT - CHE DO MO PHONG DEMO (KHONG CAN BAT GAME)
echo ======================================================================
echo.
echo [*] Mo phong lai xe: Tang toc, Chuyen so, Xi-nhan, RPM, Xang tren Robot.
echo [*] Nhan Ctrl+C de dung.
echo.
echo ======================================================================
echo.

python "%~dp0tools\miu_desktop_ets2.py" --demo %*
pause
