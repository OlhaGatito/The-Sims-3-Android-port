#!/usr/bin/env bash
# Build, verify, syntax-check, then stage a BYO-data PortMaster package.
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
STAGE="$ROOT/build/nextos/package/sims3"
OUT_REL="build/nextos/sims3.zip"
OUT="$ROOT/$OUT_REL"

bash "$ROOT/scripts/build-loader-nextos.sh"
bash "$ROOT/scripts/verify-loader-nextos.sh" "build/nextos/sims3_s3e_loader"
bash "$ROOT/scripts/test-shell-scripts.sh"

rm -rf "$STAGE"
mkdir -p "$STAGE"
required=(
  "The Sims 3.sh"
  "port.json"
  "extractor.json"
  "README.md"
  "INSTAL.md"
  "COMPATIBILIDADE.md"
)
for rel in "${required[@]}"; do
  [[ -f "$ROOT/$rel" ]] || { printf '[NEXTOS-PACKAGE][ERRO] Arquivo necessário ausente: %s\n' "$rel" >&2; exit 1; }
  cp -a "$ROOT/$rel" "$STAGE/"
done
for dir in nxextract libs; do
  [[ -d "$ROOT/$dir" ]] || { printf '[NEXTOS-PACKAGE][ERRO] Diretório necessário ausente: %s\n' "$dir" >&2; exit 1; }
  cp -a "$ROOT/$dir" "$STAGE/"
done
cp -a "$ROOT/build/nextos/sims3_s3e_loader" "$STAGE/sims3_s3e_loader"
chmod +x "$STAGE/sims3_s3e_loader" "$STAGE/The Sims 3.sh"

# Resíduos de execução local (caches Python recriados a cada uso do NxExtract,
# arquivos de editor/backup) não podem ir para o ZIP. Remove SOMENTE do stage,
# nunca da árvore build/ (logs, extrações e binários compilados são preservados).
purge_stage_residues() {
  find "$STAGE" -type d -name '__pycache__' -prune -exec rm -rf {} + 2>/dev/null || true
  find "$STAGE" -type f \( \
      -name '*.pyc' -o -name '*.pyo' -o -name '*.orig' -o -name '*.rej' \
      -o -name '*~' -o -name '*.swp' -o -name '.DS_Store' -o -name 'Thumbs.db' \
    \) -delete 2>/dev/null || true
}
purge_stage_residues

# Guarda: falha se algo equivalente sobrar no stage.
stage_residues="$(find "$STAGE" \( -type d -name '__pycache__' \
  -o -type f \( -name '*.pyc' -o -name '*.pyo' \) \) -print 2>/dev/null || true)"
if [ -n "$stage_residues" ]; then
  printf '[NEXTOS-PACKAGE][ERRO] Resíduos permanecem no stage:\n%s\n' "$stage_residues" >&2
  exit 1
fi
printf '[NEXTOS-PACKAGE] Resíduos de execução removidos do stage.\n'

SDK_IMAGE="${NEXTOS_SDK_IMAGE:-nextos-public-sdk:1}"
docker run --rm -v "$ROOT:/repo" -w /repo "$SDK_IMAGE" bash -lc '
set -Eeuo pipefail
python3 - "build/nextos/package/sims3" "build/nextos/sims3.zip" <<'"'"'PY'"'"'
import pathlib, sys, zipfile
stage = pathlib.Path(sys.argv[1])
output = pathlib.Path(sys.argv[2])
output.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for path in sorted(stage.rglob("*")):
        if path.is_file():
            archive.write(path, pathlib.Path("sims3") / path.relative_to(stage))
count = sum(1 for p in stage.rglob("*") if p.is_file())
print("PACKAGE=%s FILES=%d" % (output, count))
PY
'
test -s "$OUT" || { echo "[NEXTOS-PACKAGE][ERRO] ZIP não foi criado." >&2; exit 1; }
printf '[NEXTOS-PACKAGE] Pacote BYO-data criado: %s\n' "$OUT_REL"
printf '[NEXTOS-PACKAGE] Dados/APK/OBB do jogo não foram adicionados.\n'
