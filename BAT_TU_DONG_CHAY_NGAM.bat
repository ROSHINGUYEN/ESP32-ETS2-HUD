@echo off
chcp 65001 >nul
title Kich hoat Tu dong Nhan dien Game ETS2 (Chay ngam)
cls

echo ======================================================================
echo    MIU EILIK - KICH HOAT CHE DO TU DONG 100% (CHAY NGAM WINDOWS)
echo ======================================================================
echo.
echo [*] Dang thiet lap khoi dong cung Windows...

powershell -NoProfile -ExecutionPolicy Bypass -Command "$startup = [Environment]::GetFolderPath('Startup'); $target = Join-Path $startup 'MiuEilikETS2Companion.vbs'; Copy-Item '%~dp0tools\launch_silent.vbs' $target -Force; if (Test-Path $target) { Write-Host '[OK] Da them vao thu muc Windows Startup thanh cong!' -ForegroundColor Green } else { Write-Host '[LOI] Khong the sao chep vao Startup!' -ForegroundColor Red }"

echo.
echo [*] Dang khoi chay tien trinh chay ngam ngay bay gio...
wscript.exe "%~dp0tools\launch_silent.vbs"

echo.
echo ======================================================================
echo  DA KICH HOAT THANH CONG!
echo ======================================================================
echo  1. Ung dung bay gio dang CHAY NGAM TANG HINH (0% CPU, khong hien cua so).
echo  2. Tu dong khoi dong moi khi ban bat may tinh.
echo  3. Tu dong ket noi va nhan dien lai robot neu rut/cam cap USB.
echo  4. Ban chi can bat Game ETS2:
echo     - Robot se TU DONG nhan dien game!
echo     - Tu dong hien thi Loading Splash -^> HUD Toc do.
echo     - Khi thoat game: Tu dong tro ve bieu cam mat RoboEyes.
echo ======================================================================
echo.
pause
