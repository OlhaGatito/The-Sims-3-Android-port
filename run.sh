#!/bin/bash
# The Sims 3 — internal runtime with multi-CFW support
# Supports: muOS, ArkOS, ROCKNIX, NextOS, PortMaster e Linux ARM genérico
set +u
GAMEDIR="$(CDPATH= cd -- "$(dirname -- "$0")" 2>/dev/null && pwd -P)" || exit 1
cd "$GAMEDIR" || exit 1
LOGDIR="${SIMS3_LOG_DIR:-$GAMEDIR/logs}"
mkdir -p "$LOGDIR" 2>/dev/null || exit 1
LOG="${SIMS3_LOG:-$LOGDIR/debug.log}"
touch "$LOG" 2>/dev/null || LOG="${TMPDIR:-/tmp}/sims3-debug.log"
echo "=== The Sims 3 runtime ==="
echo "GAMEDIR=$GAMEDIR"

# --- Deteccao de CFW quando o PortMaster nao definiu ---
if [ -z "${CFW_NAME:-}" ]; then
    CFW_NAME=unknown
    [ -f /opt/muos/bin/muos-version ] && CFW_NAME=muos
    { [ -d /opt/system/bin ] && [ -f "/opt/system/Advanced/Firmware Version.txt" ]; } && CFW_NAME=arkos
    [ -f /opt/bin/emulationstation ] && CFW_NAME=rocknix
    grep -qi "nextos" /etc/os-release 2>/dev/null && CFW_NAME=nextos
fi
export CFW_NAME

# --- Deteccao de arquitetura ---
ARCH="${DEVICE_ARCH:-$(uname -m)}"
export DEVICE_ARCH="$ARCH"
echo "CFW=$CFW_NAME"
echo "DEVICE=${DEVICE_NAME:-$(uname -n 2>/dev/null || echo unknown)}"
echo "ARCH=$ARCH"
echo "DATE=$(date 2>/dev/null || true)"

# --- Compatibilidade de audio/video (deteccao automatica) ---
if [ -f "$GAMEDIR/port_compat.sh" ]; then
    . "$GAMEDIR/port_compat.sh"
    port_detect_runtime 2>/dev/null || true
fi
GAME_DIR="$GAMEDIR/game"
GAME_IMAGE="$GAME_DIR/game.s3e.unpacked"
LOADER="$GAMEDIR/sims3_s3e_loader"
ASSET_DIR="$GAME_DIR/assets"
[ -f "$LOADER" ] || { echo "[ERROR] loader not found: $LOADER"; exit 1; }
if [ ! -x "$LOADER" ]; then chmod +x "$LOADER" 2>/dev/null || true; fi
[ -x "$LOADER" ] || { echo "[ERROR] loader is not executable: $LOADER"; exit 126; }

# First launch: keep output visible so Gatito can update PortMaster's GUI.
if [ ! -f "$GAME_IMAGE" ] || [ ! -d "$ASSET_DIR" ]; then
    GATITO_UI="$GAMEDIR/gatito-extract/run.sh"
    GATITO_LOG="$GAMEDIR/logs/gatito-launch.log"
    {
        echo ""
        echo "=== The Sims 3 / Gatito launch ==="
        echo "DATE=$(date 2>/dev/null || true)"
        echo "GAMEDIR=$GAMEDIR"
        echo "GATITO_UI=$GATITO_UI"
    } >>"$GATITO_LOG" 2>&1
    [ -f "$GATITO_UI" ] || { echo "[ERROR] Gatito UI launcher missing: $GATITO_UI"; exit 1; }
    
    # Run Gatito UI with a safety timeout (5 minutes). If it hangs, we'll skip and try loader.
    echo "[INFO] Starting Gatito UI (5 min timeout)..." >>"$GATITO_LOG" 2>&1
    timeout 300 bash "$GATITO_UI" >>"$GATITO_LOG" 2>&1
    EXTRACT_RC=$?
    echo "[Gatito] launcher exit code=$EXTRACT_RC" >>"$GATITO_LOG" 2>&1
    
    # If UI timed out (124) or failed (!=0), try loading game anyway or use fallback
    if [ "$EXTRACT_RC" -ne 0 ]; then
        echo "[AVISO] Gatito UI exit=$EXTRACT_RC (timed out or failed)" >>"$GATITO_LOG" 2>&1
        if [ ! -f "$GAME_IMAGE" ]; then
            echo "[INFO] Game image missing, no extraction occurred. Exiting." >>"$GATITO_LOG" 2>&1
            echo "[ERROR] data preparation failed: $EXTRACT_RC"
            exit "$EXTRACT_RC"
        fi
        echo "[INFO] Game image exists despite UI exit code. Proceeding..." >>"$GATITO_LOG" 2>&1
    fi
