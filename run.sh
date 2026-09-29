#!/bin/sh
# The Sims 3 — PortMaster multi-device runtime
# Foco: PortMaster + ARM32 hard-float; perfil gráfico/áudio negociado pelo firmware;
# NXExtract antes do loader; ambiente gráfico/áudio fornecido pelo firmware.

set +u
# O control.txt do PortMaster pode consultar variáveis ausentes; manter o shell tolerante.
GAMEDIR="$(cd "$(dirname "$0")" 2>/dev/null && pwd -P)" || exit 1
cd "$GAMEDIR" || exit 1
LOGDIR="${SIMS3_LOG_DIR:-$GAMEDIR/logs}"
mkdir -p "$LOGDIR" || exit 1
LOG="${SIMS3_LOG:-$LOGDIR/r36s.run.log}"
exec >>"$LOG" 2>&1

echo "=== The Sims 3 / RK3326 runtime ==="
echo "[runtime] gamedir=$GAMEDIR"
# PortMaster/CFW: usar o mesmo handoff leve do NextOS.
# O frontend continua dono do ciclo de vida; este runtime apenas prepara e executa o jogo.
XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
controlfolder=""
for _cf in /opt/system/Tools/PortMaster /opt/tools/PortMaster \
           "$XDG_DATA_HOME/PortMaster" /roms/ports/PortMaster \
           /storage/.config/PortMaster; do
  [ -d "$_cf" ] && { controlfolder="$_cf"; break; }
done
: "${controlfolder:=/storage/.config/PortMaster}"
if [ -f "$controlfolder/control.txt" ]; then
  . "$controlfolder/control.txt"
  case "${CFW_NAME:-}" in
    ''|*[!A-Za-z0-9._-]*) ;;
    *) [ -f "$controlfolder/mod_${CFW_NAME}.txt" ] && . "$controlfolder/mod_${CFW_NAME}.txt" ;;
  esac
  command -v get_controls >/dev/null 2>&1 && get_controls || true
fi
: "${ESUDO:=}"
printf '[runtime] cfw=%s controlfolder=%s\n' "${CFW_NAME:-unknown}" "$controlfolder"

# Bibliotecas do firmware/PortMaster primeiro. O loader S3E abre suas dependências
# próprias explicitamente; não devemos interpor libs Android sobre a libc do CFW.
LD_PARTS="/usr/lib32:/lib32:/usr/lib:/lib"
[ -n "$controlfolder" ] && LD_PARTS="$controlfolder/libs:$controlfolder/libs.armhf:$LD_PARTS"
[ -d "$GAMEDIR/libs" ] && LD_PARTS="$GAMEDIR/libs:$LD_PARTS"
export LD_LIBRARY_PATH="$LD_PARTS${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
printf '[runtime] LD_LIBRARY_PATH=%s\n' "$LD_LIBRARY_PATH"

# Política de áudio adaptada do NextOS:
# AUDIO_DRIVER vazio = SDL escolhe o backend disponível no firmware.
# AUDIO_DRIVER=alsa/pulse/pipewire pode ser usado para diagnóstico.
case "${AUDIO_DRIVER:-}" in
  '')
    unset SDL_AUDIODRIVER 2>/dev/null || true
    echo '[audio] backend automático: SDL/firmware'
    ;;
  alsa)
    export SDL_AUDIODRIVER=alsa
    echo '[audio] AUDIO_DRIVER=alsa -> SDL_AUDIODRIVER=alsa'
    ;;
  pulse)
    export SDL_AUDIODRIVER=pulseaudio
    echo '[audio] AUDIO_DRIVER=pulse -> SDL_AUDIODRIVER=pulseaudio'
    ;;
  pipewire)
    export SDL_AUDIODRIVER=pipewire
    echo '[audio] AUDIO_DRIVER=pipewire -> SDL_AUDIODRIVER=pipewire'
    ;;
  *)
    echo '[audio] ERRO: AUDIO_DRIVER deve ser vazio, alsa, pulse ou pipewire'
    exit 76
    ;;
esac
if [ -d /dev/snd ]; then
  echo '[audio] /dev/snd presente'
  ls -la /dev/snd 2>/dev/null || true
else
  echo '[audio] AVISO: /dev/snd ausente'
