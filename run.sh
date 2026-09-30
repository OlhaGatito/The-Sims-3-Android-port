#!/bin/bash
# The Sims 3 — internal PortMaster runtime
# The launcher loads the PortMaster environment; this script only runs the game.

set +u

GAMEDIR="$(CDPATH= cd -- "$(dirname -- "$0")" 2>/dev/null && pwd -P)" || exit 1
cd "$GAMEDIR" || exit 1

LOGDIR="${SIMS3_LOG_DIR:-$GAMEDIR/logs}"
mkdir -p "$LOGDIR" 2>/dev/null || exit 1
LOG="${SIMS3_LOG:-$LOGDIR/debug.log}"
touch "$LOG" 2>/dev/null || LOG="${TMPDIR:-/tmp}/sims3-debug.log"
exec >>"$LOG" 2>&1

echo "=== The Sims 3 internal runtime ==="
echo "GAMEDIR=$GAMEDIR"
echo "CFW=${CFW_NAME:-unknown}"
echo "DEVICE=${DEVICE_NAME:-unknown}"
echo "ARCH=${DEVICE_ARCH:-unknown}"
echo "DATE=$(date 2>/dev/null || true)"

GAME_DIR="$GAMEDIR/game"
GAME_IMAGE="$GAME_DIR/game.s3e.unpacked"
LOADER="$GAMEDIR/sims3_s3e_loader"
ASSET_DIR="$GAME_DIR/assets"

[ -f "$LOADER" ] || { echo "[ERROR] loader not found: $LOADER"; exit 1; }
if [ ! -x "$LOADER" ]; then
    chmod +x "$LOADER" 2>/dev/null || true
fi
[ -x "$LOADER" ] || {
    echo "[ERROR] loader is not executable: $LOADER"
    exit 126
}

# First launch: prepare game data from a user-supplied APK/APKM/APKS/XAPK/OBB.
if [ ! -f "$GAME_IMAGE" ] || [ ! -d "$ASSET_DIR" ]; then
    EXTRACTOR="$GAMEDIR/run-extractor.sh"
    [ -f "$EXTRACTOR" ] || {
        echo "[ERROR] game data is missing and extractor was not found: $EXTRACTOR"
        exit 1
    }
    echo "[setup] game data is incomplete; starting Gatito Extractor UI"
    GATITO_UI="$GAMEDIR/gatito-extract/run.sh"
    if [ -f "$GATITO_UI" ]; then
        bash "$GATITO_UI"
    else
        bash "$EXTRACTOR"
    fi
    EXTRACT_RC=$?
    [ "$EXTRACT_RC" -eq 0 ] || {
        echo "[ERROR] data preparation failed with exit code $EXTRACT_RC"
        exit "$EXTRACT_RC"
    }
fi

[ -f "$GAME_IMAGE" ] || { echo "[ERROR] game image not found after preparation: $GAME_IMAGE"; exit 1; }
[ -d "$ASSET_DIR" ] || { echo "[ERROR] assets directory not found after preparation: $ASSET_DIR"; exit 1; }

# Resolution comes from PortMaster/CFW when available.
export SIMS3_W="${SIMS3_W:-${DISPLAY_WIDTH:-640}}"
export SIMS3_H="${SIMS3_H:-${DISPLAY_HEIGHT:-480}}"

# Keep PortMaster's library environment and add only the port's own libraries.
if [ -d "$GAMEDIR/libs.armhf" ]; then
    export LD_LIBRARY_PATH="$GAMEDIR/libs.armhf${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

export SDL_GAMECONTROLLERCONFIG="${sdl_controllerconfig:-${SDL_GAMECONTROLLERCONFIG:-}}"

echo "Resolution: ${SIMS3_W}x${SIMS3_H}"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
echo "SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-auto}"
echo "SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-auto}"

echo "--- AUDIO DETECTION ---"
if [ -d /dev/snd ]; then
    echo "audio: /dev/snd available"
    ls -la /dev/snd 2>/dev/null || true
else
    echo "audio: /dev/snd unavailable"
fi
[ -f /proc/asound/cards ] && cat /proc/asound/cards
command -v pactl >/dev/null 2>&1 && echo "audio: PulseAudio tools available"
command -v pw-cli >/dev/null 2>&1 && echo "audio: PipeWire tools available"

echo "--- VIDEO DETECTION ---"
[ -d /dev/dri ] && ls -la /dev/dri 2>/dev/null || true
[ -e /dev/fb0 ] && echo "video: framebuffer available"
[ -e /dev/mali0 ] && echo "video: Mali device available"

if [ -f /proc/device-tree/compatible ]; then
    echo "compatible=$(tr "\000" "\n" < /proc/device-tree/compatible 2>/dev/null | tr "\n" " ")"
fi

# Let PortMaster prepare the binary before execution.
if command -v pm_platform_helper >/dev/null 2>&1; then
    pm_platform_helper "$LOADER" || echo "[WARN] pm_platform_helper returned $?"
fi

GPTOKEYB_PID=""
if [ -f "$GAMEDIR/sims3.gptk" ] && [ -n "${GPTOKEYB:-}" ]; then
    # PortMaster's GPTOKEYB value is a command plus arguments (often ESUDO,
    # the gptokeyb path and ESUDOKILL), so invoke it as an argv array.
    read -r -a GPTOKEYB_CMD <<< "$GPTOKEYB"
    if [ "${#GPTOKEYB_CMD[@]}" -gt 0 ]; then
        "${GPTOKEYB_CMD[@]}" "$LOADER" -c "$GAMEDIR/sims3.gptk" &
        GPTOKEYB_PID=$!
    fi
fi

echo "--- STARTING LOADER ---"
TASKSET_CMD=()
if [ -n "${TASKSET:-}" ]; then
    read -r -a TASKSET_CMD <<< "$TASKSET"
fi
if [ "${#TASKSET_CMD[@]}" -gt 0 ]; then
    "${TASKSET_CMD[@]}" "$LOADER" --run --root "$GAME_DIR" "$GAME_IMAGE"
    GAME_RC=$?
else
    "$LOADER" --run --root "$GAME_DIR" "$GAME_IMAGE"
    GAME_RC=$?
fi

echo "Loader exit code: $GAME_RC"

# An opt-in compatibility retry is available after a nonzero loader exit.
if [ "$GAME_RC" -ne 0 ] \
    && [ "${SIMS3_RETRY_FALLBACK:-0}" = "1" ] \
    && [ "${SIMS3_FALLBACK_ACTIVE:-0}" != "1" ] \
    && [ -f "$GAMEDIR/run-fallback.sh" ]; then
    echo "--- STARTING COMPATIBILITY FALLBACK ---"
    export SIMS3_FALLBACK_ACTIVE=1
    bash "$GAMEDIR/run-fallback.sh"
    GAME_RC=$?
    echo "Fallback exit code: $GAME_RC"
fi

if [ -n "${GPTOKEYB_PID:-}" ]; then
    kill "$GPTOKEYB_PID" 2>/dev/null || true
fi

if command -v pidof >/dev/null 2>&1 && [ -n "${ESUDO:-}" ]; then
    $ESUDO kill -9 "$(pidof gptokeyb)" 2>/dev/null || true
fi

unset SDL_GAMECONTROLLERCONFIG
pm_finish 2>/dev/null || true
exit "$GAME_RC"
