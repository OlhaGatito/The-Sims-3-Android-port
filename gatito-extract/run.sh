#!/bin/bash
# The Sims 3 — Gatito Extractor UI launche
# Called by the PortMaster runtime after the port has been installed.
# The UI requires the recipe, game directory, engine and log; always provide them.
set +e

GAMEDIR="${NXEXTRACT_GAME_DIR:-$(CDPATH= cd -- "$(dirname -- "$0")/.." 2>/dev/null && pwd -P)}"
RECIPE="${NXEXTRACT_RECIPE:-$GAMEDIR/extractor.json}"
PYTHON_BIN="${NXEXTRACT_PYTHON:-python3}"
UI="$GAMEDIR/gatito-extract/BUILD.ui/gatito-ui.py"
ENGINE="${NXEXTRACT_ENGINE:-$GAMEDIR/gatito-extract/gatito-extract-v3.py}"
LOGDIR="${SIMS3_LOG_DIR:-$GAMEDIR/logs}"
LOG="${SIMS3_EXTRACTOR_LOG:-$LOGDIR/gatito-extract-ui.log}"

mkdir -p "$LOGDIR" 2>/dev/null
touch "$LOG" 2>/dev/null || LOG="${TMPDIR:-/tmp}/sims3-gatito-extract.log"

{
    echo ""
    echo "=== The Sims 3 / Gatito UI launcher ==="
    echo "DATE=$(date 2>/dev/null || true)"
    echo "GAMEDIR=$GAMEDIR"
    echo "RECIPE=$RECIPE"
    echo "UI=$UI"
    echo "ENGINE=$ENGINE"
    echo "PYTHON=$PYTHON_BIN"
    echo "LOG=$LOG"
    echo "CFW=${CFW_NAME:-unknown}"
    echo "DEVICE=${DEVICE_NAME:-unknown}"
    echo "ARCH=${DEVICE_ARCH:-$(uname -m 2>/dev/null || true)}"
    echo "PWD=$PWD"
} >>"$LOG" 2>&1

fail() {
    rc="$1"
    shift
    echo "[Gatito] ERROR: $*" >>"$LOG" 2>&1
    echo "[Gatito] ERROR: $*" >&2
    exit "$rc"
}

[ -d "$GAMEDIR" ] || fail 72 "game directory not found: $GAMEDIR"
[ -f "$RECIPE" ] || fail 72 "recipe not found: $RECIPE"
[ -f "$UI" ] || fail 72 "UI not found: $UI"
[ -f "$ENGINE" ] || fail 72 "engine not found: $ENGINE"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail 69 "Python 3 not found: $PYTHON_BIN"

{
    echo "[Gatito] all launch prerequisites found"
    echo "[Gatito] starting graphical UI"
} >>"$LOG" 2>&1

export GAMEDIR
export NXEXTRACT_GAME_DIR="$GAMEDIR"
export NXEXTRACT_RECIPE="$RECIPE"
export NXEXTRACT_ENGINE="$ENGINE"
export SIMS3_EXTRACTOR_LOG="$LOG"

"$PYTHON_BIN" "$UI" --game-dir "$GAMEDIR" --recipe "$RECIPE" --engine "$ENGINE" --log "$LOG" "$@"
RC=$?

echo "[Gatito] UI exit code=$RC" >>"$LOG" 2>&1
exit "$RC"
