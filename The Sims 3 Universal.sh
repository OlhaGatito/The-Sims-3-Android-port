#!/usr/bin/env bash
# The Sims 3 — launcher universal de localização
# Compatibilidade auxiliar; a entrada normal do PortMaster é "The Sims 3.sh".

set -u
SCRIPT_DIR="$(cd -- "$(dirname -- "$0" 2>/dev/null)" && pwd -P)" || exit 1
LOGDIR_FALLBACK="${SIMS3_LOG_DIR:-$SCRIPT_DIR/logs}"
mkdir -p "$LOGDIR_FALLBACK" 2>/dev/null || true
LOG_FALLBACK="$LOGDIR_FALLBACK/The Sims 3 Universal.log"
exec >>"$LOG_FALLBACK" 2>&1

echo "=== The Sims 3 / universal launcher ==="
echo "[launcher] script=$SCRIPT_DIR"
echo "[launcher] date=$(date 2>/dev/null || true)"

find_game_dir() {
  if [ -n "${SIMS3_GAME_DIR:-}" ] && [ -d "$SIMS3_GAME_DIR" ]; then
    cd "$SIMS3_GAME_DIR" && pwd -P
    return 0
  fi
  for candidate in "$SCRIPT_DIR/sims3" "$SCRIPT_DIR/The Sims 3" "/mnt/mmc/roms/ports/sims3" "/mnt/mmc/roms/ports/The Sims 3" "/mnt/sdcard/Roms/ports/sims3" "/mnt/sdcard/Roms/ports/The Sims 3" "/roms/ports/sims3" "/roms/ports/The Sims 3" "/storage/roms/ports/sims3" "/storage/roms/ports/The Sims 3"; do
    if [ -d "$candidate" ]; then
      cd "$candidate" 2>/dev/null && pwd -P
      return 0
    fi
  done
  for base in /mnt/mmc/roms/ports /mnt/sdcard/Roms/ports /roms/ports /storage/roms/ports; do
    [ -d "$base" ] || continue
    candidate="$(find "$base" -maxdepth 2 -type f -name "sims3_s3e_loader" -print -quit 2>/dev/null)"
    if [ -n "$candidate" ]; then dirname "$candidate"; return 0; fi
  done
  return 1
}

GAMEDIR="$(find_game_dir)" || {
  echo "[ERROR] pasta do The Sims 3 não encontrada."
  echo "[INFO] use SIMS3_GAME_DIR=/caminho/para/sims3."
  exit 1
}

echo "[launcher] game=$GAMEDIR"
LOGDIR="${SIMS3_LOG_DIR:-$GAMEDIR/logs}"
mkdir -p "$LOGDIR" || exit 1
LOG="$LOGDIR/The Sims 3 Universal.log"
exec >>"$LOG" 2>&1
echo "[launcher] log=$LOG"

if [ ! -f "$GAMEDIR/run.sh" ]; then
  echo "[ERROR] run.sh não encontrado em $GAMEDIR"
  exit 75
fi

chmod +x "$GAMEDIR/run.sh" 2>/dev/null || true
export SIMS3_GAME_DIR="$GAMEDIR"
echo "[launcher] iniciando run.sh"
exec "$GAMEDIR/run.sh"
