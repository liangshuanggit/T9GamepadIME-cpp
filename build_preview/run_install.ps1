$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$setup = Join-Path $root "installer\T9GamepadIME-Setup.exe"
Write-Host "Running silent install: $setup"
$p = Start-Process -FilePath $setup -ArgumentList "/VERYSILENT","/SUPPRESSMSGBOXES","/NORESTART","/SP-" -Wait -PassThru
Write-Host ("Setup exit code: {0}" -f $p.ExitCode)
$dir = Join-Path $env:LOCALAPPDATA "Programs\T9GamepadIME"
if (Test-Path $dir) {
    Write-Host "Installed files:"
    Get-ChildItem -Recurse $dir | ForEach-Object { Write-Host ("  " + $_.FullName + "  (" + $_.Length + ")") }
} else {
    Write-Host "INSTALL FAILED: dir not found"
    exit 1
}
