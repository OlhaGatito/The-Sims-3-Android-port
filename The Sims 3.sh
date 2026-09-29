#!/bin/bash
# PORTMASTER: sims3-native-arm.zip, The Sims 3.sh
# The Sims 3 — Marmalade S3E loader

# shellcheck disable=SC1090,SC1091,SC2154

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
    controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
    controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
    controlfolder="$XDG_DATA_HOME/PortMaster"
else
    controlfolder="/roms/ports/PortMaster"
fi

source "$controlfolder/control.txt"

export PORT_32BIT="Y"

[ -f "$controlfolder/tasksetter" ] &&
    source "$controlfolder/tasksetter"

[ -f "$controlfolder/device_info.txt" ] &&
    source "$controlfolder/device_info.txt"

[ -f "$controlfolder/mod_${CFW_NAME}.txt" ] &&
    source "$controlfolder/mod_${CFW_NAME}.txt"

get_controls

GAMEDIR="/$directory/ports/sims3"
cd "$GAMEDIR" || exit 1

[ -f "$GAMEDIR/run.sh" ] || exit 1
chmod +x "$GAMEDIR/run.sh" 2>/dev/null || true

export SIMS3_GAME_DIR="$GAMEDIR"

exec "$GAMEDIR/run.sh" "$@"
