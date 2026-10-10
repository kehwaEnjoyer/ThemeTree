#!/usr/bin/env bash

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${HOME}/.local/bin"
BASHRC="${HOME}/.bashrc"

echo "==> Setting executable permissions..."
chmod +x "${PROJECT_ROOT}/scripts/themeTreeDaemon.sh"
chmod +x "${PROJECT_ROOT}/scripts/themeTree-set.sh"
chmod +x "${PROJECT_ROOT}/scripts/themeTree-next.sh"
if [[ -f "${PROJECT_ROOT}/bin/themeTree" ]]; then
    chmod +x "${PROJECT_ROOT}/bin/themeTree"
fi

echo "==> Symlinking into ${BIN_DIR}..."
mkdir -p "$BIN_DIR"

# 1. Link C++ engine binary from bin/
if [[ -f "${PROJECT_ROOT}/bin/themeTree" ]]; then
    ln -sf "${PROJECT_ROOT}/bin/themeTree" "${BIN_DIR}/themeTree"
else
    echo "[!] Warning: ${PROJECT_ROOT}/bin/themeTree not found. Compile CMake project first!"
fi

# 2. Link shell scripts from scripts/
ln -sf "${PROJECT_ROOT}/scripts/themeTreeDaemon.sh" "${BIN_DIR}/themetreedaemon"
ln -sf "${PROJECT_ROOT}/scripts/themeTree-set.sh" "${BIN_DIR}/themeTree-set"
ln -sf "${PROJECT_ROOT}/scripts/themeTree-next.sh"   "${BIN_DIR}/themeTree-next"

echo "==> Updating PATH in ${BASHRC}..."

# Ensure ~/.local/bin is in PATH for shell & launcher detection
if ! grep -q 'HOME/.local/bin' "$BASHRC" && [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
    echo '' >> "$BASHRC"
    echo '# Added by ThemeTree setup' >> "$BASHRC"
    echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$BASHRC"
fi

echo "==> Setup complete!"
echo "    - C++ Engine:  'themeTree'"
echo "    - Daemon:      'themetreedaemon'"
echo "    - Switcher:    'themeTree-set'"