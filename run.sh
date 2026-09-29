#!/bin/sh
# The Sims 3 — PortMaster / NextOS-style multi-device runtime
# Arquitetura: wrapper fino -> este runtime único -> PortMaster/CFW -> loader ARM32.
# Não gerencia frontend nem mata processos; o CFW/PortMaster é dono desse ciclo.
# BYO-data: APK/OBB/assets do jogo não fazem parte deste projeto.

set +u

# 1) Resolver o diretório real do port e abrir o log ANTES de qualquer outra etapa.
GAMEDIR="$(CDPATH= cd -- "$(dirname -- "$0")" 2>/dev/null && pwd -P)" || exit 1
cd "$GAMEDIR" || exit 1

LOGDIR="${SIMS3_LOG_DIR:-$GAMEDIR/logs}"
mkdir -p "$LOGDIR" 2>/dev/null || exit 1
LOG="${SIMS3_LOG:-$LOGDIR/debug.log}"
touch "$LOG" 2>/dev/null || {
  LOG="${TMPDIR:-/tmp}/sims3-debug.log"
  touch "$LOG" 2>/dev/null || true
}
exec >>"$LOG" 2>&1

echo "=== The Sims 3 / NextOS-style PortMaster runtime ==="
echo "[runtime] gamedir=$GAMEDIR"
echo "[runtime] invoked=$0"
echo "[runtime] pwd=$(pwd 2>/dev/null || true)"
echo "[runtime] args=$*"

# 2) PortMaster preamble.
# A lista inclui os caminhos oficiais e os layouts encontrados em muOS/Anbernic.
XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
controlfolder=""
for _cf in   /opt/system/Tools/PortMaster   /opt/tools/PortMaster   "$XDG_DATA_HOME/PortMaster"   /mnt/mmc/MUOS/PortMaster   /mnt/sdcard/MUOS/PortMaster   /mnt/mmc/Roms/PORTS/PortMaster   /mnt/sdcard/Roms/PORTS/PortMaster   /roms/ports/PortMaster   /roms/PORTS/PortMaster   /storage/roms/ports/PortMaster   /storage/roms/PORTS/PortMaster
do
  if [ -d "$_cf" ]; then
    controlfolder="$_cf"
    break
  fi
done
: "${controlfolder:=/roms/ports/PortMaster}"
export controlfolder

echo "[runtime] controlfolder=$controlfolder"

if [ -f "$controlfolder/control.txt" ]; then
  # control.txt pode consultar variáveis não definidas.
  . "$controlfolder/control.txt"
  [ -f "$controlfolder/mod_${CFW_NAME}.txt" ] && . "$controlfolder/mod_${CFW_NAME}.txt"
  command -v get_controls >/dev/null 2>&1 && get_controls || true
else
  echo "[WARN] PortMaster control.txt não encontrado em $controlfolder"
fi

: "${ESUDO:=}"
: "${CFW_NAME:=unknown}"
: "${DEVICE_ARCH:=armhf}"
export ESUDO CFW_NAME DEVICE_ARCH

# PortMaster identifica explicitamente ports 32-bit quando necessário.
export PORT_32BIT=Y

# Preservar o mapeamento SDL fornecido pelo PortMaster.
if [ -n "${sdl_controllerconfig:-}" ]; then
  export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
fi

echo "[runtime] cfw=$CFW_NAME"
echo "[runtime] device_arch=$DEVICE_ARCH"
echo "[runtime] arch=$(uname -m 2>/dev/null || echo unknown)"
echo "[runtime] kernel=$(uname -r 2>/dev/null || echo unknown)"

# 3) Bibliotecas: firmware/PortMaster primeiro; libs específicas do port depois.
# Não inserir libs Android sobre libc/libdl/libpthread do firmware.
LD_PARTS=""
for _lib in   "$controlfolder/libs"   "$controlfolder/libs.armhf"   /usr/lib32 /lib32 /usr/lib /lib   /opt/muos/extra/lib /opt/python/lib /usr/lib/gl4es /opt/muos/frontend/lib
do
  [ -d "$_lib" ] || continue
  case ":$LD_PARTS:" in
    *":$_lib:"*) ;;
    *) LD_PARTS="${LD_PARTS:+$LD_PARTS:}$_lib" ;;
  esac
