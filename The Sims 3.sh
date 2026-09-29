#!/usr/bin/env bash
# The Sims 3 — PortMaster launcher
# /ports/The Sims 3.sh
# /ports/sims3/

set -u

SCRIPT_DIR="$(cd -- "$(dirname -- "$0" 2>/dev/null)" && pwd -P)" || exit 1
LOGDIR="${SIMS3_LOG_DIR:-$SCRIPT_DIR/logs}"
mkdir -p "$LOGDIR" 2>/dev/null || true
LOG="${SIMS3_LAUNCHER_LOG:-$LOGDIR/The Sims 3.log}"
exec >>"$LOG" 2>&1

echo "=== The Sims 3 / PortMaster launcher ==="
echo "[launcher] script=$SCRIPT_DIR"
echo "[launcher] date=$(date 2>/dev/null || true)"

GAMEDIR="$SCRIPT_DIR/sims3"
if [ -n "${SIMS3_GAME_DIR:-}" ]; then
  GAMEDIR="$SIMS3_GAME_DIR"
fi
echo "[launcher] game=$GAMEDIR"

if [ ! -d "$GAMEDIR" ]; then
  echo "[ERROR] pasta do port não encontrada: $GAMEDIR"
  exit 1
fi

cd "$GAMEDIR" || exit 1
if [ ! -f "$GAMEDIR/r36s.run.sh" ]; then
  echo "[ERROR] r36s.run.sh não encontrado em $GAMEDIR"
  exit 1
fi

chmod +x "$GAMEDIR/r36s.run.sh" 2>/dev/null || true
export SIMS3_GAME_DIR="$GAMEDIR"
echo "[launcher] iniciando r36s.run.sh"
exec "$GAMEDIR/r36s.run.sh"
