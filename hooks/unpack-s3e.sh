#!/bin/bash
# The Sims 3 — NXExtract hook
# Converte o S3E LZMA extraído para o payload XE3U usado pelo loader.
# O hook opera SOMENTE no stage fornecido pelo NXExtract.

set -eu

STAGE="${NXEXTRACT_STAGE:?NXEXTRACT_STAGE não definido}"
GAME_DIR="${NXEXTRACT_GAME_DIR:-$(cd "$(dirname "$0")/.." 2>/dev/null && pwd -P)}"
LOGDIR="${SIMS3_LOG_DIR:-$GAME_DIR/logs}"
mkdir -p "$LOGDIR" || exit 1
LOG="${SIMS3_HOOK_LOG:-$LOGDIR/unpack-s3e.log}"

# Keep hook diagnostics visible to Gatito/PortMaster while also preserving them
# in the persistent hook log.
if command -v tee >/dev/null 2>&1; then
    exec > >(tee -a "$LOG") 2>&1
else
    exec >>"$LOG" 2>&1
fi

echo "=== The Sims 3 / S3E unpack hook ==="
echo "[hook] game_dir=$GAME_DIR"
echo "[hook] stage=$STAGE"
echo "[hook] log=$LOG"

SRC="$STAGE/game/The Sims 3.s3e"
DST="$STAGE/game/game.s3e.unpacked"
LOADER="$GAME_DIR/sims3_s3e_loader"

echo "[hook] source=$SRC"
echo "[hook] destination=$DST"
echo "[hook] loader=$LOADER"

log_source_diagnostics() {
    REASON="$1"
    LOADER_EXIT="$2"
    echo "[diagnostics] failure=$REASON loader_exit=$LOADER_EXIT"
    echo "[diagnostics] cfw=${CFW_NAME:-unknown} device=${DEVICE_NAME:-unknown} arch=${DEVICE_ARCH:-unknown}"
    echo "[diagnostics] system=$(uname -srm 2>/dev/null || true)"
    echo "[diagnostics] decoder=loader-bundled-lzma-sdk"
    echo "[diagnostics] loader_path=$LOADER"
    echo "[diagnostics] loader_command=sims3_s3e_loader --unpack-s3e <source> <destination>"
    if [ -f "$SRC" ]; then
        echo "[diagnostics] source_size_bytes=$(wc -c < "$SRC" 2>/dev/null | tr -d '[:space:]' || true)"
        echo "[diagnostics] source_first_13_bytes_hex=$(od -An -tx1 -N13 "$SRC" 2>/dev/null | tr -d ' \n' || true)"
        if command -v sha256sum >/dev/null 2>&1; then sha256sum "$SRC" 2>&1 || true; fi
    else
        echo "[diagnostics] source_file_missing=1"
    fi
    if [ -f "$DST" ]; then
        echo "[diagnostics] output_size_bytes=$(wc -c < "$DST" 2>/dev/null | tr -d '[:space:]' || true)"
        echo "[diagnostics] output_first_4_bytes_hex=$(od -An -tx1 -N4 "$DST" 2>/dev/null | tr -d ' \n' || true)"
    else
        echo "[diagnostics] output_file_missing=1"
    fi
}

echo "[Sims3 hook] preparando S3E"

if [ ! -f "$SRC" ]; then
    echo "[Sims3 hook] ERRO: entrada S3E não encontrada no stage" >&2
    echo "[diagnostics] expected_source=$SRC"
    echo "[diagnostics] stage=$STAGE"
    exit 10
fi

mkdir -p "$STAGE/game"
rm -f "$DST"

if [ -f "$LOADER" ] && [ ! -x "$LOADER" ]; then chmod +x "$LOADER" 2>/dev/null || true; fi
if [ ! -x "$LOADER" ]; then
    echo "[Sims3 hook] ERRO: loader com decodificador LZMA não encontrado/executável" >&2
    log_source_diagnostics "loader_unavailable" 127
    exit 69
fi

echo "[Sims3 hook] destino confirmado: $DST"
echo "[Sims3 hook] iniciando LZMA..."
if "$LOADER" --unpack-s3e "$SRC" "$DST"; then
    :
else
    LOADER_EXIT=$?
    echo "[Sims3 hook] ERRO: decodificador LZMA do loader não conseguiu preparar o S3E" >&2
    log_source_diagnostics "lzma_sdk_decode_failed" "$LOADER_EXIT"
    rm -f "$DST"
    exit 13
fi

if [ ! -s "$DST" ]; then
    echo "[Sims3 hook] ERRO: payload descompactado vazio" >&2
    log_source_diagnostics "empty_output" 0
    exit 11
fi

MAGIC="$(od -An -tx1 -N4 "$DST" | tr -d ' \n')"
case "$MAGIC" in
    58453355) ;;
    *)
        echo "[Sims3 hook] ERRO: header XE3U não encontrado" >&2
        echo "[diagnostics] output_header_hex=${MAGIC:-unavailable}"
        log_source_diagnostics "invalid_xe3u_header" 0
        exit 12
        ;;
esac

printf '[Sims3 hook] payload: '
wc -c < "$DST"
printf '[Sims3 hook] header: %s\n' "$MAGIC"
echo "[Sims3 hook] LZMA concluído: $DST"