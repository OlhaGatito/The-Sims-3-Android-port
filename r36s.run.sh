#!/bin/sh
# The Sims 3 — RK3326 / R36S runtime
# Foco: R36S/R35S e outros handhelds RK3326; ARM32 hard-float; PortMaster;
# NXExtract antes do loader; ambiente gráfico/áudio fornecido pelo firmware.

set -u
GAMEDIR="$(cd "$(dirname "$0")" 2>/dev/null && pwd -P)" || exit 1
cd "$GAMEDIR" || exit 1
LOGDIR="${SIMS3_LOG_DIR:-$GAMEDIR/logs}"
mkdir -p "$LOGDIR" || exit 1
LOG="${SIMS3_LOG:-$LOGDIR/r36s.run.log}"
exec >>"$LOG" 2>&1
echo "=== The Sims 3 / RK3326 runtime ==="
echo "[runtime] gamedir=$GAMEDIR"
echo "[runtime] cfw=${CFW_NAME:-unknown}"
echo "[runtime] arch=$(uname -m 2>/dev/null || echo unknown)"
echo "[runtime] kernel=$(uname -r 2>/dev/null || echo unknown)"
if [ -f /proc/device-tree/compatible ]; then
  COMPAT="$(tr "\000" "\n" < /proc/device-tree/compatible 2>/dev/null | tr "\n" " ")"
  echo "[runtime] compatible=$COMPAT"
fi
PAYLOAD="$GAMEDIR/game/game.s3e.unpacked"
for f in "$GAMEDIR/r36s.run.sh" "$GAMEDIR/run-extractor.sh" "$GAMEDIR/nxextract/run-extractor.sh" "$GAMEDIR/nxextract/nxextract-runtime-env.sh" "$GAMEDIR/nxextract/nxextract-ui" "$GAMEDIR/sims3_s3e_loader"; do
  [ -f "$f" ] && chmod +x "$f" 2>/dev/null || true
done
if [ ! -s "$PAYLOAD" ]; then
  echo "[runtime] payload ausente; iniciando NXExtract."
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
fi
[ -s "$PAYLOAD" ] || { echo "[ERROR] NXExtract terminou sem produzir $PAYLOAD"; exit 73; }
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
if [ -d "$GAMEDIR/game" ]; then
  echo "[runtime] game directory listing:"
  ls -la "$GAMEDIR/game" 2>/dev/null || true
  for required in "$GAMEDIR/game/dlc.dz" "$GAMEDIR/game/LowRes/res.dz"; do
    if [ -f "$required" ]; then
      echo "[runtime] data OK: $required"
    else
      echo "[WARN] game data missing: $required"
    fi
  done
fi
echo "[runtime] starting ARM32 S3E loader"
"$GAMEDIR/sims3_s3e_loader" --run --root "$GAMEDIR/game" "$PAYLOAD"
status=$?
echo "[runtime] loader exit=$status"
exit "$status"
