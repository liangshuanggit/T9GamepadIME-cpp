$ErrorActionPreference = "Stop"
$url = "https://raw.githubusercontent.com/jrsoftware/issrc/refs/heads/main/Files/Languages/ChineseSimplified.isl"
$out = Join-Path $PSScriptRoot "ChineseSimplified.isl"
Write-Host "Downloading to $out ..."
Invoke-WebRequest -Uri $url -OutFile $out
$f = Get-Item $out
Write-Host ("OK: {0} bytes" -f $f.Length)
