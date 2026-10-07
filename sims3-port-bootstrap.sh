#!/bin/bash
# Sims 3 PortMaster Bootstrap - NextOS-style compatibility
# Supports: muOS, ArkOS, ROCKNIX, NextOS, PortMaster

set +u

# PortMaster control detection (NextOS pattern)
NXBOOTSTRAP_PM_ROOTS=(
  "/opt/system/Tools/PortMaster" "/opt/tools/PortMaster"
  "$XDG_DATA_HOME/PortMaster" "/storage/.config/PortMaster"
  "/var/data/PortMaster" "/PortMaster"
  "/roms/tools/PortMaster" "/roms2/tools/PortMaster"
  "/storage/roms/ports/PortMaster" "/storage/roms/Ports/PortMaster"
  "/roms/ports/PortMaster" "/roms/Ports/PortMaster"
  "/roms2/ports/PortMaster" "/roms2/Ports/PortMaster"
  "/userdata/roms/ports/PortMaster"
  "/userdata/system/.local/share/PortMaster"
  "/mnt/ports/PortMaster" "/mnt/Ports/PortMaster"
  "/mnt/mmc/MUOS/PortMaster" "/mnt/mmc/ports/PortMaster"
  "/mnt/SDCARD/Apps/PortMaster/PortMaster"
  "/mnt/sdcard/Roms/.portmaster/PortMaster"
)

controlfolder=""
for NXBOOTSTRAP_PM_CANDIDATE in "${NXBOOTSTRAP_PM_ROOTS[@]}"; do
  if [ -f "$NXBOOTSTRAP_PM_CANDIDATE/control.txt" ] && [ ! -L "$NXBOOTSTRAP_PM_CANDIDATE/control.txt" ]; then
    NXBOOTSTRAP_PM_REAL=$(readlink -f "$NXBOOTSTRAP_PM_CANDIDATE" 2>/dev/null) || continue
    if [ -d "$NXBOOTSTRAP_PM_REAL" ] && [ -f "$NXBOOTSTRAP_PM_REAL/control.txt" ] && [ ! -L "$NXBOOTSTRAP_PM_REAL/control.txt" ]; then
      controlfolder=$NXBOOTSTRAP_PM_REAL
      break
    fi
  fi
done

# Load PortMaster configuration
NXCOMPAT_PORTMASTER_DIR=""
CFW_NAME=""
if [ -f "$controlfolder/control.txt" ] && [ ! -L "$controlfolder/control.txt" ]; then
  source "$controlfolder/control.txt"
  NXCOMPAT_PORTMASTER_DIR="$controlfolder"
fi

detect_cfw() {
  local cfw=unknown
  [ -f /opt/muos/bin/muos-version ] && cfw=muos
  { [ -d /opt/system/bin ] && [ -f "/opt/system/Advanced/Firmware Version.txt" ]; } && cfw=arkos
  [ -f /opt/bin/emulationstation ] && cfw=rocknix
  grep -qi "nextos" /etc/os-release 2>/dev/null && cfw=nextos
  [ "$cfw" = "unknown" ] && grep -qi "rocknix" /etc/os-release 2>/dev/null && cfw=rocknix
  [ "$cfw" = "unknown" ] && grep -qi "aurknix" /etc/os-release 2>/dev/null && cfw=rocknix
  [ "$cfw" = "unknown" ] && [ -d /storage/roms ] && [ -f /opt/bin/emulationstation ] && cfw=rocknix
  echo "$cfw"
}
CFW_NAME=$(detect_cfw)
export CFW_NAME

# Game directory setup
GAMEDIR="/$directory/ports/sims3"
[ -d "$GAMEDIR" ] || GAMEDIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)/sims3"
cd "$GAMEDIR" || exit 1
GAMEDIR=$(pwd -P)

# PortMaster compatibility - Audio/Input setup
export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-auto}
[ -n "$sdl_controllerconfig" ] && export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

# NXExtract integration (BYO-data pattern)
NXEXTRACT_REQUESTED=1
if [ "$NXEXTRACT_REQUESTED" = 1 ]; then
  if [ -f "$GAMEDIR/nxextract/run-extractor.sh" ]; then
    $ESUDO chmod +x "$GAMEDIR/nxextract/run-extractor.sh" 2>/dev/null || true
    NXEXTRACT_GAME_DIR="$GAMEDIR" bash "$GAMEDIR/nxextract/run-extractor.sh" || {
      echo "ERROR: game data installation did not complete"
      exit 1
    }
  fi
fi

# Library paths (NextOS pattern)
LIBS=""
for d in "$controlfolder/libs" "$controlfolder/libs.armhf" "$controlfolder/libs.aarch64" \
         /usr/local/lib/aarch64-linux-gnu /usr/local/lib64 /usr/local/lib \
         /usr/lib/aarch64-linux-gnu /lib/aarch64-linux-gnu \
         /usr/lib64 /lib64 /usr/lib /lib; do
  [ -n "$d" ] && [ -d "$d" ] && LIBS="${LIBS:+$LIBS:}$d"
done
export LD_LIBRARY_PATH="${LIBS}:$GAMEDIR:${LD_LIBRARY_PATH:-}"

# Capabilities declaration
export NXCOMPAT_PORT_ID=sims3
export NXCOMPAT_GAME_DIR="$GAMEDIR"
export NXCOMPAT_REQUIRED_CAPABILITIES='
host.aarch64-libs
graphics.window
graphics.gles2
graphics.egl
graphics.egl-config
graphics.drawable
audio.output-open
input.controller-mapping'
export NXCOMPAT_ENABLED_QUIRKS=''
export NXCOMPAT_RUNTIME_REPORT=log-and-logo

# Cleanup traps
nxbootstrap_finish() {
  if command -v pm_finish >/dev/null 2>&1; then pm_finish || true; fi
}
trap nxbootstrap_finish EXIT

# Console setup (NextOS pattern)
[ -n "$CUR_TTY" ] && [ -c "$CUR_TTY" ] && \
  printf '\033[?25l\033[2J\033[H' > "$CUR_TTY" 2>/dev/null || true

# Instance lock (NextOS pattern)
NXBOOTSTRAP_LOCK_ROOT=${XDG_RUNTIME_DIR:-/tmp}
NXBOOTSTRAP_LOCK_DIR="$NXBOOTSTRAP_LOCK_ROOT/.nxbootstrap-${UID:-$(id -u)}"
[ ! -d "$NXBOOTSTRAP_LOCK_DIR" ] && (umask 077; mkdir "$NXBOOTSTRAP_LOCK_DIR") 2>/dev/null || true
chmod 0700 "$NXBOOTSTRAP_LOCK_DIR" 2>/dev/null || true
NXBOOTSTRAP_LOCK_FILE="$NXBOOTSTRAP_LOCK_DIR/nxport-sims3.flock"

# Manifest validation
[ ! -f "$GAMEDIR/sims3_s3e_loader" ] && {
  echo "ERROR: Loader not found: $GAMEDIR/sims3_s3e_loader"
  exit 1
}

# Set up loader environment
export HOME="$GAMEDIR"
export SIMS3_PORTMASTER_COMPAT=1

# Signal handling
nxbootstrap_request_shutdown() {
  NXBOOTSTRAP_SHUTDOWN_REQUESTED=1
}
trap nxbootstrap_request_shutdown INT TERM HUP

echo "=== Sims 3 PortMaster Bootstrap ready ==="
echo "CFW=$CFW_NAME"
echo "GAMEDIR=$GAMEDIR"
echo "NXCOMPAT_PORTMASTER_DIR=$NXCOMPAT_PORTMASTER_DIR"