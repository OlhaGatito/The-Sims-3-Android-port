#!/bin/sh
# The Sims 3 — PortMaster runtime
#
# O run.sh é o ponto de entrada do runtime. Antes de executar o loader,
# garante que a preparação de dados passe pelo runner oficial do NXExtract.

set -u

GAMEDIR="$(cd "$(dirname "$0")" 2>/dev/null && pwd -P)" || exit 1
cd "$GAMEDIR" || exit 1

LOG="${SIMS3_LOG:-$GAMEDIR/run.log}"
exec >>"$LOG" 2>&1

echo "=== The Sims 3 / runtime ==="
echo "[runtime] gamedir=$GAMEDIR"
echo "[runtime] cfw=${CFW_NAME:-unknown}"
echo "[runtime] arch=$(uname -m 2>/dev/null || echo unknown)"

# Não exigir game/assets antes do NXExtract.
# O objetivo desta etapa é justamente preparar os dados fornecidos pelo usuário.
PAYLOAD="$GAMEDIR/game/game.s3e.unpacked"

for f in "$GAMEDIR/run-extractor.sh" "$GAMEDIR/nxextract-runtime-env.sh" "$GAMEDIR/nxextract.py" "$GAMEDIR/nxextract-ui" "$GAMEDIR/sims3_s3e_loader"; do
  [ -f "$f" ] && chmod +x "$f" 2>/dev/null || true
done

if [ ! -s "$PAYLOAD" ]; then
  echo "[runtime] payload ausente; iniciando NXExtract."

  if [ ! -f "$GAMEDIR/extractor.json" ]; then
    echo "[ERROR] extractor.json não encontrado."
    echo "[ERROR] NXExtract não pode ser iniciado sem a receita."
    exit 72
  fi

  if [ ! -x "$GAMEDIR/run-extractor.sh" ]; then
    echo "[ERROR] run-extractor.sh não encontrado/executável."
    exit 72
  fi

  if [ ! -f "$GAMEDIR/nxextract.py" ] || [ ! -f "$GAMEDIR/nxextract-runtime-env.sh" ] || [ ! -f "$GAMEDIR/nxextract-ui" ]; then
    echo "[ERROR] cadeia NXExtract incompleta."
    echo "[ERROR] necessários: nxextract.py, nxextract-runtime-env.sh e nxextract-ui."
    exit 72
  fi

  NX_FIRMWARE_LIBS=""
  for d in "/opt/system/Tools/PortMaster/libs" "/opt/tools/PortMaster/libs" "/usr/local/lib/aarch64-linux-gnu" "/usr/local/lib" "/usr/lib/aarch64-linux-gnu" "/lib/aarch64-linux-gnu" "/usr/lib" "/lib"; do
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

if [ ! -s "$PAYLOAD" ]; then
  echo "[ERROR] NXExtract terminou sem produzir: $PAYLOAD"
  exit 73
fi

if [ -z "${SIMS3_W:-}" ] || [ -z "${SIMS3_H:-}" ]; then
  fb_w=""; fb_h=""
  if [ -r /sys/class/graphics/fb0/modes ]; then
    mode=$(head -n1 /sys/class/graphics/fb0/modes 2>/dev/null)
    fb_w=$(echo "$mode" | sed -n "s/.*[:_]\([0-9]\{3,4\}\)x\([0-9]\{3,4\}\).*/\1/p")
    fb_h=$(echo "$mode" | sed -n "s/.*[:_]\([0-9]\{3,4\}\)x\([0-9]\{3,4\}\).*/\2/p")
  fi
  case "$fb_w" in ""|*[!0-9]*) fb_w=640 ;; esac
  case "$fb_h" in ""|*[!0-9]*) fb_h=480 ;; esac
  [ "$fb_w" -ge 320 ] 2>/dev/null && [ "$fb_w" -le 4096 ] 2>/dev/null || fb_w=640
  [ "$fb_h" -ge 240 ] 2>/dev/null && [ "$fb_h" -le 4096 ] 2>/dev/null || fb_h=480
  SIMS3_W=$fb_w; SIMS3_H=$fb_h
fi
export SIMS3_W SIMS3_H
echo "[runtime] surface=${SIMS3_W}x${SIMS3_H}"
echo "[runtime] starting loader"
exec "$GAMEDIR/sims3_s3e_loader" --run --root "$GAMEDIR/game" "$PAYLOAD"
