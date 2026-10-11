@echo off
chcp 65001 >nul
title Kich hoat Tu dong Nhan dien Game ETS2 (Chay ngam)
cls

echo ======================================================================
echo    MIU EILIK - KICH HOAT CHE DO TU DONG 100% (CHAY NGAM WINDOWS)
echo ======================================================================
echo.
echo [*] Dang kiem tra va dung tien trinh cu neu dang chay...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*miu_desktop_ets2.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force; Write-Host '[OK] Da dong tien trinh cu.' -ForegroundColor Yellow }"

echo [*] Dang sao chep khoi dong vao thu muc Windows Startup...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$startup = [Environment]::GetFolderPath('Startup'); $target = Join-Path $startup 'MiuEilikETS2Companion.vbs'; Copy-Item '%~dp0tools\launch_silent.vbs' $target -Force; if (Test-Path $target) { Write-Host '[OK] Da cai dat vao Windows Startup thanh cong!' -ForegroundColor Green } else { Write-Host '[LOI] Khong the sao chep vao Startup!' -ForegroundColor Red }"

echo.
echo [*] Dang khoi dong tien trinh chay ngam ngay lap tuc...
wscript.exe "%~dp0tools\launch_silent.vbs"

echo.
echo ======================================================================
echo  DA KICH HOAT THANH CONG!
echo ======================================================================
echo  1. Ung dung bay gio dang CHAY NGAM TANG HINH (0% CPU, khong hien cua so).
echo  2. TU DONG KHOI DONG moi khi bat may tinh Windows (0-touch).
echo  3. TU DONG KET NOI lai robot ESP32 khi cam/rut cap USB.
echo  4. TU DONG LAY LUONG API/Shared Memory khi vao game ETS2:
echo     - Hien thi Loading Splash -> Chuyen sang HUD Taplo day du.
echo     - Khi thoat game: Tu dong quay ve mat robot RoboEyes.
echo ======================================================================
echo.
pause
