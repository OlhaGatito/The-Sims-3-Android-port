#!/usr/bin/env bash
# Rebuild the port's Linux stubs in libs/ from the provenance source
# sims3-stubs.c (see the header of that file for the origin and the fix).
#
# Policy:
#   - builds with the documented NextOS SDK (same toolchain as the loader),
#     falling back to a host arm-linux-gnueabihf-gcc when Docker is absent;
#   - backs up the previous libs/ files ONCE into build/libs-backup/
#     (later runs never clobber that backup);
#   - validates the result structurally (ABI, exports, dependencies)
#     instead of comparing hashes, because the SDK and host toolchains
#     may legitimately produce different byte sequences;
#   - never touches the tracked loader binaries.
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
SDK_IMAGE="${NEXTOS_SDK_IMAGE:-nextos-public-sdk:1}"
SRC="$ROOT/sims3-stubs.c"
OUT_DIR="$ROOT/libs"
WORK_DIR="$ROOT/build/libs-build"
BACKUP_DIR="$ROOT/build/libs-backup"

# Flags recovered from build-stubs.sh (commit a63c177). They reproduce the
# previously shipped artifact byte-for-byte with arm-linux-gnueabihf-gcc 13.3.
ARCH_FLAGS=(-march=armv7-a -mfpu=vfp -mfloat-abi=hard)
MODULES=(libs3eAndroidJNI.so libs3eVFS.so)

die() { printf '[NEXTOS-LIBS][ERRO] %s\n' "$*" >&2; exit 1; }

[[ -f "$SRC" ]] || die "fonte ausente: sims3-stubs.c"
[[ -d "$OUT_DIR" ]] || die "diretório ausente: libs/"

# --- 1. backup one-shot da versão anterior -------------------------------
mkdir -p "$BACKUP_DIR"
for m in "${MODULES[@]}"; do
  if [[ -f "$OUT_DIR/$m" && ! -f "$BACKUP_DIR/$m" ]]; then
    cp -p "$OUT_DIR/$m" "$BACKUP_DIR/$m"
    printf '[NEXTOS-LIBS] backup da versão anterior: build/libs-backup/%s\n' "$m"
  fi
done

# --- 2. build ------------------------------------------------------------
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

if command -v docker >/dev/null 2>&1; then
  printf '[NEXTOS-LIBS] Compilando stubs ARMv7 hard-float com %s\n' "$SDK_IMAGE"
  bash "$ROOT/scripts/nextos-bootstrap.sh"
  docker run --rm -v "$ROOT:/repo" -w /repo "$SDK_IMAGE" bash -lc "
set -Eeuo pipefail
test -s /repo/sims3-stubs.c
arm-linux-gnueabihf-gcc -fPIC -shared -O2 -Wall -Wextra \
  -march=armv7-a -mfpu=vfp -mfloat-abi=hard -D__ARM__ \
  -o /repo/build/libs-build/libs3eAndroidJNI.so \
  /repo/sims3-stubs.c
test -s /repo/build/libs-build/libs3eAndroidJNI.so
cp /repo/build/libs-build/libs3eAndroidJNI.so \
   /repo/build/libs-build/libs3eVFS.so
"
elif command -v arm-linux-gnueabihf-gcc >/dev/null 2>&1; then
  printf '[NEXTOS-LIBS][AVISO] Docker ausente; usando arm-linux-gnueabihf-gcc do host.\n'
  (
    cd "$WORK_DIR"
    arm-linux-gnueabihf-gcc -fPIC -shared -O2 -Wall -Wextra \
      "${ARCH_FLAGS[@]}" -D__ARM__ \
      -o libs3eAndroidJNI.so "$SRC"
    # O original de proveniência era uma cópia byte-a-byte do primeiro.
    cp libs3eAndroidJNI.so libs3eVFS.so
  )
else
  die "nem Docker/SDK nem arm-linux-gnueabihf-gcc disponíveis"
fi

# --- 3. validação estrutural --------------------------------------------
NM="${ARM_NM:-arm-linux-gnueabihf-nm}"
command -v "$NM" >/dev/null 2>&1 || NM=nm

fail=0
for m in "${MODULES[@]}"; do
  lib="$WORK_DIR/$m"
  printf '\n[NEXTOS-LIBS] validando %s\n' "$m"
  [[ -s "$lib" ]] || { echo "  FALHOU: arquivo vazio/ausente"; fail=1; continue; }

  hdr="$(readelf -h "$lib" 2>/dev/null)"
  echo "$hdr" | grep -q 'Class:.*ELF32' \
    && echo "  OK   classe ELF32" || { echo "  FALHOU classe"; fail=1; }
  echo "$hdr" | grep -q 'Machine:.*ARM' \
    && echo "  OK   máquina ARM" || { echo "  FALHOU máquina"; fail=1; }
  echo "$hdr" | grep -Eq 'Flags:.*Version5 EABI, hard-float ABI' \
    && echo "  OK   EABI5 hard-float" || { echo "  FALHOU EABI5 hard-float"; fail=1; }

  attrs="$(readelf -A "$lib" 2>/dev/null)"
  echo "$attrs" | grep -q 'Tag_ABI_VFP_args: VFP registers' \
    && echo "  OK   args FP em registradores VFP" || { echo "  FALHOU FP ABI"; fail=1; }
  echo "$attrs" | grep -q 'Tag_CPU_arch: v7' \
    && echo "  OK   CPU arch v7" || { echo "  FALHOU CPU arch"; fail=1; }

  exports="$("$NM" -D --defined-only "$lib" 2>/dev/null | awk '{print $NF}' | sort)"
  echo "$exports" | sed 's/^/       exporta: /'

  for want in s3eAndroidJNIInitialize s3eAndroidJNITerminate s3eVFSInit \
              s3eVFSTerm s3eGetSystemProperty s3eDeviceYield; do
    echo "$exports" | grep -qx "$want" \
      && echo "  OK   exporta $want" || { echo "  FALHOU falta exportar $want"; fail=1; }
  done

  for bad in dlopen dlsym dlclose; do
    if echo "$exports" | grep -qx "$bad"; then
      echo "  FALHOU interpoem $bad (quebra o loader)"; fail=1
    else
      echo "  OK   nao interpoem $bad"
    fi
  done

  needed="$(readelf -d "$lib" 2>/dev/null | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p' | tr '\n' ' ')"
  echo "       NEEDED: ${needed:-<nenhum>}"
  [ "$needed" = "libc.so.6 " ] || [ "$needed" = "libc.so.6" ] \
    && echo "  OK   depende apenas de libc.so.6" || { echo "  FALHOU NEEDED: $needed"; fail=1; }

  echo "       sha256: $(sha256sum "$lib" | cut -d' ' -f1)"
  echo "       bytes : $(wc -c <"$lib" | tr -d ' ')"
done

[ "$fail" -eq 0 ] || die "validação reprovada; libs/ não foi alterado"

# --- 4. instalação -------------------------------------------------------
for m in "${MODULES[@]}"; do
  cp "$WORK_DIR/$m" "$OUT_DIR/$m"
done
printf '\n[NEXTOS-LIBS] instalado em libs/:\n'
for m in "${MODULES[@]}"; do
  printf '  %-22s %6s bytes  sha256=%s\n' \
    "$m" "$(wc -c <"$OUT_DIR/$m" | tr -d ' ')" \
    "$(sha256sum "$OUT_DIR/$m" | cut -c1-16)"
done
printf '[NEXTOS-LIBS] versão anterior preservada em build/libs-backup/ e no histórico do git.\n'
