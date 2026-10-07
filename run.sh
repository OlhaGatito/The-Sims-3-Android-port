#!/bin/bash
# The Sims 3 — internal runtime with multi-CFW support
# Supports: muOS, ArkOS, ROCKNIX, NextOS, PortMaster e Linux ARM genérico
set +u

# ============================================================
# EARLY OUTPUT (visible on console before any redirection)
# ============================================================
echo "=== The Sims 3 runtime starting ==="
GAMEDIR="$(CDPATH= cd -- "$(dirname -- "$0")" 2>/dev/null && pwd -P)" || { echo "[FATAL] Cannot determine GAMEDIR"; exit 1; }
cd "$GAMEDIR" || { echo "[FATAL] Cannot cd to $GAMEDIR"; exit 1; }
echo "GAMEDIR=$GAMEDIR"

# ============================================================
# LOG SETUP (with fallback to /tmp)
# ============================================================
LOGDIR="${SIMS3_LOG_DIR:-$GAMEDIR/logs}"
mkdir -p "$LOGDIR" 2>/dev/null || LOGDIR="${TMPDIR:-/tmp}"
LOG="${SIMS3_LOG:-$LOGDIR/debug.log}"
touch "$LOG" 2>/dev/null || LOG="${TMPDIR:-/tmp}/sims3-debug.log"
echo "LOG=$LOG"

# NOW redirect all output to log (after critical setup)
exec >>"$LOG" 2>&1
echo "=== The Sims 3 runtime (redirected) ==="
echo "START_TIME=$(date 2>/dev/null || true)"

# ============================================================
# CFW DETECTION
# ============================================================
if [ -z "${CFW_NAME:-}" ]; then
    CFW_NAME=unknown
    [ -f /opt/muos/bin/muos-version ] && CFW_NAME=muos
    { [ -d /opt/system/bin ] && [ -f "/opt/system/Advanced/Firmware Version.txt" ]; } && CFW_NAME=arkos
    [ -f /opt/bin/emulationstation ] && CFW_NAME=rocknix
    grep -qi "nextos" /etc/os-release 2>/dev/null && CFW_NAME=nextos
    # ROCKNIX/Aurknix detection via os-release or directory structure
    [ "$CFW_NAME" = "unknown" ] && grep -qi "rocknix" /etc/os-release 2>/dev/null && CFW_NAME=rocknix
    [ "$CFW_NAME" = "unknown" ] && grep -qi "aurknix" /etc/os-release 2>/dev/null && CFW_NAME=rocknix
    [ "$CFW_NAME" = "unknown" ] && [ -d /storage/roms ] && [ -f /opt/bin/emulationstation ] && CFW_NAME=rocknix
fi
export CFW_NAME

# ============================================================
# ARCH DETECTION
# ============================================================
ARCH="${DEVICE_ARCH:-$(uname -m)}"
export DEVICE_ARCH="$ARCH"
echo "CFW=$CFW_NAME"
echo "DEVICE=${DEVICE_NAME:-$(uname -n 2>/dev/null || echo unknown)}"
echo "ARCH=$ARCH"
echo "DATE=$(date 2>/dev/null || true)"

# ============================================================
# AUDIO/VIDEO COMPATIBILITY
# ============================================================
if [ -f "$GAMEDIR/port_compat.sh" ]; then
    . "$GAMEDIR/port_compat.sh"
    port_detect_runtime 2>/dev/null || true
fi

GAME_DIR="$GAMEDIR/game"
GAME_IMAGE="$GAME_DIR/game.s3e.unpacked"
ASSET_DIR="$GAME_DIR/assets"

# ============================================================
# LOADER SELECTION (by kernel arch)
# ============================================================
select_loader() {
    _arch="$1"
    case "$_arch" in
        aarch64|arm64)
            if [ -x "$GAMEDIR/sims3_s3e_loader_aarch64" ]; then
                echo "$GAMEDIR/sims3_s3e_loader_aarch64"
                return 0
            fi
            echo "$GAMEDIR/sims3_s3e_loader"
            return 0
            ;;
        arm*|aarch32)
            echo "$GAMEDIR/sims3_s3e_loader"
            return 0
            ;;
        *)
            echo "$GAMEDIR/sims3_s3e_loader"
            return 0
            ;;
    esac
}
LOADER="$(select_loader "$ARCH")"
echo "LOADER=$LOADER (arch=$ARCH)"

