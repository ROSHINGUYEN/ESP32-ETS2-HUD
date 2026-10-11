$ErrorActionPreference = 'SilentlyContinue'

# Dong bat ky tien trinh miu_desktop_ets2 nao dang chay
Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like "*miu_desktop_ets2.py*" } | ForEach-Object {
    Stop-Process -Id $_.ProcessId -Force
}

$pythonw = (Get-Command pythonw).Source
if (-not $pythonw) {
    $pythonw = (Get-Command python).Source
}

$scriptPath = Join-Path $PSScriptRoot "miu_desktop_ets2.py"

# Chay an hoan toan khong hien cua so
Start-Process -FilePath $pythonw -ArgumentList "`"$scriptPath`"" -WindowStyle Hidden
Write-Host "[OK] Da kich hoat tien trinh chay ngam: $pythonw"