done
[ -d "$GAMEDIR/libs" ] && LD_PARTS="$GAMEDIR/libs${LD_PARTS:+:$LD_PARTS}"
export LD_LIBRARY_PATH="$LD_PARTS${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
echo "[runtime] LD_LIBRARY_PATH=$LD_LIBRARY_PATH"

# 4) Áudio: deixar SDL/firmware negociar por padrão.
# Para diagnóstico, SIMS3_AUDIO_DRIVER pode ser alsa, pulseaudio ou pipewire.
# AUDIO_DRIVER também é aceito como compatibilidade com a convenção NextOS.
AUDIO_SEL="${SIMS3_AUDIO_DRIVER:-${AUDIO_DRIVER:-}}"
case "$AUDIO_SEL" in
  "")
    unset SDL_AUDIODRIVER 2>/dev/null || true
    echo "[audio] backend automático: SDL/firmware"
    ;;
  alsa)
    export SDL_AUDIODRIVER=alsa
    echo "[audio] driver=alsa"
    ;;
  pulse|pulseaudio)
    export SDL_AUDIODRIVER=pulseaudio
    echo "[audio] driver=pulseaudio"
    ;;
  pipewire)
    export SDL_AUDIODRIVER=pipewire
    echo "[audio] driver=pipewire"
    ;;
  *)
    echo "[audio] ERRO: driver inválido: $AUDIO_SEL"
    exit 76
    ;;
esac

if [ -d /dev/snd ]; then
  echo "[audio] /dev/snd presente"
  ls -la /dev/snd 2>/dev/null || true
else
  echo "[audio] AVISO: /dev/snd ausente"
fi
if [ -r /proc/asound/cards ]; then
  echo "[audio] ALSA cards:"
  cat /proc/asound/cards 2>/dev/null || true
fi

# 5) Ambiente do aparelho.
if [ -f /proc/device-tree/compatible ]; then
  COMPAT="$(tr "\000" "\n" < /proc/device-tree/compatible 2>/dev/null | tr "\n" " ")"
  echo "[runtime] compatible=$COMPAT"
fi
echo "[runtime] DISPLAY_WIDTH=${DISPLAY_WIDTH:-unknown} DISPLAY_HEIGHT=${DISPLAY_HEIGHT:-unknown}"

PAYLOAD="$GAMEDIR/game/game.s3e.unpacked"
S3E="$GAMEDIR/game/The Sims 3.s3e"
ASSET_DIR="$GAMEDIR/game/assets"

# 6) Permissões dos componentes do runtime.
for f in   "$GAMEDIR/run.sh"   "$GAMEDIR/run-extractor.sh"   "$GAMEDIR/nxextract/run-extractor.sh"   "$GAMEDIR/nxextract/nxextract-runtime-env.sh"   "$GAMEDIR/nxextract/nxextract-ui"   "$GAMEDIR/sims3_s3e_loader"
do
  [ -f "$f" ] && chmod +x "$f" 2>/dev/null || true
done

# 7) Instalação transacional: somente executar NXExtract quando necessário.
NEED_EXTRACT=0
[ -s "$S3E" ] || NEED_EXTRACT=1
[ -s "$PAYLOAD" ] || NEED_EXTRACT=1
[ -f "$ASSET_DIR/app.icf" ] || NEED_EXTRACT=1
[ -f "$ASSET_DIR/s3e.icf" ] || NEED_EXTRACT=1

