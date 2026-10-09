#!/usr/bin/env bash
# Verify the loader with readelf from the same NextOS SDK used for compilation.
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
SDK_IMAGE="${NEXTOS_SDK_IMAGE:-nextos-public-sdk:1}"
BIN="${1:-build/nextos/sims3_s3e_loader}"
MAX_GLIBC="${MAX_GLIBC:-2.22}"

[[ "$BIN" = /* ]] || BIN="$ROOT/$BIN"
[[ -s "$BIN" ]] || { printf '[NEXTOS-VERIFY][ERRO] Loader ausente/vazio: %s\n' "$BIN" >&2; exit 1; }
BIN_REL="${BIN#"$ROOT"/}"
[[ "$BIN_REL" != "$BIN" ]] || { echo "[NEXTOS-VERIFY][ERRO] Passe um loader localizado dentro do repositório." >&2; exit 1; }

bash "$ROOT/scripts/nextos-bootstrap.sh"

docker run --rm -e BIN_REL="$BIN_REL" -e MAX_GLIBC="$MAX_GLIBC" -v "$ROOT:/repo" -w /repo "$SDK_IMAGE" bash -lc '
set -Eeuo pipefail
bin="/repo/$BIN_REL"
max_glibc="$MAX_GLIBC"

if command -v file >/dev/null 2>&1; then
  echo "=== file ==="
  file "$bin"
else
  echo "[WARN] ferramenta file ausente; validação ELF continuará com readelf."
fi
echo "=== ELF header ==="
readelf -h "$bin"
echo "=== ARM attributes ==="
readelf -A "$bin"
echo "=== program interpreter ==="
readelf -l "$bin"
echo "=== dynamic dependencies ==="
readelf -d "$bin"
echo "=== required GLIBC versions ==="
readelf --version-info "$bin" | grep -oE "GLIBC_[0-9]+\.[0-9]+" | sort -Vu || true

readelf -h "$bin" | grep "Class:.*ELF32" >/dev/null || { echo "ERRO: não é ELF32" >&2; exit 1; }
readelf -h "$bin" | grep "Machine:.*ARM" >/dev/null || { echo "ERRO: arquitetura não é ARM32" >&2; exit 1; }
readelf -l "$bin" | grep "/lib/ld-linux-armhf.so.3" >/dev/null || { echo "ERRO: interpretador ARM hard-float esperado não encontrado" >&2; exit 1; }
readelf -A "$bin" | grep "Tag_ABI_VFP_args: VFP registers" >/dev/null || { echo "ERRO: ABI hard-float não confirmada pelos atributos ELF" >&2; exit 1; }
readelf -d "$bin" | grep "(NEEDED)" >/dev/null || { echo "ERRO: loader não parece dinamicamente ligado" >&2; exit 1; }

max_required="$(readelf --version-info "$bin" | grep -oE "GLIBC_[0-9]+\.[0-9]+" | sed "s/GLIBC_//" | sort -Vu | tail -n 1 || true)"
if [[ -n "$max_required" ]]; then
  highest="$(printf "%s\n%s\n" "$max_glibc" "$max_required" | sort -V | tail -n 1)"
  if [[ "$highest" != "$max_glibc" ]]; then
    echo "ERRO: o loader requer GLIBC_$max_required, acima do limite GLIBC_$max_glibc" >&2
    exit 1
  fi
  echo "GLIBC_MAX=$max_required (limite=$max_glibc)"
else
  echo "GLIBC_MAX=nenhuma versão GLIBC explícita encontrada"
fi

echo "NEXTOS_VERIFY=PASS"
'
