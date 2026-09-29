#!/usr/bin/env bash
# The Sims 3 — projeto específico: entrada do NXExtract
# O motor universal permanece em nxextract/nxextract.py.
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
GAME_DIR="${NXEXTRACT_GAME_DIR:-$SCRIPT_DIR}"
RECIPE="${NXEXTRACT_RECIPE:-$GAME_DIR/extractor.json}"
ENGINE_DIR="$SCRIPT_DIR/nxextract"

[ -d "$ENGINE_DIR" ] || {
  printf '[NXExtract] diretório do motor não encontrado: %s\n' "$ENGINE_DIR" >&2
  exit 72
}
[ -f "$ENGINE_DIR/nxextract.py" ] || {
  printf '[NXExtract] nxextract.py não encontrado: %s\n' "$ENGINE_DIR/nxextract.py" >&2
  exit 72
}
[ -f "$ENGINE_DIR/run-extractor.sh" ] || {
  printf '[NXExtract] run-extractor.sh não encontrado: %s\n' "$ENGINE_DIR/run-extractor.sh" >&2
  exit 72
}
[ -f "$ENGINE_DIR/nxextract-runtime-env.sh" ] || {
  printf '[NXExtract] nxextract-runtime-env.sh não encontrado: %s\n' "$ENGINE_DIR/nxextract-runtime-env.sh" >&2
  exit 72
}
[ -f "$RECIPE" ] || {
  printf '[NXExtract] extractor.json não encontrado: %s\n' "$RECIPE" >&2
  exit 72
}

export NXEXTRACT_GAME_DIR="$GAME_DIR"
export NXEXTRACT_RECIPE="$RECIPE"

exec bash "$ENGINE_DIR/run-extractor.sh" "$@"
