#!/usr/bin/env bash

if [[ -z "$1" ]]; then
    echo "Usage: themeTree-set.sh /path/to/wallpaper.png"
    exit 1
fi

WALLPAPER="$(realpath "$1")"
filename=$(basename -- "$WALLPAPER")
name="${filename%.*}"

WALL_CACHE_DIR="${HOME}/.config/themeTree/themeTreePalettes/palettes/${name}"
ACTIVE_DIR="${HOME}/.config/themeTree/active"

mkdir -p ${ACTIVE_DIR}

if [[ ! -d "$WALL_CACHE_DIR" ]]; then
    echo "[SetTheme] Cache miss for ${filename}. Waiting for daemon..."
    exit 1
fi

# 1. Symlink every pre-rendered template output to ~/.cache/themeTree/active/
for generated_file in "${WALL_CACHE_DIR}"/*; do
    [[ -f "$generated_file" ]] || continue
    fname=$(basename -- "$generated_file")
    ln -sf "$generated_file" "${ACTIVE_DIR}/${fname}"
done

# 2. Update Wallpaper via hyprpaper
if command -v hyprpaper &>/dev/null; then
    hyprctl hyprpaper preload "$WALLPAPER" >/dev/null
    for monitor in $(hyprctl monitors | grep "Monitor" | awk '{print $2}'); do
        hyprctl hyprpaper wallpaper "$monitor,$WALLPAPER" >/dev/null
    done
    hyprctl hyprpaper unload all >/dev/null
fi

# 3. Reload Desktop Components
hyprctl reload &>/dev/null
killall -SIGUSR1 waybar 2>/dev/null
# Push live color changes across all open Kitty terminal windows
kitty @ set-colors -a "${ACTIVE_DIR}/kitty.conf" 2>/dev/null || killall -SIGUSR1 kitty 2>/dev/null

echo "[SetTheme] Successfully switched to ${filename}"