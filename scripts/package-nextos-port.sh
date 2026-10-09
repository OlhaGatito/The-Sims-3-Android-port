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
  "run.sh"
  "run-fallback.sh"
  "port_compat.sh"
  "sims3-port-bootstrap.sh"
  "detect_system.sh"
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
for dir in hooks nxextract libs.armhf; do
  [[ -d "$ROOT/$dir" ]] || { printf '[NEXTOS-PACKAGE][ERRO] Diretório necessário ausente: %s\n' "$dir" >&2; exit 1; }
  cp -a "$ROOT/$dir" "$STAGE/"
done
cp -a "$ROOT/build/nextos/sims3_s3e_loader" "$STAGE/sims3_s3e_loader"
chmod +x "$STAGE/sims3_s3e_loader" "$STAGE/The Sims 3.sh" "$STAGE/run.sh" "$STAGE/run-fallback.sh" "$STAGE/port_compat.sh" "$STAGE/sims3-port-bootstrap.sh"

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
print(f"PACKAGE={output} FILES={sum(1 for p in stage.rglob('*') if p.is_file())}")
PY
'
test -s "$OUT" || { echo "[NEXTOS-PACKAGE][ERRO] ZIP não foi criado." >&2; exit 1; }
printf '[NEXTOS-PACKAGE] Pacote BYO-data criado: %s\n' "$OUT_REL"
printf '[NEXTOS-PACKAGE] Dados/APK/OBB do jogo não foram adicionados.\n'
