#!/bin/bash
# The Sims 3 — Gatito Extractor UI launcher
# Called by the PortMaster runtime after the port has been installed.
# The UI requires both the recipe and game directory; always provide them.
set -e

GAMEDIR="${NXEXTRACT_GAME_DIR:-$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)}"
RECIPE="${NXEXTRACT_RECIPE:-$GAMEDIR/extractor.json}"
PYTHON_BIN="${NXEXTRACT_PYTHON:-python3}"
UI="$GAMEDIR/gatito-extract/BUILD.ui/gatito-ui.py"

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
command -v "$PYTHON_BIN" >/dev/null 2>&1 || {
    echo "[Gatito] Python 3 not found: $PYTHON_BIN" >&2
    exit 69
}

export GAMEDIR
export NXEXTRACT_GAME_DIR="$GAMEDIR"
export NXEXTRACT_RECIPE="$RECIPE"

exec "$PYTHON_BIN" "$UI" "$RECIPE" --game-dir "$GAMEDIR" "$@"
