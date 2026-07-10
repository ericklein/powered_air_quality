# Configuration
$SKETCH = "."
$BOARD = "esp32:esp32:esp32:PartitionScheme=min_spiffs"
$BUILD_DIR = "c:\arduinobuild"
$LIB_PATH = "$env:USERPROFILE\Dropbox\make\arduino\libraries"

Write-Host "Detecting available boards..." -ForegroundColor Cyan
arduino-cli board list

do {
    $portNumber = Read-Host "Enter COM port number (e.g., 9)"
} while (-not ($portNumber -match '^\d+$'))

$PORT = "COM$portNumber"


# Ensure directories exist
if (!(Test-Path $BUILD_DIR)) { New-Item -ItemType Directory -Path $BUILD_DIR }

Write-Host "--- Starting Ultra-Fast Windows Compile with library discovery---" -ForegroundColor Cyan

# Execute arduino-cli
# --jobs 0 uses all CPU cores
arduino-cli compile --fqbn $BOARD `
  --jobs 0 `
  --libraries $LIB_PATH `
  --build-path $BUILD_DIR `
  --build-property "compiler.c.elf.extra_flags=-Wl,-Map,$BUILD_DIR/output.map" `
  -v `
  $SKETCH

# Check exit code ($?)
if ($LASTEXITCODE -eq 0) {
    Write-Host "--- Compile Success! ---" -ForegroundColor Green
    arduino-cli upload `
        -p $PORT `
        --fqbn $BOARD `
        --input-dir $BUILD_DIR
} else {
    Write-Host "--- Compile Failed ---" -ForegroundColor Red
    Write-Host "Tip: Check library path, FQBN, and compile output above."
}