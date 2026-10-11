@echo off
chcp 65001 >nul
title MIU EILIK - ETS2 Dashboard Companion
cls
echo ======================================================================
echo    MIU EILIK ROBOT - EURO TRUCK SIMULATOR 2 DASHBOARD COMPANION
echo ======================================================================
echo.
echo [*] Cau noi Telemetry giua Game ETS2 va Robot ESP32-S3 qua USB Serial.
echo [*] Tinh nang:
echo     + Doc truc tiep tu Windows Shared Memory (Khong can app trung gian)
echo     + Tu dong hien man hinh Loading Splash khi vao game
echo     + Tu dong hien thi HUD Toc do, Cap so, RPM, Xang, Xi-nhan
echo     + Tu dong tro ve bieu cam mat RoboEyes khi thoat game
echo.
echo ======================================================================
echo Dang khoi chay Desktop Companion...
echo Nhan Ctrl+C de dung chuong trinh.
echo ======================================================================
echo.

python "%~dp0tools\miu_desktop_ets2.py" --game ets2 %*

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [LOI] Chuong trinh ket thuc voi ma loi: %ERRORLEVEL%
    pause
)
