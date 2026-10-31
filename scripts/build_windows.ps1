# Build the Windows x64 port.
#
#   powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1 -Rom C:\path\to\esprade.zip
#
# Requires Python 3.11+, CMake, Ninja, clang (or MSVC), and SDL2's VC development package.
param(
    [string]$Rom = "$PSScriptRoot\..\rom\esprade.zip",
    [string]$BuildDir = "$PSScriptRoot\..\build\cmake",
    [switch]$SkipRomTools,
    [switch]$NoSdl
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path "$PSScriptRoot\.."
Set-Location $Root

if (-not (Test-Path $Rom)) {
    throw "ROM not found: $Rom. Pass -Rom C:\path\to\esprade.zip"
}

if (-not $SkipRomTools) {
    python tools\verify_rom.py $Rom
    if ($LASTEXITCODE -ne 0) { throw "ROM verification failed" }
    python tools\seed_symbols.py $Rom
    if ($LASTEXITCODE -ne 0) { throw "symbol seeding failed" }
    python tools\disassemble.py $Rom
    if ($LASTEXITCODE -ne 0) { throw "static analysis failed" }
    python tools\recompile.py $Rom
    if ($LASTEXITCODE -ne 0) { throw "translation failed" }
}

$Sdl = "$Root\build\downloads\SDL2-2.32.10"
if (-not (Test-Path $Sdl)) {
    New-Item -ItemType Directory -Force "$Root\build\downloads" | Out-Null
    $zip = "$Root\build\downloads\SDL2-devel-VC.zip"
    Write-Host "Downloading SDL2..."
    Invoke-WebRequest -Uri "https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-VC.zip" -OutFile $zip
    Expand-Archive -Path $zip -DestinationPath "$Root\build\downloads" -Force
}

$cmakeArgs = @("-S", ".", "-B", $BuildDir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release")
if (Get-Command clang -ErrorAction SilentlyContinue) {
    $cmakeArgs += "-DCMAKE_C_COMPILER=clang"
}
if (-not $NoSdl) {
    $cmakeArgs += "-DCMAKE_PREFIX_PATH=$Sdl"
}
cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
cmake --build $BuildDir
if ($LASTEXITCODE -ne 0) { throw "build failed" }

Write-Host "Headless: $BuildDir\esprade_headless.exe"
Write-Host "Windowed: $BuildDir\esprade.exe (SDL2.dll is copied next to it)"
