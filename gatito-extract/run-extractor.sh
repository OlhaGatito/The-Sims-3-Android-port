#!/bin/bash
# Sims 3 NxExtract Runner - NextOS compatible
# Runs the Gatito extractor with NxExtract patterns

set +u

# Script directory
SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
GAME_DIR="${SIMS3_GAME_DIR:-$(cd "$SCRIPT_DIR/.." && pwd)}"
RECIPE_FILE="${GAME_DIR}/extractor.json"

# Export required variables
export SIMS3_GAME_DIR="$GAME_DIR"
export SIMS3_RECIPE="$RECIPE_FILE"

# Source runtime environment
if [ -f "$SCRIPT_DIR/nxextract-runtime-env.sh" ]; then
    source "$SCRIPT_DIR/nxextract-runtime-env.sh"
else
    echo "ERROR: nxextract-runtime-env.sh not found"
    exit 1
fi

# Validate all required files
validate_environment() {
    local missing=0
    
    [ -f "$RECIPE_FILE" ] || { log "ERROR" "Recipe not found: $RECIPE_FILE"; missing=1; }
    [ -f "$SCRIPT_DIR/gatito-extract-v3.py" ] || { log "ERROR" "Engine not found: $SCRIPT_DIR/gatito-extract-v3.py"; missing=1; }
    [ -f "$SCRIPT_DIR/nxextract-ui" ] || { log "ERROR" "UI not found: $SCRIPT_DIR/nxextract-ui"; missing=1; }
    [ -x "$SCRIPT_DIR/nxextract-ui" ] || { log "ERROR" "UI not executable: $SCRIPT_DIR/nxextract-ui"; missing=1; }
    [ -x "$SCRIPT_DIR/gatito-extract-v3.py" ] || { log "ERROR" "Engine not executable: $SCRIPT_DIR/gatito-extract-v3.py"; missing=1; }
    
    # Check hooks
    [ -f "$GAME_DIR/hooks/unpack-s3e.sh" ] || { log "ERROR" "Hook not found: $GAME_DIR/hooks/unpack-s3e.sh"; missing=1; }
    [ -x "$GAME_DIR/hooks/unpack-s3e.sh" ] || { log "ERROR" "Hook not executable: $GAME_DIR/hooks/unpack-s3e.sh"; missing=1; }
    
    # Check loader
    [ -f "$GAME_DIR/sims3_s3e_loader" ] || { log "ERROR" "Loader not found: $GAME_DIR/sims3_s3e_loader"; missing=1; }
    [ -x "$GAME_DIR/sims3_s3e_loader" ] || { log "ERROR" "Loader not executable: $GAME_DIR/sims3_s3e_loader"; missing=1; }
    
    [ "$missing" -eq 0 ] || exit 1
}

log "OK" "Environment validation passed"

# Check for existing extraction (BYO-data fast path)
GAME_IMAGE="$GAME_DIR/game/game.s3e.unpacked"
ASSET_DIR="$GAME_DIR/game/assets"

if [ -f "$GAME_IMAGE" ] && [ -d "$ASSET_DIR" ] && [ -n "$(ls -A "$ASSET_DIR" 2>/dev/null)" ]; then
    # Quick validation
    S3E_HEADER=$(od -An -tx1 -N4 "$GAME_IMAGE" 2>/dev/null | tr -d ' \n')
    if [ "$S3E_HEADER" = "58453355" ]; then
        log "OK" "Valid extraction already exists, skipping"
        log "INFO" "GameImage: $GAME_IMAGE"
        log "INFO" "AssetDir: $ASSET_DIR"
        exit 0
    else
        log "WARN" "Existing extraction invalid, re-extracting"
    fi
fi

# Run extraction with NxExtract UI
log "INFO" "Iniciando Gatito Extractor com interface NxExtract (PT-BR)..."

# Use Portuguese NxExtract UI
"$SCRIPT_DIR/nxextract-ui-ptbr" "$RECIPE_FILE" "$GAME_DIR" "$NXEXTRACT_LOG" "1800"
EXTRACT_RC=$?

if [ "$EXTRACT_RC" -eq 0 ]; then
    log "OK" "Extraction completed successfully"
else
    log "ERROR" "Extraction failed with code $EXTRACT_RC"
    exit $EXTRACT_RC
fi

# Final verification
if [ ! -f "$GAME_IMAGE" ] || [ ! -d "$ASSET_DIR" ]; then
    log "ERROR" "Extraction verification failed - missing files"
    exit 1
fi

# Verify S3E header one final time
S3E_HEADER=$(od -An -tx1 -N4 "$GAME_IMAGE" 2>/dev/null | tr -d ' \n')
if [ "$S3E_HEADER" != "58453355" ]; then
    log "ERROR" "Final S3E validation failed"
    exit 1
fi

log "OK" "Final verification passed"
exit 0