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

# Log from the very beginning (before any checks) to catch early failures
exec >>"$LOG" 2>&1
echo "=== The Sims 3 runtime ==="
echo "GAMEDIR=$GAMEDIR"
echo "START_TIME=$(date 2>/dev/null || true)"

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
ASSET_DIR="$GAME_DIR/assets"

# --- Selecao automatica do loader por arquitetura ---
# REGRA IMPORTANTE: binario aarch64 (64-bit) NAO roda em kernel 32-bit.
# Por isso o loader armv7 e sempre o padrao seguro; o aarch64 so e usado
# quando o kernel e 64-bit E o binario existe.
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
            # Kernel 32-bit: SOMENTE o loader armv7 funciona aqui.
            # Um binario aarch64 daria "cannot execute binary file".
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

# --- Reaproveita dados extraidos de instalacao duplicada ---
# Em alguns CFWs o PortMaster monta os ports em dois lugares
# (ex.: /storage/roms/ports e /storage/games-external/ports).
# Se a extracao existe em outro caminho, copia para ca em vez de reextrair.
reuse_existing_game_data() {
    [ -f "$GAME_IMAGE" ] && [ -d "$ASSET_DIR" ] && return 0
    for _cand in ${SIMS3_ALT_DATADIRS:-/storage/roms/ports/sims3 /storage/games-external/ports/sims3 /roms/ports/sims3 /opt/roms/ports/sims3} /mnt/mmc/MUOS/PortMaster/ports/sims3; do
        [ "$_cand" = "$GAMEDIR" ] && continue
        if [ -f "$_cand/game/game.s3e.unpacked" ] && [ -d "$_cand/game/assets" ]; then
            echo "[INFO] Dados extraidos encontrados em instalacao alternativa: $_cand"
            mkdir -p "$GAME_DIR" 2>/dev/null || return 1
            if cp -r "$_cand/game/." "$GAME_DIR/" 2>/dev/null; then
                echo "[INFO] Dados copiados de $_cand/game para $GAME_DIR (evita reextracao)"
                return 0
            fi
            return 1
        fi
    done
    return 1
}
reuse_existing_game_data || true
# Reavalia paths apos possivel copia
GAME_IMAGE="$GAME_DIR/game.s3e.unpacked"
ASSET_DIR="$GAME_DIR/assets"
[ -f "$LOADER" ] || { echo "[ERROR] loader not found: $LOADER"; exit 1; }
if [ ! -x "$LOADER" ]; then chmod +x "$LOADER" 2>/dev/null || true; fi
[ -x "$LOADER" ] || { echo "[ERROR] loader is not executable: $LOADER"; exit 126; }

# Verifica compatibilidade 32-bit no host aarch64 (loader é ARMv7)
if [ "$ARCH" = "aarch64" ]; then
    INTERP=$(readelf -l "$LOADER" 2>/dev/null | grep 'INTERP' | awk '{print $4}' | tr -d '[]')
    if [ -n "$INTERP" ] && [ ! -f "$INTERP" ]; then
        echo "[AVISO] Loader 32-bit requer interpretador $INTERP (não encontrado)" 
        echo "[AVISO] Tentando localizar ld-linux-armhf.so.3..."
        for p in /lib/ld-linux-armhf.so.3 /lib/arm-linux-gnueabihf/ld-linux-armhf.so.3 /usr/lib/arm-linux-gnueabihf/ld-linux-armhf.so.3; do
            [ -f "$p" ] && { echo "[OK] Encontrado: $p"; break; }
        done
        # Tenta adicionar paths de bibliotecas 32-bit
        export LD_LIBRARY_PATH="/lib/arm-linux-gnueabihf:/usr/lib/arm-linux-gnueabihf:/lib:/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    fi
fi

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
    
    # Run Gatito UI with a safety timeout (15 minutes). If it hangs, we'll skip and try loader.
    echo "[INFO] Starting Gatito UI (15 min timeout)..." >>"$GATITO_LOG" 2>&1
    timeout 900 bash "$GATITO_UI" >>"$GATITO_LOG" 2>&1
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

# Se o Gatito UI acabou de rodar, aguarda um momento para liberar DRM master
if [ -n "${EXTRACT_RC:-}" ]; then
    # Matar processos residuais do extrator (python/SDL que seguram DRM master)
    for _pat in gatito-ui.py gatito-extract; do
        if command -v pkill >/dev/null 2>&1; then
            pkill -f "$_pat" 2>/dev/null || true
        fi
    done
    sleep 2
    # Tenta restaurar modo de console se necessário
    command -v chvt >/dev/null 2>&1 && chvt 1 2>/dev/null || true
fi

# Everything (including early checks) goes to the persistent log.
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

# Se estava com pageflip error (erro -22 EINVAL do DRM), forcar fbcon
GATITO_LAUNCH_LOG="$LOGDIR/gatito-launch.log"
if [ -f "$GATITO_LAUNCH_LOG" ] && grep -q "ERROR: Could not queue pageflip" "$GATITO_LAUNCH_LOG" 2>/dev/null; then
    echo "[WARN] Detectado pageflip -22 no Gatito UI ($GATITO_LAUNCH_LOG); resetando display para fbcon..."
    export SDL_VIDEODRIVER=fbcon
    sleep 1
elif [ -f "$LOG" ] && grep -q "ERROR: Could not queue pageflip" "$LOG" 2>/dev/null; then
    echo "[WARN] Detectado pageflip -22 no runtime log; resetando display para fbcon..."
    export SDL_VIDEODRIVER=fbcon
    sleep 1
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