# ============================================================
# VERIFY LOADER EXISTS AND IS EXECUTABLE (critical!)
# ============================================================
if [ ! -f "$LOADER" ]; then
    echo "[FATAL] Loader not found: $LOADER"
    echo "[HINT] Expected: $GAMEDIR/sims3_s3e_loader (ARMv7) or $GAMEDIR/sims3_s3e_loader_aarch64 (AArch64)"
    ls -la "$GAMEDIR"/sims3_s3e_loader* 2>/dev/null || echo "No loader files in $GAMEDIR"
    exit 1
fi
if [ ! -x "$LOADER" ]; then
    chmod +x "$LOADER" 2>/dev/null || true
fi
if [ ! -x "$LOADER" ]; then
    echo "[FATAL] Loader not executable: $LOADER"
    exit 126
fi
echo "[OK] Loader verified: $LOADER"

# ============================================================
# 32-bit COMPAT CHECK (for ARMv7 loader on AArch64 host)
# ============================================================
if [ "$ARCH" = "aarch64" ]; then
    if command -v readelf >/dev/null 2>&1; then
        INTERP=$(readelf -l "$LOADER" 2>/dev/null | sed -n 's/.*Requesting program interpreter: \([^]]*\).*/\1/p')
        if [ -n "$INTERP" ] && [ ! -f "$INTERP" ]; then
            echo "[WARN] Loader 32-bit requires interpreter $INTERP (not found)"
            for p in /lib/ld-linux-armhf.so.3 /lib/arm-linux-gnueabihf/ld-linux-armhf.so.3 /usr/lib/arm-linux-gnueabihf/ld-linux-armhf.so.3; do
                [ -f "$p" ] && { echo "[OK] Found: $p"; break; }
            done
            export LD_LIBRARY_PATH="/lib/arm-linux-gnueabihf:/usr/lib/arm-linux-gnueabihf:/lib:/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        fi
    else
        echo "[WARN] readelf not available, skipping interpreter check"
    fi
fi

# ============================================================
# REUSE EXISTING EXTRACTED DATA (handles dual mount paths)
# ============================================================
reuse_existing_game_data() {
    [ -f "$GAME_IMAGE" ] && [ -d "$ASSET_DIR" ] && return 0
    
    # Verifica integridade do game.s3e.unpacked local
    if [ -f "$GAME_IMAGE" ] && [ -s "$GAME_IMAGE" ]; then
        local file_size=$(wc -c < "$GAME_IMAGE" 2>/dev/null || echo 0)
        if [ "$file_size" -lt 1048576 ]; then  # 1MB minimum
            echo "[WARN] game.s3e.unpacked seems corrupted or incomplete ($file_size bytes)"
            echo "[INFO] Removing and forcing re-extraction"
            rm -f "$GAME_IMAGE"
        fi
    fi
    
    for _cand in ${SIMS3_ALT_DATADIRS:-/storage/roms/ports/sims3 /storage/games-external/ports/sims3 /roms/ports/sims3 /opt/roms/ports/sims3} /mnt/mmc/MUOS/PortMaster/ports/sims3; do
        [ "$_cand" = "$GAMEDIR" ] && continue
        if [ -f "$_cand/game/game.s3e.unpacked" ] && [ -d "$_cand/game/assets" ]; then
            echo "[INFO] Found extracted data in alternate install: $_cand"
            
            # Copia apenas se game.s3e.unpacked for válido
            if [ -s "$_cand/game/game.s3e.unpacked" ]; then
                local src_size=$(wc -c < "$_cand/game/game.s3e.unpacked" 2>/dev/null || echo 0)
                local src_header=$(od -An -tx1 -N4 "$_cand/game/game.s3e.unpacked" 2>/dev/null | tr -d ' \n')
                if [ "$src_size" -ge 1048576 ] && [ "$src_header" = "58453355" ]; then
                    mkdir -p "$GAME_DIR" 2>/dev/null || return 1
                    if cp -r "$_cand/game/assets" "$GAME_DIR/" 2>/dev/null; then
                        if cp "$_cand/game/game.s3e.unpacked" "$GAME_IMAGE" 2>/dev/null; then
                            echo "[INFO] Copied valid data from $_cand/game to $GAME_DIR"
                            return 0
                        fi
                    fi
                else
                    echo "[WARN] Skipping data from $_cand: game.s3e.unpacked corrupted (size=$src_size header=$src_header)"
                fi
            else
                echo "[WARN] Skipping data from $_cand: game.s3e.unpacked empty"
            fi
        fi
    done
    return 1
}
reuse_existing_game_data || true

# Re-evaluate paths after possible copy
GAME_IMAGE="$GAME_DIR/game.s3e.unpacked"
ASSET_DIR="$GAME_DIR/assets"

