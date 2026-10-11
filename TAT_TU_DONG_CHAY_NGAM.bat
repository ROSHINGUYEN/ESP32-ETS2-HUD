@echo off
chcp 65001 >nul
title Tat Tu dong Chay ngam Miu Eilik
cls

echo ======================================================================
echo    MIU EILIK - TẮT CHẾ ĐỘ TỰ ĐỘNG CHẠY NGẦM
echo ======================================================================
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command "$startup = [Environment]::GetFolderPath('Startup'); $target = Join-Path $startup 'MiuEilikETS2Companion.vbs'; if (Test-Path $target) { Remove-Item $target -Force; Write-Host '[OK] Da xoa khoi thu muc Windows Startup.' -ForegroundColor Green } else { Write-Host '[*] Khong co trong thu muc Startup.' }; Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*miu_desktop_ets2.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force; Write-Host '[OK] Da dung tien trinh chay ngam.' -ForegroundColor Yellow }"

echo.
echo ======================================================================
echo  Da tat che do chay ngam hoan toan.
echo ======================================================================
echo.
pause
