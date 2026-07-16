# Analyze Arduino / ESP32 linker map file to show allocated memory usage by library.
# Usage: .\analyze_map_improved.ps1 [-MapFile c:\arduinobuild\output.map] [-Top 0] [-Csv out.csv]
#
# Notes:
# - This parses the real "Linker script and memory map" section, not the earlier
#   archive-reference or discarded-section lists.
# - It handles ESP32 archive members such as libfoo.a(bar.c.obj) and normal
#   Arduino library object files such as libraries\FastLED\FastLED.cpp.o.
# - It separates Flash, IRAM, and DRAM because ESP32 places some executable code
#   in internal RAM rather than ordinary flash sections.
# - Debug/non-runtime map sections are ignored by default; pass -IncludeOther to inspect them.

param(
    [string]$MapFile = "c:\arduinobuild\output.map",
    [int]$Top = 0,
    [string]$Csv = "",
    [switch]$IncludeOther
)

if (!(Test-Path $MapFile)) {
    Write-Host "Map file not found: $MapFile" -ForegroundColor Red
    exit 1
}

Write-Host "Analyzing: $MapFile" -ForegroundColor Cyan
Write-Host ""

# Per-library accumulators.
$flashByLib = @{}
$iramByLib  = @{}
$dramByLib  = @{}
$otherByLib = @{}

# Optional detail table for CSV export.
$rows = New-Object System.Collections.Generic.List[object]

function Add-Size($table, [string]$lib, [Int64]$size) {
    if ($size -le 0) { return }
    if (-not $table.ContainsKey($lib)) { $table[$lib] = [Int64]0 }
    $table[$lib] += $size
}

function Get-LibraryName([string]$objPath) {
    $p = $objPath.Trim()

    # Strip any trailing symbol note, just in case a map variant emits one.
    $p = $p -replace '\s+\([^)]*\)$', ''

    if ($p -match '\\libraries\\([^\\]+)\\') {
        return $matches[1]
    }
    if ($p -match '\\sketch\\') {
        return 'sketch'
    }
    if ($p -match '\\core\\') {
        return 'core'
    }

    # ESP-IDF archive library, e.g. ...\lib\libesp_rom.a(esp_rom_gpio.c.obj)
    if ($p -match '\\lib([^\\]+)\.a\(') {
        return 'esp-idf:' + $matches[1]
    }

    # Any other archive member.
    if ($p -match '\\([^\\]+)\.a\(') {
        return $matches[1]
    }

    return 'other'
}

function Get-Bucket([string]$outputSection, [string]$inputSection) {
    $out = $outputSection.ToLowerInvariant()
    $inp = $inputSection.ToLowerInvariant()

    # ESP32 executable RAM. Keep separate from ordinary flash because it consumes IRAM.
    if ($out -match '^\.iram' -or $out -match '^\.iram0' -or
        $inp -match '^\.iram' -or $inp -match '^\.dram0\.text') {
        return 'IRAM'
    }

    # ESP32 data RAM.
    if ($out -match '^\.dram' -or $out -match '^\.dram0' -or
        $out -match '^\.data' -or $out -match '^\.bss' -or $out -match '^\.noinit' -or
        $inp -match '^\.dram0\.data' -or $inp -match '^\.dram0\.bss' -or
        $inp -match '^\.data' -or $inp -match '^\.bss' -or $inp -match '^\.noinit') {
        return 'DRAM'
    }

    # Flash-resident code/constants.
    if ($out -match '^\.flash' -or $out -match '^\.rodata' -or $out -match '^\.text' -or
        $inp -match '^\.flash' -or $inp -match '^\.literal' -or $inp -match '^\.rodata' -or $inp -match '^\.text') {
        return 'FLASH'
    }

    return 'OTHER'
}

function Format-Size([Int64]$bytes) {
    if ($bytes -ge 1MB) { return '{0:N2} MB' -f ($bytes / 1MB) }
    if ($bytes -ge 1KB) { return '{0:N2} KB' -f ($bytes / 1KB) }
    return "$bytes B"
}