# ============================================================
# FINAL S3E HEADER VALIDATION
# ============================================================
if [ -f "$GAME_IMAGE" ]; then
    local file_size=$(wc -c < "$GAME_IMAGE" 2>/dev/null || echo 0)
    local header=$(od -An -tx1 -N4 "$GAME_IMAGE" 2>/dev/null | tr -d ' \n')
    if [ "$file_size" -lt 1048576 ] || [ "$header" != "58453355" ]; then
        echo "[WARN] S3E file invalid or too small: file_size=$file_size header=$header"
        echo "[INFO] Removing corrupted file and forcing extraction"
        rm -f "$GAME_IMAGE"
    fi
fi

# ============================================================
# FIRST LAUNCH: RUN EXTRACTOR IF NEEDED
# ============================================================
if [ ! -f "$GAME_IMAGE" ] || [ ! -d "$ASSET_DIR" ]; then
    echo "[INFO] Game data not found, starting extractor..."
    GATITO_UI="$GAMEDIR/gatito-extract/run.sh"
    GATITO_LOG="$GAMEDIR/logs/gatito-launch.log"
    mkdir -p "$(dirname "$GATITO_LOG")" 2>/dev/null
    {
        echo ""
        echo "=== The Sims 3 / Gatito launch ==="
        echo "DATE=$(date 2>/dev/null || true)"
        echo "GAMEDIR=$GAMEDIR"
        echo "GATITO_UI=$GATITO_UI"
    } >>"$GATITO_LOG" 2>&1
    
    if [ ! -f "$GATITO_UI" ]; then
        echo "[FATAL] Gatito UI launcher missing: $GATITO_UI"
        exit 1
    fi
    
    echo "[INFO] Starting Gatito UI (15 min timeout)..." >>"$GATITO_LOG" 2>&1
    timeout 900 bash "$GATITO_UI" >>"$GATITO_LOG" 2>&1
    EXTRACT_RC=$?
    echo "[Gatito] launcher exit code=$EXTRACT_RC" >>"$GATITO_LOG" 2>&1
    
    if [ "$EXTRACT_RC" -ne 0 ]; then
        echo "[WARN] Gatito UI exit=$EXTRACT_RC (timed out or failed)" >>"$GATITO_LOG" 2>&1
        if [ ! -f "$GAME_IMAGE" ]; then
            echo "[FATAL] Game image missing after extraction, cannot continue"
            exit "$EXTRACT_RC"
        fi
        echo "[INFO] Game image exists despite UI exit code, proceeding..." >>"$GATITO_LOG" 2>&1
    fi
fi

# ============================================================
# FINAL VERIFICATION BEFORE LAUNCHING GAME
# ============================================================
if [ ! -f "$GAME_IMAGE" ]; then
    echo "[FATAL] game image not found: $GAME_IMAGE"
    exit 1
fi
if [ ! -d "$ASSET_DIR" ]; then
    echo "[FATAL] assets directory not found: $ASSET_DIR"
    exit 1
fi
echo "[OK] Game data verified: $GAME_IMAGE"
echo "[OK] Assets verified: $ASSET_DIR"

# ============================================================
# CLEANUP AFTER EXTRACTOR (release DRM master)
# ============================================================
if [ -n "${EXTRACT_RC:-}" ]; then
    for _pat in gatito-ui.py gatito-extract; do
        if command -v pkill >/dev/null 2>&1; then
            pkill -f "$_pat" 2>/dev/null || true
        fi
    done
    sleep 2
    command -v chvt >/dev/null 2>&1 && chvt 1 2>/dev/null || true
fi

# ============================================================
# LIBRARY PATHS
# ============================================================
LIB_PATHS="$GAMEDIR/libs.armhf"
for _p in /lib/arm-linux-gnueabihf /usr/lib/arm-linux-gnueabihf \
          /lib/aarch64-linux-gnu /usr/lib/aarch64-linux-gnu \
          /lib /usr/lib /usr/local/lib; do
    [ -d "$_p" ] && LIB_PATHS="$LIB_PATHS:$_p"
done
export LD_LIBRARY_PATH="$LIB_PATHS${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

export SIMS3_RETRY_FALLBACK="${SIMS3_RETRY_FALLBACK:-1}"

# ============================================================
# PAGEFLIP ERROR DETECTION -> FORCE FBCON
# ============================================================
GATITO_LAUNCH_LOG="$LOGDIR/gatito-launch.log"
if [ -f "$GATITO_LAUNCH_LOG" ] && grep -q "ERROR: Could not queue pageflip" "$GATITO_LAUNCH_LOG" 2>/dev/null; then
    echo "[WARN] Detected pageflip -22 in Gatito UI log; forcing fbcon..."
    export SDL_VIDEODRIVER=fbcon
    sleep 1
