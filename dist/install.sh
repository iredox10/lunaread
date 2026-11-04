#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "=== Installing LunaRead Desktop ==="

# Check for RPM install option on Fedora/RHEL/openSUSE
if command -v dnf &>/dev/null && [ -f "${SCRIPT_DIR}/lunaread-desktop-1.0.0-Linux.rpm" ]; then
    echo "RPM package detected. You can install system-wide with:"
    echo "  sudo dnf install ${SCRIPT_DIR}/lunaread-desktop-1.0.0-Linux.rpm"
    echo ""
fi

# Determine target directories (system-wide if root, user-local otherwise)
if [ "$(id -u)" -eq 0 ]; then
    BIN_DIR="/usr/local/bin"
    APPS_DIR="/usr/share/applications"
    ICONS_DIR="/usr/share/icons/hicolor/scalable/apps"
else
    BIN_DIR="${HOME}/.local/bin"
    APPS_DIR="${HOME}/.local/share/applications"
    ICONS_DIR="${HOME}/.local/share/icons/hicolor/scalable/apps"
fi

mkdir -p "${BIN_DIR}" "${APPS_DIR}" "${ICONS_DIR}"

echo "Installing binary to ${BIN_DIR}/lunaread-desktop..."
install -m 755 "${SCRIPT_DIR}/lunaread-desktop" "${BIN_DIR}/lunaread-desktop"

if [ -f "${ROOT_DIR}/assets/icons/lunaread.svg" ]; then
    echo "Installing icon to ${ICONS_DIR}/lunaread.svg..."
    install -m 644 "${ROOT_DIR}/assets/icons/lunaread.svg" "${ICONS_DIR}/lunaread.svg"
fi

if [ -f "${SCRIPT_DIR}/LunaRead.desktop" ]; then
    echo "Installing desktop launcher to ${APPS_DIR}/LunaRead.desktop..."
    install -m 644 "${SCRIPT_DIR}/LunaRead.desktop" "${APPS_DIR}/LunaRead.desktop"
fi

if command -v update-desktop-database &>/dev/null; then
    update-desktop-database "${APPS_DIR}" 2>/dev/null || true
fi

echo ""
echo "=== LunaRead installed successfully! ==="
echo "You can now launch LunaRead from your application launcher or run:"
echo "  ${BIN_DIR}/lunaread-desktop"
