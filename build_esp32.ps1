# Configuration
$SKETCH = "powered_air_quality.ino"
$BOARD = "esp32:esp32:esp32:PartitionScheme=min_spiffs"
$BUILD_DIR = "./build"
$CACHE_DIR = "./cache"
$LIB_PATH = "$env:USERPROFILE\Dropbox\make\arduino\libraries"


# Ensure directories exist
if (!(Test-Path $BUILD_DIR)) { New-Item -ItemType Directory -Path $BUILD_DIR }
if (!(Test-Path $CACHE_DIR)) { New-Item -ItemType Directory -Path $CACHE_DIR }

Write-Host "--- Starting Ultra-Fast Windows Compile ---" -ForegroundColor Cyan

# Execute arduino-cli
# --jobs 0 uses all CPU cores
arduino-cli compile --fqbn $BOARD `
  --jobs 0 `
  --libraries $LIB_PATH `
  --build-path $BUILD_DIR `
  --build-cache-path $CACHE_DIR `
  -v `
  --skip-libraries-discovery `
  $SKETCH

# Check exit code ($?)
if ($LASTEXITCODE -eq 0) {
    Write-Host "--- Compile Success! ---" -ForegroundColor Green
} else {
    Write-Host "--- Compile Failed ---" -ForegroundColor Red
    Write-Host "Tip: If a library is missing, try removing --skip-libraries-discovery once."
}