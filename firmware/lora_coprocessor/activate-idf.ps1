# Load ESP-IDF into the current PowerShell session (makes idf.py available).
# Usage:  . .\activate-idf.ps1
$IdfRoot = "C:\Espressif\frameworks\esp-idf-v5.5.4"
$IdfExport = Join-Path $IdfRoot "export.ps1"
$IdfPython = "C:\Espressif\python_env\idf5.5_py3.11_env"

if (-not (Test-Path $IdfExport)) {
    Write-Error "ESP-IDF not found at $IdfExport — adjust activate-idf.ps1"
} elseif (-not (Test-Path $IdfPython)) {
    Write-Error "IDF Python env not found at $IdfPython — run $IdfRoot\install.ps1 esp32c3"
} else {
    $env:IDF_PYTHON_ENV_PATH = $IdfPython
    . $IdfExport
    Write-Host "ESP-IDF activated (Python 3.11 env): idf.py is on PATH" -ForegroundColor Green
}
