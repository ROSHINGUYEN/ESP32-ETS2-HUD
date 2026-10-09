@echo off
REM ============================================================
REM OLED Test Helper - Flash sketch test OLED doc lap
REM ============================================================
REM Su dung: copy file test vao src_dir, flash, xem OLED, restore
REM ============================================================

setlocal

set ROOT=%~dp0..
set SRC=%ROOT%\miu_eilik
set TOOLS=%ROOT%\tools

echo.
echo ============================================
echo  OLED TEST HELPER
echo ============================================
echo.
echo File chinh hien tai:  %SRC%\miu_eilik.ino
echo File test:            %TOOLS%\oled_test.ino
echo.
echo Lua chon:
echo   1) Flash test sketch (thay the file chinh)
echo   2) Khoi phuc file chinh
echo   3) Flash + Monitor (sau khi da chon 1)
echo   4) Chi Monitor
echo   5) Thoat
echo.

set /p choice="Nhap 1/2/3/4/5: "

if "%choice%"=="1" goto flash_test
if "%choice%"=="2" goto restore
if "%choice%"=="3" goto flash_monitor
if "%choice%"=="4" goto monitor_only
if "%choice%"=="5" goto end
goto end

:flash_test
if exist "%SRC%\miu_eilik.ino.bak" (
    echo [LOI] File backup da ton tai - co the lan truoc chua restore
    echo Xoa file backup cu: %SRC%\miu_eilik.ino.bak
    del "%SRC%\miu_eilik.ino.bak"
)
copy "%SRC%\miu_eilik.ino" "%SRC%\miu_eilik.ino.bak" >nul
copy "%TOOLS%\oled_test.ino" "%SRC%\miu_eilik.ino" >nul
echo [OK] Da copy test sketch vao %SRC%\miu_eilik.ino
echo [OK] File goc da backup thanh .bak
echo.
echo Tiep theo: chay lenh 3 de flash + monitor
goto end

:restore
if not exist "%SRC%\miu_eilik.ino.bak" (
    echo [LOI] Khong co file backup de khoi phuc
    goto end
)
copy "%SRC%\miu_eilik.ino.bak" "%SRC%\miu_eilik.ino" >nul
del "%SRC%\miu_eilik.ino.bak"
echo [OK] Da khoi phuc file goc v6.10
goto end

:flash_monitor
echo [UPLOAD] Dang flash + monitor...
cd /d "%ROOT%"
call "E:\.platformio\penv\Scripts\platformio.exe" run -t upload -t monitor -p COM5
goto end

:monitor_only
echo [MONITOR] Mo Serial Monitor COM5 @ 115200...
echo Nhan Ctrl+C de thoat
cd /d "%ROOT%"
call "E:\.platformio\penv\Scripts\platformio.exe" device monitor -p COM5 -b 115200
goto end

:end
echo.
endlocal
