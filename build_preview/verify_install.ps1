$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$setup = Join-Path $root "installer\T9GamepadIME-Setup.exe"
$dir = Join-Path $env:LOCALAPPDATA "Programs\T9GamepadIME"

# 1) 安装
Write-Host "=== Install ==="
$p = Start-Process -FilePath $setup -ArgumentList "/VERYSILENT","/SUPPRESSMSGBOXES","/NORESTART" -Wait -PassThru
Write-Host ("Setup exit: {0}" -f $p.ExitCode)

# 2) 自检（用 Start-Process 捕获退出码）
Write-Host "=== Selftest ==="
$exe = Join-Path $dir "t9ime.exe"
Push-Location $dir
$p2 = Start-Process -FilePath $exe -ArgumentList "--selftest=ni" -Wait -PassThru -RedirectStandardOutput (Join-Path $env:TEMP "t9st.txt")
Pop-Location
$code = $p2.ExitCode
Write-Host ("Selftest exit: {0}" -f $code)

# 3) 卸载
Write-Host "=== Uninstall ==="
$unins = Join-Path $dir "unins000.exe"
$p3 = Start-Process -FilePath $unins -ArgumentList "/VERYSILENT","/SUPPRESSMSGBOXES","/NORESTART" -Wait -PassThru
Write-Host ("Uninstall exit: {0}" -f $p3.ExitCode)
Start-Sleep -Seconds 2
if (Test-Path $dir) {
    Write-Host "UNINSTALL FAILED: dir still exists"
    exit 1
} else {
    Write-Host "Uninstall OK: dir fully removed"
}
exit $code