function Show-Table([string]$title, $table, [ConsoleColor]$color) {
    Write-Host "=== $title ===" -ForegroundColor $color
    Write-Host ('-' * 58)

    $total = [Int64]0
    $items = $table.GetEnumerator() | Sort-Object Value -Descending
    if ($Top -gt 0) { $items = $items | Select-Object -First $Top }

    foreach ($item in $items) {
        $total += [Int64]$item.Value
        Write-Host ('{0,-36} {1,15}' -f $item.Key, (Format-Size ([Int64]$item.Value)))
    }

    $realTotal = [Int64]0
    foreach ($v in $table.Values) { $realTotal += [Int64]$v }

    Write-Host ('-' * 58)
    if ($Top -gt 0) {
        Write-Host ('{0,-36} {1,15}' -f "SHOWN TOP $Top", (Format-Size $total)) -ForegroundColor Green
        Write-Host ('{0,-36} {1,15}' -f 'TOTAL', (Format-Size $realTotal)) -ForegroundColor Green
    } else {
        Write-Host ('{0,-36} {1,15}' -f 'TOTAL', (Format-Size $realTotal)) -ForegroundColor Green
    }
    Write-Host ""
}

$inMemoryMap = $false
$currentOutputSection = ''
$pendingInputSection = ''

# Matches an output-section header, for example:
# .flash.text     0x400d0020    0xed014
$outputSectionPattern = '^([.]\S+)\s+0x[0-9a-fA-F]+\s+0x[0-9a-fA-F]+(?:\s|$)'

# Matches one-line input section entries, for example:
#  .text.foo      0x400d1234      0x40 C:\...\file.cpp.o
#  .text.foo      0x400d1234      0x40 C:\...\libfoo.a(bar.c.obj)
$oneLineInputPattern = '^\s+([.]\S+)\s+0x[0-9a-fA-F]+\s+(0x[0-9a-fA-F]+)\s+(.+(?:\.o|\.obj)(?:\))?)\s*$'

# Matches the continuation line used when the input section name is too long:
#  .text.very_long_symbol_name
#                 0x400d1234      0x40 C:\...\file.cpp.o
$continuedInputPattern = '^\s+0x[0-9a-fA-F]+\s+(0x[0-9a-fA-F]+)\s+(.+(?:\.o|\.obj)(?:\))?)\s*$'
$inputSectionOnlyPattern = '^\s+([.]\S+)\s*$'

Get-Content $MapFile | ForEach-Object {
    $line = $_

    if (-not $inMemoryMap) {
        if ($line -match '^Linker script and memory map') {
            $inMemoryMap = $true
        }
        return
    }

    if ($line -match $outputSectionPattern -and $line -notmatch '^\s') {
        $currentOutputSection = $matches[1]
        $pendingInputSection = ''
        return
    }

    $inputSection = $null
    $sizeHex = $null
    $objPath = $null

    if ($line -match $oneLineInputPattern) {
        $inputSection = $matches[1]
        $sizeHex = $matches[2]
        $objPath = $matches[3]
        $pendingInputSection = ''
    } elseif ($pendingInputSection -ne '' -and $line -match $continuedInputPattern) {
        $inputSection = $pendingInputSection
        $sizeHex = $matches[1]
        $objPath = $matches[2]
        $pendingInputSection = ''
    } elseif ($line -match $inputSectionOnlyPattern) {
        $pendingInputSection = $matches[1]
        return
    } else {
        $pendingInputSection = ''
        return
    }

    $size = [Convert]::ToInt64($sizeHex, 16)
    if ($size -le 0) { return }

    $lib = Get-LibraryName $objPath
    $bucket = Get-Bucket $currentOutputSection $inputSection

    switch ($bucket) {
        'FLASH' { Add-Size $flashByLib $lib $size }
        'IRAM'  { Add-Size $iramByLib  $lib $size }
        'DRAM'  { Add-Size $dramByLib  $lib $size }
        default { if ($IncludeOther) { Add-Size $otherByLib $lib $size } }
    }

    if ($Csv -ne '' -and ($bucket -ne 'OTHER' -or $IncludeOther)) {
        $rows.Add([pscustomobject]@{
            Bucket = $bucket
            Library = $lib
            Size = $size
            OutputSection = $currentOutputSection
            InputSection = $inputSection
            Object = $objPath.Trim()
        }) | Out-Null
    }
}

Show-Table 'FLASH Usage by Library' $flashByLib Yellow
Show-Table 'IRAM Usage by Library'  $iramByLib  Magenta
Show-Table 'DRAM Usage by Library'  $dramByLib  Yellow

if ($IncludeOther -and $otherByLib.Count -gt 0) {
    Show-Table 'OTHER / Non-runtime Sections by Library' $otherByLib DarkGray
}

if ($Csv -ne '') {
    $rows | Export-Csv -NoTypeInformation -Path $Csv
    Write-Host "CSV detail written to: $Csv" -ForegroundColor Cyan
}
