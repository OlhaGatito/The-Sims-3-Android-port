#!/bin/sh
# The Sims 3 — NXExtract hook
# Converte o S3E LZMA extraído para o payload XE3U usado pelo loader.
# O hook deve operar SOMENTE no stage fornecido pelo NXExtract.

set -eu

STAGE="${NXEXTRACT_STAGE:?NXEXTRACT_STAGE não definido}"
GAME_DIR="${NXEXTRACT_GAME_DIR:-$(cd "$SCRIPT_DIR/.." 2>/dev/null && pwd -P)}"
LOGDIR="${SIMS3_LOG_DIR:-$GAME_DIR/logs}"
mkdir -p "$LOGDIR" || exit 1
LOG="${SIMS3_HOOK_LOG:-$LOGDIR/unpack-s3e.log}"
exec >>"$LOG" 2>&1
echo "=== The Sims 3 / S3E unpack hook ==="
echo "[hook] game_dir=$GAME_DIR"
echo "[hook] stage=$STAGE"
echo "[hook] log=$LOG"
SRC="$STAGE/game/The Sims 3.s3e"
DST="$STAGE/game/game.s3e.unpacked"
echo "[hook] source=$SRC"
echo "[hook] destination=$DST"

log_source_diagnostics() {
    REASON="$1"
    XZ_EXIT="$2"
    echo "[diagnostics] failure=$REASON xz_exit=$XZ_EXIT"
    echo "[diagnostics] cfw=${CFW_NAME:-unknown} device=${DEVICE_NAME:-unknown} arch=${DEVICE_ARCH:-unknown}"
    SYSTEM_INFO="$(uname -srm 2>/dev/null || true)"
    echo "[diagnostics] system=${SYSTEM_INFO:-unknown}"
    XZ_PATH="$(command -v xz 2>/dev/null || true)"
    echo "[diagnostics] xz_path=${XZ_PATH:-unavailable}"
    XZ_VERSION="$(xz --version 2>&1 | sed -n '1p' || true)"
    echo "[diagnostics] xz_version=${XZ_VERSION:-unavailable}"
    echo "[diagnostics] xz_command=xz --single-stream --format=lzma -d -c <source>"
    if [ -f "$SRC" ]; then
        SOURCE_SIZE="$(wc -c < "$SRC" 2>/dev/null | tr -d '[:space:]' || true)"
        SOURCE_HEADER="$(od -An -tx1 -N13 "$SRC" 2>/dev/null | tr -d ' \n' || true)"
        echo "[diagnostics] source_size_bytes=${SOURCE_SIZE:-unknown}"
        echo "[diagnostics] source_first_13_bytes_hex=${SOURCE_HEADER:-unavailable}"
        if command -v sha256sum >/dev/null 2>&1; then
            sha256sum "$SRC" 2>&1 || echo "[diagnostics] sha256sum failed"
        elif command -v sha256 >/dev/null 2>&1; then
            sha256 "$SRC" 2>&1 || echo "[diagnostics] sha256 failed"
        else
            echo "[diagnostics] sha256 tool unavailable"
        fi
    else
        echo "[diagnostics] source_file_missing=1"
    fi
    if [ -f "$DST" ]; then
        OUTPUT_SIZE="$(wc -c < "$DST" 2>/dev/null | tr -d '[:space:]' || true)"
        echo "[diagnostics] partial_output_size_bytes=${OUTPUT_SIZE:-unknown}"
    else
        echo "[diagnostics] partial_output_exists=0"
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

if xz --single-stream --format=lzma -d -c "$SRC" > "$DST"; then
    :
else
    XZ_EXIT=$?
    echo "[Sims3 hook] ERRO: xz não conseguiu descompactar o S3E" >&2
    log_source_diagnostics "lzma_decode_failed" "$XZ_EXIT"
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
    58453355)
        ;;
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
echo "[Sims3 hook] LZMA concluído"