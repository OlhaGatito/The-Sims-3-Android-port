#!/bin/bash
# PORTMASTER: sims3-native-arm.zip, The Sims 3.sh
# The Sims 3 — Marmalade S3E loader

# shellcheck disable=SC1090,SC1091,SC2154

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/PortMaster/" ]; then
    controlfolder="/PortMaster"
elif [ -d "/mnt/mmc/MUOS/PortMaster/" ]; then
    controlfolder="/mnt/mmc/MUOS/PortMaster"
elif [ -d "/mnt/sdcard/MUOS/PortMaster/" ]; then
    controlfolder="/mnt/sdcard/MUOS/PortMaster"
elif [ -d "/opt/system/Tools/PortMaster/" ]; then
    controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
    controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
    controlfolder="$XDG_DATA_HOME/PortMaster"
else
    controlfolder="/roms/ports/PortMaster"
fi

if [ ! -f "$controlfolder/control.txt" ]; then
    echo "[The Sims 3] PortMaster control.txt not found: $controlfolder/control.txt" >&2
    exit 1
fi

source "$controlfolder/control.txt" || {
    echo "[The Sims 3] failed to load PortMaster control.txt" >&2
    exit 1
}

export PORT_32BIT="Y"

[ -f "$controlfolder/tasksetter" ] &&
    source "$controlfolder/tasksetter"

[ -f "$controlfolder/device_info.txt" ] &&
    source "$controlfolder/device_info.txt"

[ -f "$controlfolder/mod_${CFW_NAME}.txt" ] &&
    source "$controlfolder/mod_${CFW_NAME}.txt"

get_controls || {
    echo "[The Sims 3] PortMaster get_controls failed" >&2
    exit 1
}

if [ -z "${directory:-}" ]; then
    echo "[The Sims 3] PortMaster did not define directory" >&2
    exit 1
fi

case "$directory" in
    /*) GAMEDIR="${directory%/}/ports/sims3" ;;
    *)  GAMEDIR="/${directory%/}/ports/sims3" ;;
esac

cd "$GAMEDIR" || exit 1

[ -f "$GAMEDIR/run.sh" ] || {
    echo "[The Sims 3] runtime not found: $GAMEDIR/run.sh" >&2
    exit 1
}

export SIMS3_GAME_DIR="$GAMEDIR"

exec bash "$GAMEDIR/run.sh" "$@"
