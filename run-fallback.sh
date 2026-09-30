#!/bin/bash
# The Sims 3 — compatibility retry
# This is only called after the normal runtime returns an error.

set +u

GAMEDIR="$(CDPATH= cd -- "$(dirname -- "$0")" 2>/dev/null && pwd -P)" || exit 1
GAME_DIR="$GAMEDIR/game"
GAME_IMAGE="$GAME_DIR/game.s3e.unpacked"
LOADER="$GAMEDIR/sims3_s3e_loader"

LOGDIR="$GAMEDIR/logs"
mkdir -p "$LOGDIR"
exec >>"$LOGDIR/fallback.log" 2>&1

echo "=== The Sims 3 fallback ==="
echo "Previous runtime failed; retrying with conservative audio/video settings."

[ -f "$LOADER" ] || exit 1
[ -f "$GAME_IMAGE" ] || exit 1

# Do not override a PortMaster-provided backend unless a fallback is needed.
if [ -d /dev/snd ]; then
    export SDL_AUDIODRIVER="${SIMS3_AUDIO_DRIVER:-alsa}"
    echo "Fallback audio: SDL_AUDIODRIVER=$SDL_AUDIODRIVER"
fi

# Only use framebuffer fallback when the system exposes it and no video backend was supplied.
if [ -e /dev/fb0 ] && [ -z "${SDL_VIDEODRIVER:-}" ]; then
    export SDL_VIDEODRIVER=fbcon
    echo "Fallback video: SDL_VIDEODRIVER=$SDL_VIDEODRIVER"
fi

export SIMS3_W="${SIMS3_W:-${DISPLAY_WIDTH:-640}}"
export SIMS3_H="${SIMS3_H:-${DISPLAY_HEIGHT:-480}}"

if [ -d "$GAMEDIR/libs.armhf" ]; then
    export LD_LIBRARY_PATH="$GAMEDIR/libs.armhf${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

command -v pm_platform_helper >/dev/null 2>&1 && pm_platform_helper "$LOADER" || true

"$LOADER" --run --root "$GAME_DIR" "$GAME_IMAGE"
RC=$?

echo "Fallback exit code: $RC"
exit "$RC"
