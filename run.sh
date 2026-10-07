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
detect_cfw() {
    local cfw=unknown
    [ -f /opt/muos/bin/muos-version ] && cfw=muos
    { [ -d /opt/system/bin ] && [ -f "/opt/system/Advanced/Firmware Version.txt" ]; } && cfw=arkos
    [ -f /opt/bin/emulationstation ] && cfw=rocknix
    grep -qi "nextos" /etc/os-release 2>/dev/null && cfw=nextos
    [ "$cfw" = "unknown" ] && grep -qi "rocknix" /etc/os-release 2>/dev/null && cfw=rocknix
    [ "$cfw" = "unknown" ] && grep -qi "aurknix" /etc/os-release 2>/dev/null && cfw=rocknix
    [ "$cfw" = "unknown" ] && [ -d /storage/roms ] && [ -f /opt/bin/emulationstation ] && cfw=rocknix
    echo "$cfw"
}
CFW_NAME=$(detect_cfw)
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
    case "$1" in
        aarch64|arm64)
            [ -x "$GAMEDIR/sims3_s3e_loader_aarch64" ] && { echo "$GAMEDIR/sims3_s3e_loader_aarch64"; return; }
            echo "$GAMEDIR/sims3_s3e_loader"
            ;;
        arm*|aarch32)
            echo "$GAMEDIR/sims3_s3e_loader"
            ;;
        *)
            echo "$GAMEDIR/sims3_s3e_loader"
            ;;
    esac
}
LOADER="$(select_loader "$ARCH")"
echo "LOADER=$LOADER (arch=$ARCH)"

# ============================================================
# VERIFY LOADER
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
[ ! -x "$LOADER" ] && { echo "[FATAL] Loader not executable: $LOADER"; exit 126; }
echo "[OK] Loader verified: $LOADER"

# ============================================================
# 32-bit COMPAT CHECK (for ARMv7 loader on AArch64 host)
# ============================================================
if [ "$ARCH" = "aarch64" ] && command -v readelf >/dev/null 2>&1; then
    INTERP=$(readelf -l "$LOADER" 2>/dev/null | sed -n 's/.*Requesting program interpreter: \([^]]*\).*/\1/p' | head -1)
    if [ -n "$INTERP" ] && [ ! -f "$INTERP" ]; then
        echo "[WARN] Loader 32-bit requires interpreter $INTERP (not found)"
        for p in /lib/ld-linux-armhf.so.3 /lib/arm-linux-gnueabihf/ld-linux-armhf.so.3 /usr/lib/arm-linux-gnueabihf/ld-linux-armhf.so.3; do
            [ -f "$p" ] && { echo "[OK] Found: $p"; break; }
        done
        export LD_LIBRARY_PATH="/lib/arm-linux-gnueabihf:/usr/lib/arm-linux-gnueabihf:/lib:/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    fi
fi

# ============================================================
# S3E VALIDATION HELPER
# ============================================================
validate_s3e() {
    local f="$1"
    [ -f "$f" ] && [ -s "$f" ] || return 1
    local sz=$(wc -c < "$f" 2>/dev/null || echo 0)
    local hdr=$(od -An -tx1 -N4 "$f" 2>/dev/null | tr -d ' \n')
    [ "$sz" -ge 1048576 ] && [ "$hdr" = "58453355" ]
}

# ============================================================
# CHECK IF GAME DATA ALREADY VALID (FAST PATH)
# ============================================================
if validate_s3e "$GAME_IMAGE" && [ -d "$ASSET_DIR" ] && [ -n "$(ls -A "$ASSET_DIR" 2>/dev/null)" ]; then
    echo "[OK] Game data already valid, skipping extractor"
    EXTRACT_RC=0
else
    echo "[INFO] Game data missing or invalid, will attempt to restore/extract"
    EXTRACT_RC=1
fi

# ============================================================
# REUSE EXISTING EXTRACTED DATA FROM ALTERNATE LOCATIONS
# ============================================================
reuse_existing_game_data() {
    # Already valid? Nothing to do
    validate_s3e "$GAME_IMAGE" && [ -d "$ASSET_DIR" ] && return 0

    # Remove corrupted local file if exists
    if [ -f "$GAME_IMAGE" ] && ! validate_s3e "$GAME_IMAGE"; then
        local sz=$(wc -c < "$GAME_IMAGE" 2>/dev/null || echo 0)
        local hdr=$(od -An -tx1 -N4 "$GAME_IMAGE" 2>/dev/null | tr -d ' \n')
        echo "[WARN] Local game.s3e.unpacked invalid (size=$sz header=$hdr), removing"
        rm -f "$GAME_IMAGE"
    fi

    # Search alternate locations
    for _cand in ${SIMS3_ALT_DATADIRS:-/storage/roms/ports/sims3 /storage/games-external/ports/sims3 /roms/ports/sims3 /opt/roms/ports/sims3} /mnt/mmc/MUOS/PortMaster/ports/sims3; do
        [ "$_cand" = "$GAMEDIR" ] && continue
        local src_img="$_cand/game/game.s3e.unpacked"
        local src_assets="$_cand/game/assets"
        [ -f "$src_img" ] && [ -d "$src_assets" ] || continue

        if validate_s3e "$src_img" && [ -n "$(ls -A "$src_assets" 2>/dev/null)" ]; then
            echo "[INFO] Found valid extracted data in: $_cand"
            mkdir -p "$GAME_DIR" 2>/dev/null || return 1
            cp -r "$src_assets" "$GAME_DIR/" 2>/dev/null || continue
            cp "$src_img" "$GAME_IMAGE" 2>/dev/null || continue
            echo "[INFO] Copied valid data from $_cand"
            return 0
        else
            local sz=$(wc -c < "$src_img" 2>/dev/null || echo 0)
            local hdr=$(od -An -tx1 -N4 "$src_img" 2>/dev/null | tr -d ' \n')
            echo "[WARN] Skipping $_cand: S3E invalid (size=$sz header=$hdr)"
        fi
    done
    return 1
}

