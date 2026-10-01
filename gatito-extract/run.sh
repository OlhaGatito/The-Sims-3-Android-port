#!/bin/bash
# The Sims 3 — Gatito Extractor UI launcher
# Called by the PortMaster runtime after the port has been installed.
# The UI requires the recipe, game directory, engine and log; always provide them.
set -e

GAMEDIR="${NXEXTRACT_GAME_DIR:-$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)}"
RECIPE="${NXEXTRACT_RECIPE:-$GAMEDIR/extractor.json}"
PYTHON_BIN="${NXEXTRACT_PYTHON:-python3}"
UI="$GAMEDIR/gatito-extract/BUILD.ui/gatito-ui.py"
ENGINE="${NXEXTRACT_ENGINE:-$GAMEDIR/gatito-extract/gatito-extract-v3.py}"
LOGDIR="${SIMS3_LOG_DIR:-$GAMEDIR/logs}"
LOG="${SIMS3_EXTRACTOR_LOG:-$LOGDIR/gatito-extract-ui.log}"

[ -d "$GAMEDIR" ] || {
    echo "[Gatito] game directory not found: $GAMEDIR" >&2
    exit 72
}
[ -f "$RECIPE" ] || {
    echo "[Gatito] recipe not found: $RECIPE" >&2
    exit 72
}
[ -f "$UI" ] || {
    echo "[Gatito] UI not found: $UI" >&2
    exit 72
}
[ -f "$ENGINE" ] || {
    echo "[Gatito] engine not found: $ENGINE" >&2
    exit 72
}
mkdir -p "$LOGDIR"

command -v "$PYTHON_BIN" >/dev/null 2>&1 || {
    echo "[Gatito] Python 3 not found: $PYTHON_BIN" >&2
    exit 69
}

export GAMEDIR
export NXEXTRACT_GAME_DIR="$GAMEDIR"
export NXEXTRACT_RECIPE="$RECIPE"
export NXEXTRACT_ENGINE="$ENGINE"
export SIMS3_EXTRACTOR_LOG="$LOG"

exec "$PYTHON_BIN" "$UI" "$RECIPE" --game-dir "$GAMEDIR" --engine "$ENGINE" --log "$LOG" "$@"
