#!/usr/bin/env bash
# Validate the documented NextOS build environment; never silently fall back to host GCC.
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
SDK_IMAGE="${NEXTOS_SDK_IMAGE:-nextos-public-sdk:1}"

die() { printf '[NEXTOS-BOOTSTRAP][ERRO] %s\n' "$*" >&2; exit 1; }
info() { printf '[NEXTOS-BOOTSTRAP] %s\n' "$*"; }

command -v docker >/dev/null 2>&1 || die "Docker não está instalado/disponível. Use o ambiente Docker do NextOS; não será usado GCC nativo como fallback."
docker info >/dev/null 2>&1 || die "O daemon Docker não está acessível. Inicie o Docker antes de continuar."
docker image inspect "$SDK_IMAGE" >/dev/null 2>&1 || die "Imagem SDK '$SDK_IMAGE' não encontrada localmente. Prepare a imagem oficial/documentada do NextOS antes do build; este script não troca para apt/GCC do host."

mkdir -p "$ROOT/build/nextos"

info "Validando ferramentas dentro de $SDK_IMAGE"
docker run --rm -e NEXTOS_SDK_IMAGE="$SDK_IMAGE" -v "$ROOT:/repo" -w /repo "$SDK_IMAGE" bash -lc '
set -Eeuo pipefail
for tool in bash make arm-linux-gnueabihf-gcc arm-linux-gnueabihf-strip readelf file python3; do
  command -v "$tool" >/dev/null 2>&1 || {
    printf "[NEXTOS-BOOTSTRAP][ERRO] Ferramenta ausente no SDK: %s\n" "$tool" >&2
    exit 1
  }
done
printf "SDK_IMAGE=%s\n" "${NEXTOS_SDK_IMAGE:-nextos-public-sdk:1}"
arm-linux-gnueabihf-gcc --version | sed -n "1p"
make --version | sed -n "1p"
'
info "Bootstrap/preflight aprovado. Nenhum arquivo do loader foi alterado."