# Try to reuse if not already valid
if [ "$EXTRACT_RC" -ne 0 ]; then
    reuse_existing_game_data || true
    # Re-validate after potential copy
    if validate_s3e "$GAME_IMAGE" && [ -d "$ASSET_DIR" ] && [ -n "$(ls -A "$ASSET_DIR" 2>/dev/null)" ]; then
        echo "[OK] Game data restored from alternate location"
        EXTRACT_RC=0
    fi
fi

# ============================================================
# RUN EXTRACTOR ONLY IF STILL NEEDED
# ============================================================
if [ "$EXTRACT_RC" -ne 0 ]; then
    echo "[INFO] Starting Gatito Extractor (data missing/invalid)..."
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

    [ -f "$GATITO_UI" ] || { echo "[FATAL] Gatito UI launcher missing: $GATITO_UI"; exit 1; }

    echo "[INFO] Starting Gatito UI (15 min timeout)..." >>"$GATITO_LOG" 2>&1

    # Up to 2 extraction attempts
    for attempt in 1 2; do
        echo "[INFO] Extraction attempt $attempt/2..." >>"$GATITO_LOG" 2>&1
        timeout 900 bash "$GATITO_UI" >>"$GATITO_LOG" 2>&1
        EXTRACT_RC=$?
        echo "[Gatito] launcher exit code=$EXTRACT_RC (attempt $attempt)" >>"$GATITO_LOG" 2>&1

        if [ "$EXTRACT_RC" -eq 0 ] && validate_s3e "$GAME_IMAGE" && [ -d "$ASSET_DIR" ]; then
            echo "[INFO] Extraction successful, S3E valid" >>"$GATITO_LOG" 2>&1
            break
        fi

        [ "$EXTRACT_RC" -eq 0 ] && ! validate_s3e "$GAME_IMAGE" && {
            echo "[WARN] Extraction exited 0 but S3E invalid, retrying..." >>"$GATITO_LOG" 2>&1
            rm -f "$GAME_IMAGE"
        }

        [ "$attempt" -eq 2 ] && [ "$EXTRACT_RC" -ne 0 ] && {
            echo "[FATAL] Both extraction attempts failed" >>"$GATITO_LOG" 2>&1
            exit 1
        }
        echo "[INFO] Will retry extraction..." >>"$GATITO_LOG" 2>&1
    done
fi

# ============================================================
# FINAL VALIDATION BEFORE LAUNCH
# ============================================================
validate_s3e "$GAME_IMAGE" || { echo "[FATAL] S3E invalid or missing: $GAME_IMAGE"; exit 1; }
[ -d "$ASSET_DIR" ] && [ -n "$(ls -A "$ASSET_DIR" 2>/dev/null)" ] || { echo "[FATAL] Assets missing: $ASSET_DIR"; exit 1; }
echo "[OK] Game data verified: $GAME_IMAGE"
echo "[OK] Assets verified: $ASSET_DIR"

# ============================================================
# CLEANUP AFTER EXTRACTOR (release DRM master)
# ============================================================
if [ -n "${EXTRACT_RC:-}" ] && [ "$EXTRACT_RC" -eq 0 ]; then
    for _pat in gatito-ui.py gatito-extract; do
        command -v pkill >/dev/null 2>&1 && pkill -f "$_pat" 2>/dev/null || true
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

command -v pm_platform_helper >/dev/null 2>&1 && pm_platform_helper "$LOADER" || echo "[WARN] pm_platform_helper returned $?"

# ============================================================
# GPTOKEYB (if configured)
# ============================================================
GPTOKEYB_PID=""
if [ -f "$GAMEDIR/sims3.gptk" ] && [ -n "${GPTOKEYB:-}" ]; then
    read -r -a GPTOKEYB_CMD <<< "$GPTOKEYB"
    [ "${#GPTOKEYB_CMD[@]}" -gt 0 ] && { "${GPTOKEYB_CMD[@]}" "$LOADER" -c "$GAMEDIR/sims3.gptk" & GPTOKEYB_PID=$!; }
fi

# ============================================================
# LAUNCH LOADER
# ============================================================
echo "--- STARTING LOADER ---"
TASKSET_CMD=()
[ -n "${TASKSET:-}" ] && read -r -a TASKSET_CMD <<< "$TASKSET"

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
[ -n "${GPTOKEYB_PID:-}" ] && kill "$GPTOKEYB_PID" 2>/dev/null || true
command -v pidof >/dev/null 2>&1 && [ -n "${ESUDO:-}" ] && $ESUDO kill -9 "$(pidof gptokeyb 2>/dev/null)" 2>/dev/null || true
unset SDL_GAMECONTROLLERCONFIG
pm_finish 2>/dev/null || true

echo "=== The Sims 3 runtime END (exit=$GAME_RC) ==="
exit "$GAME_RC"