fi
if [ -r /proc/asound/cards ]; then
  echo '[audio] ALSA cards:'
  cat /proc/asound/cards 2>/dev/null || true
fi

echo "[runtime] cfw=${CFW_NAME:-unknown}"
echo "[runtime] arch=$(uname -m 2>/dev/null || echo unknown)"
echo "[runtime] kernel=$(uname -r 2>/dev/null || echo unknown)"
if [ -f /proc/device-tree/compatible ]; then
  COMPAT="$(tr "\000" "\n" < /proc/device-tree/compatible 2>/dev/null | tr "\n" " ")"
  echo "[runtime] compatible=$COMPAT"
fi

PAYLOAD="$GAMEDIR/game/game.s3e.unpacked"
S3E="$GAMEDIR/game/The Sims 3.s3e"
ASSET_DIR="$GAMEDIR/game/assets"
ASSET_MARKER_1="$ASSET_DIR/app.icf"
ASSET_MARKER_2="$ASSET_DIR/s3e.icf"

for f in "$GAMEDIR/run.sh" "$GAMEDIR/run-extractor.sh" "$GAMEDIR/nxextract/run-extractor.sh" "$GAMEDIR/nxextract/nxextract-runtime-env.sh" "$GAMEDIR/nxextract/nxextract-ui" "$GAMEDIR/sims3_s3e_loader"; do
  [ -f "$f" ] && chmod +x "$f" 2>/dev/null || true
done

NEED_EXTRACT=0
[ -s "$S3E" ] || NEED_EXTRACT=1
[ -s "$PAYLOAD" ] || NEED_EXTRACT=1
[ -f "$ASSET_MARKER_1" ] || NEED_EXTRACT=1
[ -f "$ASSET_MARKER_2" ] || NEED_EXTRACT=1

if [ "$NEED_EXTRACT" -eq 1 ]; then
  echo "[runtime] dados requeridos ausentes/incompletos; iniciando NXExtract."
  echo "[runtime] required s3e=$S3E"
  echo "[runtime] required payload=$PAYLOAD"
  echo "[runtime] required assets=$ASSET_DIR"
  echo "[runtime] required markers=$ASSET_MARKER_1 ; $ASSET_MARKER_2"

  [ -f "$GAMEDIR/extractor.json" ] || { echo "[ERROR] extractor.json não encontrado."; exit 72; }
  [ -x "$GAMEDIR/run-extractor.sh" ] || { echo "[ERROR] run-extractor.sh não encontrado/executável."; exit 72; }
  [ -f "$GAMEDIR/nxextract/nxextract.py" ] || { echo "[ERROR] nxextract/nxextract.py não encontrado."; exit 72; }
  [ -f "$GAMEDIR/nxextract/nxextract-runtime-env.sh" ] || { echo "[ERROR] nxextract/nxextract-runtime-env.sh não encontrado."; exit 72; }

  if [ ! -f "$GAMEDIR/nxextract/nxextract-ui" ]; then
    echo "[runtime] nxextract-ui não empacotado; NXExtract usará modo automático/headless."
  fi

  NX_FIRMWARE_LIBS=""
  for d in "/opt/system/Tools/PortMaster/libs" "/opt/tools/PortMaster/libs" "/roms/ports/PortMaster/libs" "/roms/ports/PortMaster/runtime" "/storage/roms/ports/PortMaster/libs" "/usr/lib" "/lib"; do
    if [ -d "$d" ]; then
      if [ -n "$NX_FIRMWARE_LIBS" ]; then NX_FIRMWARE_LIBS="$NX_FIRMWARE_LIBS:$d"; else NX_FIRMWARE_LIBS="$d"; fi
    fi
  done

  echo "[runtime] NXEXTRACT_GAME_DIR=$GAMEDIR"
  echo "[runtime] NXEXTRACT_FIRMWARE_LIBRARY_PATH=$NX_FIRMWARE_LIBS"

  if [ -n "$NX_FIRMWARE_LIBS" ]; then
    NXEXTRACT_GAME_DIR="$GAMEDIR" NXEXTRACT_FIRMWARE_LIBRARY_PATH="$NX_FIRMWARE_LIBS" bash "$GAMEDIR/run-extractor.sh"
  else
    NXEXTRACT_GAME_DIR="$GAMEDIR" bash "$GAMEDIR/run-extractor.sh"
  fi

  status=$?
  echo "[runtime] NXExtract exit=$status"
  [ "$status" -eq 0 ] || exit "$status"
