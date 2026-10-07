#!/bin/bash
# NxExtract Runtime Environment - Sims 3
# Sets up environment for NextOS-style execution

# Export NextOS-style environment variables
export NXEXTRACT_VERSION="2.0.0"
export NXEXTRACT_ID="sims3-s3e"
export NXEXTRACT_GAME_DIR="$SIMS3_GAME_DIR"
export NXEXTRACT_RECIPE="$SIMS3_RECIPE"
export NXEXTRACT_LOG="${SIMS3_LOG_DIR:-$SIMS3_GAME_DIR/logs}/nxextract.log"

# Set SIMS3-specific environment
export SIMS3_NXEXTRACT_MODE=1
export SIMS3_NXEXTRACT_RUNTIME="$0"
export SIMS3_NXEXTRACT_UID="${UID:-$(id -u)}"
export SIMS3_NXEXTRACT_PID="$$"

# NextOS environment detection
NXEXTRACT_CFW="unknown"
NXEXTRACT_DEVICE="unknown"
NXEXTRACT_ARCH="$(uname -m)"
NXEXTRACT_KERNEL="$(uname -r)"

# CFW detection (NextOS pattern)
detect_cfw() {
    local cfw="unknown"
    [ -f /opt/muos/bin/muos-version ] && cfw="muos"
    { [ -d /opt/system/bin ] && [ -f "/opt/system/Advanced/Firmware Version.txt" ]; } && cfw="arkos"
    [ -f /opt/bin/emulationstation ] && cfw="rocknix"
    grep -qi "nextos" /etc/os-release 2>/dev/null && cfw="nextos"
    [ "$cfw" = "unknown" ] && grep -qi "rocknix" /etc/os-release 2>/dev/null && cfw="rocknix"
    [ "$cfw" = "unknown" ] && grep -qi "aurknix" /etc/os-release 2>/dev/null && cfw="rocknix"
    [ "$cfw" = "unknown" ] && [ -d /storage/roms ] && [ -f /opt/bin/emulationstation ] && cfw="rocknix"
    echo "$cfw"
}

NXEXTRACT_CFW=$(detect_cfw)
export NXEXTRACT_CFW NXEXTRACT_DEVICE NXEXTRACT_ARCH NXEXTRACT_KERNEL

# Set up standard paths
NXEXTRACT_STAGING_DIR="${SIMS3_GAME_DIR}/.nxextract"
mkdir -p "$NXEXTRACT_STAGING_DIR" "${SIMS3_LOG_DIR:-$SIMS3_GAME_DIR}/logs"

# Export key variables
export NXEXTRACT_STAGING_DIR
export SIMS3_NXEXTRACT_START_TIME="$(date +%s)"

# Set up logging
log() {
    local level="$1"
    shift
    local message="$*"
    local timestamp=$(date '+%H:%M:%S')
    
    case "$level" in
        ERROR) echo "[$timestamp] ERROR: $message" | tee -a "$NXEXTRACT_LOG" ;;
        WARN)  echo "[$timestamp] WARN: $message" | tee -a "$NXEXTRACT_LOG" ;;
        INFO)  echo "[$timestamp] INFO: $message" | tee -a "$NXEXTRACT_LOG" ;;
        OK)    echo "[$timestamp] OK: $message" | tee -a "$NXEXTRACT_LOG" ;;
    esac
}

# Log environment setup
log "INFO" "NxExtract Runtime Environment"
log "INFO" "CFW: $NXEXTRACT_CFW"
log "INFO" "Device: $NXEXTRACT_DEVICE ($NXEXTRACT_ARCH)"
log "INFO" "Kernel: $NXEXTRACT_KERNEL"
log "INFO" "GameDir: $NXEXTRACT_GAME_DIR"
log "INFO" "Recipe: $NXEXTRACT_RECIPE"
log "INFO" "Staging: $NXEXTRACT_STAGING_DIR"

# Set up hooks environment if available
[ -d "$SIMS3_GAME_DIR/hooks" ] && {
    log "INFO" "Hooks directory: $SIMS3_GAME_DIR/hooks"
    export SIMS3_HOOKS_DIR="$SIMS3_GAME_DIR/hooks"
}

# Set up library paths for S3E stubs
NXEXTRACT_LIB_DIR="$SIMS3_GAME_DIR/libs.armhf"
[ "$NXEXTRACT_ARCH" = "aarch64" ] && [ -d "$SIMS3_GAME_DIR/libs.aarch64" ] && {
    NXEXTRACT_LIB_DIR="$SIMS3_GAME_DIR/libs.aarch64"
}

export NXEXTRACT_LIB_DIR
export LD_PRELOAD="$NXEXTRACT_LIB_DIR/libs3eAndroidJNI.so:$NXEXTRACT_LIB_DIR/libs3eVFS.so${LD_PRELOAD:+:$LD_PRELOAD}"

log "INFO" "LibDir: $NXEXTRACT_LIB_DIR"
[ -n "$LD_PRELOAD" ] && log "INFO" "LD_PRELOAD: $LD_PRELOAD"

# Set up trap for cleanup
trap 'log "INFO" "Cleaning up..."; rm -rf "$NXEXTRACT_STAGING_DIR"' EXIT