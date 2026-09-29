#!/usr/bin/env bash
# The Sims 3 — PortMaster launcher
# Wrapper fino: localiza o runtime e entrega toda a execução a run.sh.

set -u

resolve_self() {
  local src="$1" dir base link
  while [ -L "$src" ]; do
    dir="$(cd -P -- "$(dirname -- "$src")" 2>/dev/null && pwd -P)" || return 1
    link="$(readlink "$src" 2>/dev/null)" || return 1
    case "$link" in
      /*) src="$link" ;;
      *)  src="$dir/$link" ;;
    esac
  done
  dir="$(cd -P -- "$(dirname -- "$src")" 2>/dev/null && pwd -P)" || return 1
  base="$(basename -- "$src")"
  printf '%s/%s\n' "$dir" "$base"
}

SELF="$(resolve_self "$0" 2>/dev/null || true)"
[ -n "$SELF" ] || SELF="$0"
SCRIPT_DIR="$(cd -P -- "$(dirname -- "$SELF")" 2>/dev/null && pwd -P)" || exit 1

LOGDIR="\${SIMS3_LOG_DIR:-$SCRIPT_DIR/logs}"
mkdir -p "$LOGDIR" 2>/dev/null || true
LOG="\${SIMS3_LAUNCHER_LOG:-$LOGDIR/The Sims 3 launcher.log}"
if ! touch "$LOG" 2>/dev/null; then
  LOG="\${TMPDIR:-/tmp}/The Sims 3 launcher.log"
  touch "$LOG" 2>/dev/null || true
fi
exec >>"$LOG" 2>&1

echo "=== The Sims 3 / PortMaster launcher ==="
echo "[launcher] date=$(date 2>/dev/null || true)"
echo "[launcher] invoked=$0"
echo "[launcher] real_script=$SELF"
echo "[launcher] script_dir=$SCRIPT_DIR"
echo "[launcher] pwd=$(pwd 2>/dev/null || true)"
echo "[launcher] args=$*"

if [ -n "\${SIMS3_GAME_DIR:-}" ] && [ -f "\${SIMS3_GAME_DIR:-}/run.sh" ]; then
  GAMEDIR="$SIMS3_GAME_DIR"
else
  GAMEDIR=""
  CANDIDATES=(
    "$SCRIPT_DIR/sims3"
    "$SCRIPT_DIR/../ports/sims3"
    "$SCRIPT_DIR/../../ports/sims3"
    "/mnt/sdcard/ports/sims3"
    "/mnt/mmc/ports/sims3"
    "/roms/ports/sims3"
    "/roms2/ports/sims3"
    "/storage/roms/ports/sims3"
    "/userdata/roms/ports/sims3"
    "/userdata/system/roms/ports/sims3"
  )

  case "$SCRIPT_DIR" in
    */ports_scripts)
      CANDIDATES+=(
        "$SCRIPT_DIR/../ports/sims3"
        "$SCRIPT_DIR/../../ports/sims3"
      )
      ;;
  esac

  for candidate in "\${CANDIDATES[@]}"; do
    if [ -f "$candidate/run.sh" ]; then
      GAMEDIR="$(cd -P -- "$candidate" 2>/dev/null && pwd -P)" || GAMEDIR="$candidate"
      break
    fi
  done
fi

echo "[launcher] gamedir=\${GAMEDIR:-<não encontrado>}"

if [ -z "$GAMEDIR" ]; then
  echo "[ERROR] pasta/runtime do port não encontrado."
  echo "[ERROR] procurei relativo ao launcher e nos roots: /mnt/sdcard, /mnt/mmc, /roms, /roms2, /storage/roms, /userdata."
  echo "[ERROR] verifique o log: $LOG"
  exit 1
fi

RUN="$GAMEDIR/run.sh"
if [ ! -f "$RUN" ]; then
  echo "[ERROR] run.sh não encontrado: $RUN"
  exit 1
fi

cd "$GAMEDIR" || {
  echo "[ERROR] não foi possível entrar em $GAMEDIR"
  exit 1
}

chmod +x "$RUN" 2>/dev/null || true
export SIMS3_GAME_DIR="$GAMEDIR"

echo "[launcher] runtime=$RUN"
echo "[launcher] iniciando runtime em foreground"

"$RUN" "$@"
status=$?

echo "[launcher] runtime exit=$status"
exit "$status"
