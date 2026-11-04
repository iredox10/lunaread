#!/usr/bin/env bash
# Portable launch wrapper for LunaRead
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export QT_LOGGING_RULES="*.debug=false;qt.qpa.*=false"
exec "${SCRIPT_DIR}/lunaread-desktop" "$@"
