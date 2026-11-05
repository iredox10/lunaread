#!/usr/bin/env bash
set -euo pipefail

echo "==================================================="
echo "    LunaRead Desktop - macOS Build & DMG Packager"
echo "==================================================="

# Check for Homebrew Qt6 if CMAKE_PREFIX_PATH not set
if [ -z "${CMAKE_PREFIX_PATH:-}" ]; then
    if command -v brew &>/dev/null; then
        QT_PREFIX="$(brew --prefix qt@6 2>/dev/null || brew --prefix qt 2>/dev/null || true)"
        if [ -n "$QT_PREFIX" ]; then
            export CMAKE_PREFIX_PATH="$QT_PREFIX"
            echo "[*] Found Qt via Homebrew at: $QT_PREFIX"
        fi
    fi
fi

mkdir -p build
cd build

echo "[*] Configuring with CMake..."
cmake -DCMAKE_BUILD_TYPE=Release \
      ${CMAKE_PREFIX_PATH:+-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH"} \
      ..

echo "[*] Compiling LunaRead.app..."
cmake --build . --config Release -j "$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

# Locate .app bundle
APP_BUNDLE=""
if [ -d "lunaread-desktop.app" ]; then
    APP_BUNDLE="lunaread-desktop.app"
elif [ -d "LunaRead.app" ]; then
    APP_BUNDLE="LunaRead.app"
fi

if [ -n "$APP_BUNDLE" ]; then
    echo "[*] Bundling Qt dependencies and creating DMG with macdeployqt..."
    macdeployqt "$APP_BUNDLE" -dmg
    echo ""
    echo "==================================================="
    echo "[SUCCESS] macOS .dmg installer generated in build/!"
    echo "Users can open the DMG and drag LunaRead to Applications."
    echo "==================================================="
else
    echo "[*] Generating CPack DragNDrop DMG..."
    cpack -G DragNDrop
fi

cd ..
