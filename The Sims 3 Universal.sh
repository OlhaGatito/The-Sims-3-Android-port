#!/usr/bin/env bash
# The Sims 3 — launcher independente de localização.
# Pode ficar em qualquer diretório.
# Procura sims3 ao lado do launcher e nos layouts comuns do PortMaster.
# NXExtract é usado somente através do runner oficial do port.

set -u
SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" 2>/dev/null && pwd -P)" || exit 1
LOG_FALLBACK="$SCRIPT_DIR/The Sims 3.log"

find_game_dir() {
  if [ -n "$SIMS3_GAME_DIR" ] 2>/dev/null && [ -d "$SIMS3_GAME_DIR" ]; then
    cd "$SIMS3_GAME_DIR" && pwd -P
    return 0
  fi

  for candidate in     "$SCRIPT_DIR/sims3"     "$SCRIPT_DIR/The Sims 3"     "/mnt/mmc/roms/ports/sims3"     "/mnt/mmc/roms/ports/The Sims 3"     "/mnt/sdcard/Roms/ports/sims3"     "/mnt/sdcard/Roms/ports/The Sims 3"     "/roms/ports/sims3"     "/roms/ports/The Sims 3"     "/storage/roms/ports/sims3"     "/storage/roms/ports/The Sims 3"
  do
    if [ -d "$candidate" ]; then
      cd "$candidate" 2>/dev/null && pwd -P
      return 0
    fi
  done

  for base in /mnt/mmc/roms/ports /mnt/sdcard/Roms/ports /roms/ports /storage/roms/ports; do
    [ -d "$base" ] || continue
    candidate="$(find "$base" -maxdepth 2 -type f -name 'sims3_s3e_loader' -print -quit 2>/dev/null)"
    if [ -n "$candidate" ]; then
      dirname "$candidate"
      return 0
    fi
  done
  return 1
}

GAMEDIR="$(find_game_dir)" || {
  echo "[ERROR] pasta do The Sims 3 não encontrada." >"$LOG_FALLBACK"
  echo "[INFO] use SIMS3_GAME_DIR=/caminho/para/sims3." >>"$LOG_FALLBACK"
  exit 1
}

mkdir -p "$GAMEDIR" 2>/dev/null || true
LOG="$GAMEDIR/run.log"
exec >"$LOG" 2>&1
echo "=== The Sims 3 / launcher independente ==="
echo "[launcher] script=$SCRIPT_DIR"
echo "[launcher] game=$GAMEDIR"
cd "$GAMEDIR" || exit 1

for f in run.sh run-extractor.sh nxextract-runtime-env.sh nxextract-ui nxextract.py sims3_s3e_loader; do
  [ -f "$f" ] && chmod +x "$f" 2>/dev/null || true
done

if [ ! -s "$GAMEDIR/game/game.s3e.unpacked" ]; then
  echo "[launcher] payload ausente; verificando NXExtract."
  if [ -f "$GAMEDIR/run-extractor.sh" ] && [ -r "$GAMEDIR/extractor.json" ]; then
    bash "$GAMEDIR/run-extractor.sh"
    status=$?
    [ "$status" -eq 0 ] || { echo "[ERROR] NXExtract status=$status"; exit "$status"; }
  else
    echo "[ERROR] cadeia NXExtract incompleta."
    echo "[ERROR] esperado: extractor.json + run-extractor.sh + nxextract-runtime-env.sh + nxextract.py + nxextract-ui"
    echo "[INFO] não será inventada uma chamada direta ao nxextract-ui."
    exit 72
  fi
fi

[ -s "$GAMEDIR/game/game.s3e.unpacked" ] || { echo "[ERROR] payload não produzido."; exit 73; }
chmod +x "$GAMEDIR/sims3_s3e_loader" 2>/dev/null || true
[ -x "$GAMEDIR/sims3_s3e_loader" ] || { echo "[ERROR] loader não executável."; exit 74; }

echo "[launcher] iniciando The Sims 3"
exec "$GAMEDIR/sims3_s3e_loader" --run --root "$GAMEDIR/game" "$GAMEDIR/game/game.s3e.unpacked"
