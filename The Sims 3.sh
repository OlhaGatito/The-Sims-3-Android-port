#!/usr/bin/env bash
# The Sims 3 — PortMaster launcher (layout lado a lado)
# /ports/The Sims 3.sh
# /ports/sims3/
# NXExtract follows the NextOS runner/runtime boundary.

set -u
SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" 2>/dev/null && pwd -P)" || exit 1
GAMEDIR="$SCRIPT_DIR/sims3"
[ -n "$SIMS3_GAME_DIR" ] 2>/dev/null && GAMEDIR="$SIMS3_GAME_DIR"
LOG="$GAMEDIR/run.log"
mkdir -p "$GAMEDIR" 2>/dev/null || true
exec >"$LOG" 2>&1

echo "=== The Sims 3 / PortMaster ==="
echo "[launcher] game=$GAMEDIR"
[ -d "$GAMEDIR" ] || { echo "[ERROR] pasta ausente: $GAMEDIR"; exit 1; }
cd "$GAMEDIR" || exit 1

for f in run.sh run-extractor.sh nxextract-runtime-env.sh nxextract-ui nxextract.py sims3_s3e_loader; do
  [ -f "$f" ] && chmod +x "$f" 2>/dev/null || true
done

# A preparação deve passar pelo runner oficial do NXExtract.
if [ ! -s "$GAMEDIR/game/game.s3e.unpacked" ]; then
  echo "[launcher] payload ainda não preparado; iniciando NXExtract."
  if [ -f "$GAMEDIR/run-extractor.sh" ] && [ -r "$GAMEDIR/extractor.json" ]; then
    bash "$GAMEDIR/run-extractor.sh"
    status=$?
    [ "$status" -eq 0 ] || { echo "[ERROR] NXExtract status=$status"; exit "$status"; }
  else
    echo "[ERROR] cadeia NXExtract incompleta."
    echo "[ERROR] necessários: extractor.json, run-extractor.sh,"
    echo "[ERROR] nxextract-runtime-env.sh, nxextract.py e nxextract-ui."
    echo "[INFO] forneça o APK legítimo do usuário; ele não é publicado pelo port."
    exit 72
  fi
fi

[ -s "$GAMEDIR/game/game.s3e.unpacked" ] || { echo "[ERROR] payload não produzido."; exit 73; }
chmod +x "$GAMEDIR/sims3_s3e_loader" 2>/dev/null || true
[ -x "$GAMEDIR/sims3_s3e_loader" ] || { echo "[ERROR] loader não executável."; exit 74; }

echo "[launcher] iniciando The Sims 3"
exec "$GAMEDIR/sims3_s3e_loader" --run --root "$GAMEDIR/game" "$GAMEDIR/game/game.s3e.unpacked"
