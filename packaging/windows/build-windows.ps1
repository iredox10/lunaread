# LunaRead Desktop - Windows Build Script (PowerShell)
param (
    [string]$QtDir = "",
    [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"
Write-Host "===================================================" -ForegroundColor Cyan
Write-Host "    LunaRead Desktop - Windows Build & Package" -ForegroundColor Cyan
Write-Host "===================================================" -ForegroundColor Cyan

# Attempt to locate Qt6 if not explicitly provided
if (-not $QtDir -and $env:QTDIR) {
    $QtDir = $env:QTDIR
}

$CMakeArgs = @("-DCMAKE_BUILD_TYPE=$BuildType")
if ($QtDir) {
    Write-Host "[*] Using Qt from: $QtDir" -ForegroundColor Green
    $CMakeArgs += "-DCMAKE_PREFIX_PATH=$QtDir"
}

if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Push-Location "build"

Write-Host "[*] Configuring CMake..." -ForegroundColor Yellow
cmake @CMakeArgs ..

Write-Host "[*] Building LunaRead..." -ForegroundColor Yellow
cmake --build . --config $BuildType -j $env:NUMBER_OF_PROCESSORS

# Locate executable
$ExePath = ""
if (Test-Path "Release\lunaread-desktop.exe") {
    $ExePath = "Release\lunaread-desktop.exe"
} elseif (Test-Path "lunaread-desktop.exe") {
    $ExePath = "lunaread-desktop.exe"
}

if ($ExePath) {
    Write-Host "[*] Deploying Qt dependencies using windeployqt..." -ForegroundColor Yellow
    & windeployqt --release --no-translations $ExePath
}

Write-Host "[*] Generating Windows Installer and ZIP package..." -ForegroundColor Yellow
cpack -C $BuildType

Pop-Location
Write-Host "[SUCCESS] Windows build and packages generated in build/ folder." -ForegroundColor Green
