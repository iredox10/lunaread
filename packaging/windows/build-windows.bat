@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo     LunaRead Desktop - Windows Build ^& Package
echo ===================================================

:: Check for CMake
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] CMake is not found in PATH. Please install CMake or run from Developer Command Prompt.
    pause
    exit /b 1
)

:: Configure build folder
if not exist "build" mkdir build
cd build

echo.
echo [*] Configuring with CMake...
cmake -DCMAKE_BUILD_TYPE=Release ..
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b 1
)

echo.
echo [*] Compiling LunaRead Desktop...
cmake --build . --config Release -j%NUMBER_OF_PROCESSORS%
if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed.
    pause
    exit /b 1
)

echo.
echo [*] Deploying Qt runtime DLLs with windeployqt...
if exist "Release\lunaread-desktop.exe" (
    windeployqt --release --no-translations Release\lunaread-desktop.exe
) else if exist "lunaread-desktop.exe" (
    windeployqt --release --no-translations lunaread-desktop.exe
)

echo.
echo [*] Packaging installer and portable ZIP with CPack...
cpack -C Release
if %errorlevel% neq 0 (
    echo [WARNING] CPack encountered an error (check if NSIS is installed for .exe setup).
)

echo.
echo ===================================================
echo Build completed! Packages are located in the build directory:
echo   - NSIS Setup Installer (.exe)
echo   - Portable standalone archive (.zip)
echo ===================================================
cd ..
pause