fi

[ -f "$GAME_IMAGE" ] || { echo "[ERROR] game image not found: $GAME_IMAGE"; exit 1; }
[ -d "$ASSET_DIR" ] || { echo "[ERROR] assets directory not found: $ASSET_DIR"; exit 1; }

# Only the actual game runtime goes to the persistent log.
exec >>"$LOG" 2>&1
echo "=== The Sims 3 game runtime ==="
echo "Prepared game image: $GAME_IMAGE"
echo "Prepared assets: $ASSET_DIR"
export SIMS3_W="${SIMS3_W:-${DISPLAY_WIDTH:-640}}"
export SIMS3_H="${SIMS3_H:-${DISPLAY_HEIGHT:-480}}"

# Bibliotecas: prioriza as do port, depois multiarch do sistema (armhf em host aarch64)
LIB_PATHS="$GAMEDIR/libs.armhf"
for _p in /lib/arm-linux-gnueabihf /usr/lib/arm-linux-gnueabihf \
          /lib/aarch64-linux-gnu /usr/lib/aarch64-linux-gnu \
          /lib /usr/lib /usr/local/lib; do
    [ -d "$_p" ] && LIB_PATHS="$LIB_PATHS:$_p"
done
export LD_LIBRARY_PATH="$LIB_PATHS${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Fallback de compatibilidade habilitado por padrao (desative com SIMS3_RETRY_FALLBACK=0)
export SIMS3_RETRY_FALLBACK="${SIMS3_RETRY_FALLBACK:-1}"

# Em hosts AArch64, confirma suporte a binarios de 32 bits (loader armhf)
if [ "$ARCH" = "aarch64" ]; then
    if [ ! -e /lib/ld-linux-armhf.so.3 ] && [ ! -e /lib/arm-linux-gnueabihf/ld-linux-armhf.so.3 ]; then
        echo "[AVISO] Host AArch64 sem loader ARM32 (ld-linux-armhf.so.3)."
        echo "[AVISO] O loader atual so roda em ARM 32 bits (armhf)."
    fi
fi
export SDL_GAMECONTROLLERCONFIG="${sdl_controllerconfig:-${SDL_GAMECONTROLLERCONFIG:-}}"
echo "Resolution: ${SIMS3_W}x${SIMS3_H}"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
echo "SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-auto}"
echo "SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-auto}"
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
if command -v pm_platform_helper >/dev/null 2>&1; then pm_platform_helper "$LOADER" || echo "[WARN] pm_platform_helper returned $?"; fi
GPTOKEYB_PID=""
if [ -f "$GAMEDIR/sims3.gptk" ] && [ -n "${GPTOKEYB:-}" ]; then
    read -r -a GPTOKEYB_CMD <<< "$GPTOKEYB"
    if [ "${#GPTOKEYB_CMD[@]}" -gt 0 ]; then
        "${GPTOKEYB_CMD[@]}" "$LOADER" -c "$GAMEDIR/sims3.gptk" &
        GPTOKEYB_PID=$!
    fi
fi
echo "--- STARTING LOADER ---"
TASKSET_CMD=()
if [ -n "${TASKSET:-}" ]; then read -r -a TASKSET_CMD <<< "$TASKSET"; fi
if [ "${#TASKSET_CMD[@]}" -gt 0 ]; then
    "${TASKSET_CMD[@]}" "$LOADER" --run --root "$GAME_DIR" "$GAME_IMAGE"; GAME_RC=$?
else
    "$LOADER" --run --root "$GAME_DIR" "$GAME_IMAGE"; GAME_RC=$?
fi
echo "Loader exit code: $GAME_RC"
if [ "$GAME_RC" -ne 0 ] && [ "${SIMS3_RETRY_FALLBACK:-0}" = "1" ] && [ "${SIMS3_FALLBACK_ACTIVE:-0}" != "1" ] && [ -f "$GAMEDIR/run-fallback.sh" ]; then
    echo "--- STARTING COMPATIBILITY FALLBACK ---"
    export SIMS3_FALLBACK_ACTIVE=1
    bash "$GAMEDIR/run-fallback.sh"; GAME_RC=$?
    echo "Fallback exit code: $GAME_RC"
fi
if [ -n "${GPTOKEYB_PID:-}" ]; then kill "$GPTOKEYB_PID" 2>/dev/null || true; fi
if command -v pidof >/dev/null 2>&1 && [ -n "${ESUDO:-}" ]; then $ESUDO kill -9 "$(pidof gptokeyb)" 2>/dev/null || true; fi
unset SDL_GAMECONTROLLERCONFIG
pm_finish 2>/dev/null || true
exit "$GAME_RC"