elif [ -f "$LOG" ] && grep -q "ERROR: Could not queue pageflip" "$LOG" 2>/dev/null; then
    echo "[WARN] Detected pageflip -22 in runtime log; forcing fbcon..."
    export SDL_VIDEODRIVER=fbcon
    sleep 1
fi

# ============================================================
# RESOLUTION & SDL SETTINGS
# ============================================================
export SIMS3_W="${SIMS3_W:-${DISPLAY_WIDTH:-640}}"
export SIMS3_H="${SIMS3_H:-${DISPLAY_HEIGHT:-480}}"
export SDL_GAMECONTROLLERCONFIG="${sdl_controllerconfig:-${SDL_GAMECONTROLLERCONFIG:-}}"

echo "Resolution: ${SIMS3_W}x${SIMS3_H}"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
echo "SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-auto}"
echo "SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-auto}"

# ============================================================
# DIAGNOSTICS
# ============================================================
echo "--- AUDIO DETECTION ---"
if [ -d /dev/snd ]; then echo "audio: /dev/snd available"; ls -la /dev/snd 2>/dev/null || true; else echo "audio: /dev/snd unavailable"; fi
[ -f /proc/asound/cards ] && cat /proc/asound/cards
command -v pactl >/dev/null 2>&1 && echo "audio: PulseAudio tools available"
command -v pw-cli >/dev/null 2>&1 && echo "audio: PipeWire tools available"

echo "--- VIDEO DETECTION ---"
[ -d /dev/dri ] && ls -la /dev/dri 2>/dev/null || true
[ -e /dev/fb0 ] && echo "video: framebuffer available"
[ -e /dev/mali0 ] && echo "video: Mali device available"
if [ -f /proc/device-tree/compatible ]; then echo "compatible=$(tr "\000" "\n" < /proc/device-tree/compatible 2>/dev/null | tr "\n" " ")"; fi

if command -v pm_platform_helper >/dev/null 2>&1; then
    pm_platform_helper "$LOADER" || echo "[WARN] pm_platform_helper returned $?"
fi

# ============================================================
# GPTOKEYB (if configured)
# ============================================================
GPTOKEYB_PID=""
if [ -f "$GAMEDIR/sims3.gptk" ] && [ -n "${GPTOKEYB:-}" ]; then
    read -r -a GPTOKEYB_CMD <<< "$GPTOKEYB"
    if [ "${#GPTOKEYB_CMD[@]}" -gt 0 ]; then
        "${GPTOKEYB_CMD[@]}" "$LOADER" -c "$GAMEDIR/sims3.gptk" &
        GPTOKEYB_PID=$!
    fi
fi

# ============================================================
# LAUNCH LOADER
# ============================================================
echo "--- STARTING LOADER ---"
TASKSET_CMD=()
if [ -n "${TASKSET:-}" ]; then read -r -a TASKSET_CMD <<< "$TASKSET"; fi

if [ "${#TASKSET_CMD[@]}" -gt 0 ]; then
    "${TASKSET_CMD[@]}" "$LOADER" --run --root "$GAME_DIR" "$GAME_IMAGE"
    GAME_RC=$?
else
    "$LOADER" --run --root "$GAME_DIR" "$GAME_IMAGE"
    GAME_RC=$?
fi
echo "Loader exit code: $GAME_RC"

# ============================================================
# FALLBACK RETRY
# ============================================================
if [ "$GAME_RC" -ne 0 ] && [ "${SIMS3_RETRY_FALLBACK:-0}" = "1" ] && [ "${SIMS3_FALLBACK_ACTIVE:-0}" != "1" ] && [ -f "$GAMEDIR/run-fallback.sh" ]; then
    echo "--- STARTING COMPATIBILITY FALLBACK ---"
    export SIMS3_FALLBACK_ACTIVE=1
    bash "$GAMEDIR/run-fallback.sh"
    GAME_RC=$?
    echo "Fallback exit code: $GAME_RC"
fi

# ============================================================
# CLEANUP
# ============================================================
if [ -n "${GPTOKEYB_PID:-}" ]; then kill "$GPTOKEYB_PID" 2>/dev/null || true; fi
if command -v pidof >/dev/null 2>&1 && [ -n "${ESUDO:-}" ]; then $ESUDO kill -9 "$(pidof gptokeyb)" 2>/dev/null || true; fi
unset SDL_GAMECONTROLLERCONFIG
pm_finish 2>/dev/null || true

echo "=== The Sims 3 runtime END (exit=$GAME_RC) ==="
exit "$GAME_RC"