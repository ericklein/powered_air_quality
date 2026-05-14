# Configuration
$SKETCH = "."
$BOARD = "esp32:esp32:esp32:PartitionScheme=min_spiffs"
$BUILD_DIR = "./build"
$LIB_PATH = "$env:USERPROFILE\Dropbox\make\arduino\libraries"
$PORT  = "COM9"


# Ensure directories exist
if (!(Test-Path $BUILD_DIR)) { New-Item -ItemType Directory -Path $BUILD_DIR }

Write-Host "--- Starting Ultra-Fast Windows Compile with library discovery---" -ForegroundColor Cyan

# Execute arduino-cli
# --jobs 0 uses all CPU cores
arduino-cli compile --fqbn $BOARD `
  --jobs 0 `
  --libraries $LIB_PATH `
  --build-path $BUILD_DIR `
  -v `
  $SKETCH

# Check exit code ($?)
if ($LASTEXITCODE -eq 0) {
    Write-Host "--- Compile Success! ---" -ForegroundColor Green
} else {
    Write-Host "--- Compile Failed ---" -ForegroundColor Red
    Write-Host "Tip: Check library path, FQBN, and compile output above."
}