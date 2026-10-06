@echo off
title MIU EILIK - Web Flasher Suite
cd /d "%~dp0"

echo =====================================================================
echo    ROBOT MIU EILIK ESP32-S3 - WEB FLASHER LOCAL SERVER
echo =====================================================================
echo.

if exist "web\server.py" (
    python web\server.py
    goto :done
)

if exist "server.py" (
    python server.py
    goto :done
)

echo [LOI] Khong tim thay server.py!
echo Vui long kiem tra lai thu muc.

:done
if errorlevel 1 (
    echo.
    echo [THONG BAO] Da dung may chu hoac xay ra loi.
    pause
)
