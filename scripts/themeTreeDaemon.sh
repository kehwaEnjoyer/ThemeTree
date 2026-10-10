#!/usr/bin/env bash

# Check required dependencies
# Locate script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Resolve themeTree executable location
if [[ -n "$THEMETREE_BIN" && -x "$THEMETREE_BIN" ]]; then
    THEMETREE_EXEC="$THEMETREE_BIN"
elif [[ -x "${PROJECT_ROOT}/bin/themeTree" ]]; then
    THEMETREE_EXEC="${PROJECT_ROOT}/bin/themeTree"
elif command -v themeTree &>/dev/null; then
    THEMETREE_EXEC="$(command -v themeTree)"
else
    echo "[Error] 'themeTree' executable not found in ${PROJECT_ROOT}/bin/ or PATH."
    exit 1
fi

# Check remaining tools
for cmd in jq inotifywait; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "[Error] Required command '$cmd' is not installed or not in PATH."
        exit 1
    fi
done

WALLPAPER_DIR="${HOME}/Wallpapers"
TEMPLATE_DIR="${HOME}/.config/themeTree/themeTreePalettes/templates"
CACHE_DIR="${HOME}/.config/themeTree/themeTreePalettes/palettes"

mkdir -p "$CACHE_DIR" "$TEMPLATE_DIR"

render_templates() {
    local json_file="$1"
    local out_dir="$2"
    mkdir -p "$out_dir"

    if [[ ! -s "$json_file" ]]; then
        echo "[ThemeCache] Warning: $json_file is missing or empty."
        return 1
    fi

    # Extract key-value pairs from .colors (or root fallback)
    local sed_args=()
    while IFS="=" read -r key val; do
        [[ -n "$key" && -n "$val" ]] || continue
        
        local val_raw="${val#\#}"

        # 1. Match {{color0}} and {{color0_raw}}
        sed_args+=("-e" "s|{{${key}}}|${val}|g")
        sed_args+=("-e" "s|{{${key}_raw}}|${val_raw}|g")

        # 2. Also match {{0}} and {{0_raw}} as short forms
        if [[ "$key" =~ ^color([0-9]+)$ ]]; then
            local num="${BASH_REMATCH[1]}"
            sed_args+=("-e" "s|{{${num}}}|${val}|g")
            sed_args+=("-e" "s|{{${num}_raw}}|${val_raw}|g")
        fi
    done < <(jq -r '(.colors // .) | to_entries[] | "\(.key)=\(.value)"' "$json_file" 2>/dev/null)

    if [[ ${#sed_args[@]} -eq 0 ]]; then
        echo "[ThemeCache] Error: Failed to parse colors out of $json_file"
        return 1
    fi

    # Render all templates in TEMPLATE_DIR
    for tpl in "${TEMPLATE_DIR}"/*.tpl; do
        [[ -f "$tpl" ]] || continue
        local tpl_name
        tpl_name=$(basename -- "$tpl")
        local target_name="${tpl_name%.tpl}"
        
        sed "${sed_args[@]}" "$tpl" > "${out_dir}/${target_name}"
    done
}

process_wallpaper() {
    local img="$1"
    local filename
    filename=$(basename -- "$img")
    local name="${filename%.*}"
    
    local wall_cache_dir="${CACHE_DIR}/${name}"
    local json_out="${wall_cache_dir}/colors.json"

    mkdir -p "$wall_cache_dir"

    local run_engine=false
    local run_templates=false

    # 1. Check if C++ engine needs to run (missing JSON or image updated)
    if [[ ! -f "$json_out" || "$img" -nt "$json_out" ]]; then
        run_engine=true
        run_templates=true
    else
        # 2. Check if any template is missing or newer than its rendered file
        for tpl in "${TEMPLATE_DIR}"/*.tpl; do
            [[ -f "$tpl" ]] || continue
            local tpl_name
            tpl_name=$(basename -- "$tpl")
            local target_name="${tpl_name%.tpl}"
            local target_file="${wall_cache_dir}/${target_name}"

            if [[ ! -f "$target_file" || "$tpl" -nt "$target_file" ]]; then
                run_templates=true
                break
            fi
        done
    fi

    # Run C++ engine if needed
    if [[ "$run_engine" == true ]]; then
        echo "[ThemeCache] Extracting palette for: ${filename}"
        "$THEMETREE_EXEC" "$img" "${wall_cache_dir}/colors.css" "$json_out" --sort=hybrid
    fi

    # Run template renderer if needed
    if [[ "$run_templates" == true ]]; then
        echo "[ThemeCache] Rendering templates for: ${filename}"
        render_templates "$json_out" "$wall_cache_dir"
    fi
}

# Initial pass on start
find "$WALLPAPER_DIR" -type f \( -iname "*.png" -o -iname "*.jpg" -o -iname "*.jpeg" \) -print0 | while IFS= read -r -d '' img; do
    process_wallpaper "$img"
done

echo "[ThemeCache] Pre-caching complete. Watching for directory changes..."

# Real-time watcher
inotifywait -m -e close_write,moved_to,create "$WALLPAPER_DIR" --format "%w%f" | while read -r new_img; do
    case "$new_img" in
        *.png|*.jpg|*.jpeg|*.PNG|*.JPG|*.JPEG)
            process_wallpaper "$new_img"
            ;;
    esac
done