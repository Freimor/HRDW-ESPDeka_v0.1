# Build Meshtastic for ESPDeka P-3 V1.6 (ESP32-C3 + LR1121).
param(
    [string]$EnvName = "espdeka-p3-v16-lr1121"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$FwDir = Join-Path $Root "firmware-master"
$Zip = Join-Path $Root "meshtastic-firmware.zip"
$VariantSrc = Join-Path $Root "espdeka_p3_v16"
$VariantDst = Join-Path $FwDir "variants\esp32c3\espdeka_p3_v16"
$Py = "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe"

if (-not (Test-Path $FwDir)) {
    Write-Host "Downloading Meshtastic firmware (master.zip)..." -ForegroundColor Cyan
    Invoke-WebRequest -Uri "https://github.com/meshtastic/firmware/archive/refs/heads/master.zip" -OutFile $Zip -UseBasicParsing
    Expand-Archive -Path $Zip -DestinationPath $Root -Force
}

if (-not (Test-Path $Py)) {
    Write-Error "IDF Python 3.11 not found. Install ESP-IDF or adjust path in build.ps1"
}

& $Py -m pip install -q -U platformio

New-Item -ItemType Directory -Force -Path $VariantDst | Out-Null
Copy-Item -Path (Join-Path $VariantSrc "*") -Destination $VariantDst -Force

$UserPrefs = Join-Path $VariantSrc "userPrefs.jsonc"
if (Test-Path $UserPrefs) {
    # Merge ESPDeka prefs into upstream defaults (do not replace entire file).
    $upstreamPrefs = Join-Path $FwDir "userPrefs.jsonc"
    if (-not (Test-Path $upstreamPrefs)) {
        Invoke-WebRequest -Uri "https://raw.githubusercontent.com/meshtastic/firmware/master/userPrefs.jsonc" -OutFile $upstreamPrefs -UseBasicParsing
    }
    $base = Get-Content $upstreamPrefs -Raw
    $overlay = Get-Content $UserPrefs -Raw
    $merged = ($base.TrimEnd() -replace '\}\s*$', '') + ",`n" + ($overlay -replace '^\s*\{\s*', '' -replace '\}\s*$', '') + "`n}"
    Set-Content -Path $upstreamPrefs -Value $merged -NoNewline
}

# Zip download has no git; Meshtastic build script must tolerate missing git.exe.
$PioCustom = Join-Path $FwDir "bin\platformio-custom.py"
if (Test-Path $PioCustom) {
    $pioText = Get-Content $PioCustom -Raw
    $pioText = $pioText -replace 'except subprocess\.CalledProcessError:', 'except (subprocess.CalledProcessError, FileNotFoundError, OSError):'
    Set-Content -Path $PioCustom -Value $pioText -NoNewline
}

# Upstream patches for P-3 V1.6 LR1121 (re-applied each build).
$MainCpp = Join-Path $FwDir "src\main.cpp"
if (Test-Path $MainCpp) {
    $mainText = Get-Content $MainCpp -Raw
    if ($mainText -notmatch 'ESPDEKA_P3_V16') {
        $mainText = $mainText -replace '(SPI\.setFrequency\(4000000\);\r?\n#endif\r?\n#endif)', @'
SPI.setFrequency(4000000);
#endif
#if defined(ESPDEKA_P3_V16)
    pinMode(LORA_MISO, INPUT_PULLUP);
#endif
#endif
'@
        Set-Content -Path $MainCpp -Value $mainText -NoNewline
    }
}

$LrCpp = Join-Path $FwDir "src\mesh\LR11x0Interface.cpp"
if (Test-Path $LrCpp) {
    $lrText = Get-Content $LrCpp -Raw
    if ($lrText -notmatch 'ESPDEKA_P3_V16') {
        $lrText = $lrText -replace '// Allow extra time for TCXO to stabilize after power-on\r?\n    delay\(10\);', @'
// Allow extra time for TCXO / LR1121 reset-BUSY settle after power-on
#if defined(ESPDEKA_P3_V16)
    pinMode(LORA_MISO, INPUT_PULLUP);
    delay(300);
#else
    delay(10);
#endif
'@
        Set-Content -Path $LrCpp -Value $lrText -NoNewline
    }
}

$RlCmd = Join-Path $FwDir ".pio\libdeps\espdeka-p3-v16-lr1121\RadioLib\src\modules\LR11x0\LR11x0_commands.cpp"
if (Test-Path $RlCmd) {
    $rlText = Get-Content $RlCmd -Raw
    if ($rlText -notmatch 'ESPDEKA_MISO_MASK') {
        $rlText = $rlText -replace 'if\(device\)  \{ \*device = buff\[1\]; \}', 'if(device)  { *device = (uint8_t)(buff[1] & 0x0FU); } /* ESPDEKA_MISO_MASK */'
        Set-Content -Path $RlCmd -Value $rlText -NoNewline
        Write-Host "Patched RadioLib getVersion device mask for P-3 V1.6" -ForegroundColor Cyan
    }
}

Set-Location $FwDir
& $Py -m platformio run -e $EnvName
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$Bin = Join-Path $FwDir ".pio\build\$EnvName\firmware.bin"
Write-Host "OK: $Bin" -ForegroundColor Green
