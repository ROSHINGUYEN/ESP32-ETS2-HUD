@echo off
chcp 65001 >nul
title Kich hoat Tu dong Nhan dien Game ETS2 (MIU-ETS2)
cls

echo ======================================================================
echo    MIU-ETS2 - KICH HOAT CHE DO TU DONG 100% (CHAY NGAM WINDOWS)
echo ======================================================================
echo.
echo [*] Dang dung tien trinh cu neu dang chay...
taskkill /F /IM pythonw.exe >nul 2>&1

echo [*] Dang dang ky tu dong khoi dong vao Windows Registry (HKCU\Run)...
python "%~dp0..\tools\setup_autostart.py"

echo.
echo [*] Dang khoi dong tien trinh chay ngam ngay lap tuc...
start "" pythonw "%~dp0..\tools\miu_desktop_ets2.py"

echo.
echo ======================================================================
echo  DA KICH HOAT THANH CONG!
echo ======================================================================
echo  1. Ung dung bay gio dang CHAY NGAM TANG HINH (0% CPU, khong hien cua so).
echo  2. TU DONG KHOI DONG moi khi bat may tinh Windows qua Registry HKCU\Run.
echo  3. TU DONG LOAI BO COM1 (chi nhan dien dung cong USB ESP32-S3 / CH343).
echo  4. TU DONG KET NOI lai robot ESP32 khi cam/rut cap USB.
echo  5. TU DONG LAY LUONG API/Shared Memory khi vao game ETS2:
echo     - Hien thi Loading Splash -> Chuyen sang HUD Taplo day du.
echo     - Khi thoat game: Tu dong quay ve mat robot RoboEyes.
echo.
echo Cua so se tu dong dong sau 3 giay...
timeout /t 3 >nul
