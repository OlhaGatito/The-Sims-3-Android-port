#!/bin/bash
set -e
GAMEDIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
export GAMEDIR
exec python3 "$GAMEDIR/gatito-extract/BUILD.ui/gatito-ui.py"
