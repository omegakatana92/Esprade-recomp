# Run the ROM-dependent checks locally.
#   powershell -ExecutionPolicy Bypass -File tests\run_all.ps1 -Rom C:\path\to\esprade.zip
param(
    [string]$Rom = "$PSScriptRoot\..\rom\esprade.zip",
    [string]$BuildDir = "$PSScriptRoot\..\build\cmake"
)
$ErrorActionPreference = "Stop"
$Root = Resolve-Path "$PSScriptRoot\.."
Set-Location $Root

if (-not (Test-Path $Rom)) { throw "ROM not found: $Rom" }

python tools\verify_rom.py $Rom
if ($LASTEXITCODE -ne 0) { throw "ROM verification failed" }

python tools\seed_symbols.py $Rom
if ($LASTEXITCODE -ne 0) { throw "symbol seeding failed" }
python tools\seed_prologues.py $Rom
python tools\seed_traced.py
python tools\disassemble.py $Rom
if ($LASTEXITCODE -ne 0) { throw "analysis failed" }
python tools\recompile.py $Rom
if ($LASTEXITCODE -ne 0) { throw "translation failed" }

cmake --build $BuildDir
if ($LASTEXITCODE -ne 0) { throw "build failed" }

$headless = Join-Path $BuildDir "esprade_headless.exe"
if (-not (Test-Path $headless)) { $headless = Join-Path $BuildDir "esprade_headless" }

& $headless --rom $Rom --frames 600 --hashes build\boot.hashes
if ($LASTEXITCODE -ne 0) { throw "600-frame boot failed" }

& $headless --rom $Rom --frames 60000 --coin 2400 --start 2600 --png build\game30000.png 30000
if ($LASTEXITCODE -ne 0) { throw "60000-frame run failed" }

& $headless --rom $Rom --frames 6000 --coin 2400 --start 2600 --wav build\audio.wav
if ($LASTEXITCODE -ne 0) { throw "audio run failed" }

Write-Host "All headless checks passed."
