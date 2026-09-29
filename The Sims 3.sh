#!/usr/bin/env bash
# The Sims 3 — PortMaster launcher
# /ports/The Sims 3.sh
# /ports/sims3/

set -u

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" 2>/dev/null && pwd -P)" || exit 1
GAMEDIR="$SCRIPT_DIR/sims3"

if [ -n "${SIMS3_GAME_DIR:-}" ]; then
  GAMEDIR="$SIMS3_GAME_DIR"
fi

if [ ! -d "$GAMEDIR" ]; then
  echo "[ERROR] pasta do port não encontrada: $GAMEDIR"
  exit 1
fi

cd "$GAMEDIR" || exit 1

# O runtime RK3326/R36S contém o gate NXExtract e a execução do loader.
if [ ! -f "$GAMEDIR/r36s.run.sh" ]; then
  echo "[ERROR] r36s.run.sh não encontrado em $GAMEDIR"
  exit 1
fi

chmod +x "$GAMEDIR/r36s.run.sh" 2>/dev/null || true

exec "$GAMEDIR/r36s.run.sh"
