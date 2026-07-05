# Flash Meshtastic build to P-3 V1.6 module (hold BOOT, tap EN if connect fails).
param(
    [string]$Port = "COM5",
    [string]$EnvName = "espdeka-p3-v16-lr1121"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$FwDir = Join-Path $Root "firmware-master"
$Bin = Join-Path $FwDir ".pio\build\$EnvName\firmware.bin"

if (-not (Test-Path $Bin)) {
    Write-Host "Firmware not built — running build.ps1 first..." -ForegroundColor Yellow
    & (Join-Path $Root "build.ps1") -EnvName $EnvName
}

Set-Location $FwDir
python -m platformio run -e $EnvName -t upload --upload-port $Port
exit $LASTEXITCODE
