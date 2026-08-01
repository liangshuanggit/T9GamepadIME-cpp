$ErrorActionPreference = "Stop"
$url = "https://github.com/jrsoftware/issrc/releases/download/is-6_7_3/innosetup-6.7.3.exe"
$out = Join-Path $env:TEMP "innosetup-6.7.3.exe"
Write-Host "Downloading $url ..."
Invoke-WebRequest -Uri $url -OutFile $out
$f = Get-Item $out
Write-Host ("Downloaded: {0} bytes" -f $f.Length)
Write-Host "Installing silently ..."
Start-Process -Wait -FilePath $out -ArgumentList "/VERYSILENT","/SUPPRESSMSGBOXES","/NORESTART","/SP-"
Write-Host "Install done."
