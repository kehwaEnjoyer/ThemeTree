#!/usr/bin/env bash

WALLPAPER_DIR="${HOME}/Wallpapers"
STATE_FILE="${HOME}/.config/themeTree/lastWallpaper"

mkdir -p "$(dirname "$STATE_FILE")"

# Find all images sorted alphabetically
mapfile -t WALLPAPERS < <(find "$WALLPAPER_DIR" -maxdepth 1 -type f \( -iname "*.png" -o -iname "*.jpg" -o -iname "*.jpeg" \) | sort)

TOTAL=${#WALLPAPERS[@]}

if [[ $TOTAL -eq 0 ]]; then
    echo "[Error] No wallpapers found in $WALLPAPER_DIR"
    exit 1
fi

# Get current wallpaper
LAST_WALL=""
[[ -f "$STATE_FILE" ]] && LAST_WALL=$(cat "$STATE_FILE")

if [[ "$1" == "--random" || "$1" == "-r" ]]; then
    if [[ $TOTAL -gt 1 ]]; then
        # Pick a random wallpaper that isn't the current one
        while :; do
            RAND_INDEX=$(( RANDOM % TOTAL ))
            NEXT_WALL="${WALLPAPERS[$RAND_INDEX]}"
            [[ "$NEXT_WALL" != "$LAST_WALL" ]] && break
        done
    else
        NEXT_WALL="${WALLPAPERS[0]}"
    fi
else
    # Sequential cycling
    NEXT_INDEX=0
    for i in "${!WALLPAPERS[@]}"; do
        if [[ "${WALLPAPERS[$i]}" == "$LAST_WALL" ]]; then
            NEXT_INDEX=$(( (i + 1) % TOTAL ))
            break
        fi
    done
    NEXT_WALL="${WALLPAPERS[$NEXT_INDEX]}"
fi

echo "$NEXT_WALL" > "$STATE_FILE"

# Hand off to themeTree-set.sh
themeTree-set "$NEXT_WALL"