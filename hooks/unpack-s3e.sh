#!/bin/sh
# The Sims 3 — NXExtract hook
# Converte o S3E LZMA extraído para o payload XE3U usado pelo loader.
# O hook deve operar SOMENTE no stage fornecido pelo NXExtract.

set -eu

STAGE="${NXEXTRACT_STAGE:?NXEXTRACT_STAGE não definido}"
SRC="$STAGE/game/The Sims 3.s3e"
DST="$STAGE/game/game.s3e.unpacked"

echo "[Sims3 hook] preparando S3E"

if [ ! -f "$SRC" ]; then
    echo "[Sims3 hook] ERRO: entrada S3E não encontrada no stage" >&2
    exit 10
fi

mkdir -p "$STAGE/game"
rm -f "$DST"

xz --format=lzma -d -c "$SRC" > "$DST"

if [ ! -s "$DST" ]; then
    echo "[Sims3 hook] ERRO: payload descompactado vazio" >&2
    exit 11
fi

MAGIC="$(od -An -tx1 -N4 "$DST" | tr -d ' \n')"
case "$MAGIC" in
    58453355)
        ;;
    *)
        echo "[Sims3 hook] ERRO: header XE3U não encontrado" >&2
        exit 12
        ;;
esac

printf '[Sims3 hook] payload: '
wc -c < "$DST"
printf '[Sims3 hook] header: %s\n' "$MAGIC"
echo "[Sims3 hook] LZMA concluído"
