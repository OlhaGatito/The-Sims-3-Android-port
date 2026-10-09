#!/usr/bin/env bash
# Canonical local build: use the NextOS SDK documented in the project guide.
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
SDK_IMAGE="${NEXTOS_SDK_IMAGE:-nextos-public-sdk:1}"
OUT_REL="build/nextos/sims3_s3e_loader"
OUT="$ROOT/$OUT_REL"

bash "$ROOT/scripts/nextos-bootstrap.sh"
mkdir -p "$(dirname "$OUT")"

printf '[NEXTOS-BUILD] Compilando loader ARMv7 hard-float com %s\n' "$SDK_IMAGE"
docker run --rm -v "$ROOT:/repo" -w /repo "$SDK_IMAGE" bash -lc '
set -Eeuo pipefail
make -C loader \
  TARGET=/repo/build/nextos/sims3_s3e_loader \
  CC=arm-linux-gnueabihf-gcc \
  STRIP=arm-linux-gnueabihf-strip
test -s /repo/build/nextos/sims3_s3e_loader
'
test -s "$OUT" || { echo "[NEXTOS-BUILD][ERRO] O build não gerou $OUT_REL" >&2; exit 1; }
printf '[NEXTOS-BUILD] Artefato gerado: %s\n' "$OUT_REL"
printf '[NEXTOS-BUILD] O loader versionado em /loader e o loader da raiz não foram substituídos.\n'