if [ "$NEED_EXTRACT" -eq 1 ]; then
  echo "[runtime] dados incompletos; iniciando NXExtract."
  [ -f "$GAMEDIR/extractor.json" ] || { echo "[ERROR] extractor.json ausente"; exit 72; }
  [ -x "$GAMEDIR/run-extractor.sh" ] || { echo "[ERROR] run-extractor.sh ausente/não executável"; exit 72; }
  [ -f "$GAMEDIR/nxextract/nxextract.py" ] || { echo "[ERROR] nxextract/nxextract.py ausente"; exit 72; }

  NX_FIRMWARE_LIBS=""
  for _d in     "$controlfolder/libs"     "$controlfolder/libs.armhf"     /opt/system/Tools/PortMaster/libs     /opt/tools/PortMaster/libs     /roms/ports/PortMaster/libs     /storage/roms/ports/PortMaster/libs     /usr/lib32 /lib32 /usr/lib /lib
  do
    if [ -d "$_d" ]; then
      NX_FIRMWARE_LIBS="${NX_FIRMWARE_LIBS:+$NX_FIRMWARE_LIBS:}$_d"
    fi
  done

  echo "[runtime] NXEXTRACT_GAME_DIR=$GAMEDIR"
  echo "[runtime] NXEXTRACT_FIRMWARE_LIBRARY_PATH=$NX_FIRMWARE_LIBS"

  if [ -n "$NX_FIRMWARE_LIBS" ]; then
    NXEXTRACT_GAME_DIR="$GAMEDIR"     NXEXTRACT_FIRMWARE_LIBRARY_PATH="$NX_FIRMWARE_LIBS"     bash "$GAMEDIR/run-extractor.sh"
  else
    NXEXTRACT_GAME_DIR="$GAMEDIR" bash "$GAMEDIR/run-extractor.sh"
  fi

  status=$?
  echo "[runtime] NXExtract exit=$status"
  [ "$status" -eq 0 ] || exit "$status"
else
  echo "[runtime] S3E + payload + assets já presentes; NXExtract não será executado."
fi

# 8) Validar o resultado antes de iniciar o jogo.
[ -s "$S3E" ] || { echo "[ERROR] S3E ausente: $S3E"; exit 73; }
[ -s "$PAYLOAD" ] || { echo "[ERROR] payload ausente: $PAYLOAD"; exit 73; }
[ -f "$ASSET_DIR/app.icf" ] || { echo "[ERROR] app.icf ausente"; exit 73; }
[ -f "$ASSET_DIR/s3e.icf" ] || { echo "[ERROR] s3e.icf ausente"; exit 73; }
[ -x "$GAMEDIR/sims3_s3e_loader" ] || { echo "[ERROR] loader não executável"; exit 74; }

# 9) Resolução negociada com o CFW, com fallback 640x480.
if [ -z "${SIMS3_W:-}" ] || [ -z "${SIMS3_H:-}" ]; then
  SIMS3_W="${DISPLAY_WIDTH:-640}"
  SIMS3_H="${DISPLAY_HEIGHT:-480}"
fi
case "$SIMS3_W" in ""|*[!0-9]*) SIMS3_W=640 ;; esac
case "$SIMS3_H" in ""|*[!0-9]*) SIMS3_H=480 ;; esac
export SIMS3_W SIMS3_H

echo "[runtime] surface=${SIMS3_W}x${SIMS3_H}"
echo "[runtime] payload=$PAYLOAD"
echo "[runtime] assets=$ASSET_DIR"

echo "[runtime] game directory listing:"
ls -la "$GAMEDIR/game" 2>/dev/null || true
echo "[runtime] assets listing:"
ls -la "$ASSET_DIR" 2>/dev/null | head -n 80 || true

for required in "$GAMEDIR/game/dlc.dz" "$GAMEDIR/game/assets/LowRes/res.dz"; do
  if [ -f "$required" ]; then
    echo "[runtime] data OK: $required"
  else
    echo "[WARN] game data missing: $required"
  fi
done

# 10) Handoff PortMaster.
# Igual ao padrão documentado: helper antes do binário; sem exec e sem gerenciamento
# do frontend pelo port.
if command -v pm_platform_helper >/dev/null 2>&1; then
  echo "[runtime] pm_platform_helper: $GAMEDIR/sims3_s3e_loader"
  pm_platform_helper "$GAMEDIR/sims3_s3e_loader" || echo "[WARN] pm_platform_helper retornou $?"
else
  echo "[WARN] pm_platform_helper não encontrado"
fi

echo "[runtime] starting ARM32 S3E loader"
"$GAMEDIR/sims3_s3e_loader" --run --root "$GAMEDIR/game" "$PAYLOAD"
status=$?
echo "[runtime] loader exit=$status"
exit "$status"