else
  echo "[runtime] S3E + payload + assets requeridos já presentes; NXExtract não será executado."
fi

[ -s "$S3E" ] || { echo "[ERROR] NXExtract terminou sem produzir $S3E"; exit 73; }
[ -s "$PAYLOAD" ] || { echo "[ERROR] NXExtract terminou sem produzir $PAYLOAD"; exit 73; }
[ -f "$ASSET_MARKER_1" ] || { echo "[ERROR] NXExtract terminou sem produzir $ASSET_MARKER_1"; exit 73; }
[ -f "$ASSET_MARKER_2" ] || { echo "[ERROR] NXExtract terminou sem produzir $ASSET_MARKER_2"; exit 73; }
[ -x "$GAMEDIR/sims3_s3e_loader" ] || { echo "[ERROR] sims3_s3e_loader não está executável."; exit 74; }

if [ -z "${SIMS3_W:-}" ] || [ -z "${SIMS3_H:-}" ]; then
  fb_w=""; fb_h=""
  if [ -r /sys/class/graphics/fb0/modes ]; then
    mode="$(head -n1 /sys/class/graphics/fb0/modes 2>/dev/null)"
    fb_w="$(echo "$mode" | sed -n "s/.*[:_]\([0-9]\{3,4\}\)x\([0-9]\{3,4\}\).*/\1/p")"
    fb_h="$(echo "$mode" | sed -n "s/.*[:_]\([0-9]\{3,4\}\)x\([0-9]\{3,4\}\).*/\2/p")"
  fi
  case "$fb_w" in ""|*[!0-9]*) fb_w=640 ;; esac
  case "$fb_h" in ""|*[!0-9]*) fb_h=480 ;; esac
  SIMS3_W="$fb_w"; SIMS3_H="$fb_h"
fi
export SIMS3_W SIMS3_H

echo "[runtime] surface=${SIMS3_W}x${SIMS3_H}"
echo "[runtime] logs=$LOGDIR"
echo "[runtime] payload=$PAYLOAD"
echo "[runtime] assets=$ASSET_DIR"
# Áudio: seguir o firmware por padrão, como os ports NextOS fazem com SDL/OpenAL.
# Não forçar ALSA/PipeWire/PulseAudio por padrão: isso quebra CFWs diferentes.
# Para diagnóstico/teste, SIMS3_AUDIO_DRIVER pode ser usado explicitamente.
if [ -n "${SIMS3_AUDIO_DRIVER:-}" ]; then
  export SDL_AUDIODRIVER="$SIMS3_AUDIO_DRIVER"
  echo "[audio] SIMS3_AUDIO_DRIVER=$SIMS3_AUDIO_DRIVER -> SDL_AUDIODRIVER=$SDL_AUDIODRIVER"
else
  unset SDL_AUDIODRIVER 2>/dev/null || true
  echo "[audio] backend SDL: automático (firmware/SDL escolhe)"
fi
if [ -d /dev/snd ]; then
  echo "[audio] /dev/snd presente"
  ls -la /dev/snd 2>/dev/null || true
else
  echo "[audio] AVISO: /dev/snd não está presente"
fi
if [ -r /proc/asound/cards ]; then
  echo "[audio] ALSA cards:"
  cat /proc/asound/cards 2>/dev/null || true
else
  echo "[audio] /proc/asound/cards não disponível"
fi

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


# Handoff PortMaster: não gerencia frontend nem mata processos.
# Isso fica no PortMaster/CFW, como no runtime oficial NextOS.
if command -v pm_platform_helper >/dev/null 2>&1; then
  pm_platform_helper "$GAMEDIR/sims3_s3e_loader" >/dev/null 2>&1 || true
fi

echo "[runtime] starting ARM32 S3E loader"
"$GAMEDIR/sims3_s3e_loader" --run --root "$GAMEDIR/game" "$PAYLOAD"
status=$?
echo "[runtime] loader exit=$status"
exit "$status"
