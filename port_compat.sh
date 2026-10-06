#!/bin/sh
# PortMaster compatibility helper: detect audio/video without replacing system libs.

port_log() { printf '%s\n' "[compat] $*"; }

port_detect_audio() {
  PORT_AUDIO_BACKEND=none
  PORT_AUDIO_DEVICE_DETECTED=""
  if [ -d /dev/snd ]; then PORT_AUDIO_BACKEND=alsa; fi
  if [ "$PORT_AUDIO_BACKEND" = none ] && { [ -n "${PULSE_SERVER:-}" ] || [ -n "${PULSE_RUNTIME_PATH:-}" ] || command -v pactl >/dev/null 2>&1 || command -v pulseaudio >/dev/null 2>&1; }; then
    PORT_AUDIO_BACKEND=pulseaudio
  fi
  if [ "$PORT_AUDIO_BACKEND" = none ] && { [ -n "${PIPEWIRE_REMOTE:-}" ] || command -v pw-cli >/dev/null 2>&1; }; then
    PORT_AUDIO_BACKEND=pipewire
  fi
  if [ "$PORT_AUDIO_BACKEND" = none ] && [ -e /dev/dsp ]; then PORT_AUDIO_BACKEND=oss; fi

  case "${PORT_AUDIO_DRIVER:-auto}" in
    auto|"")
      case "$PORT_AUDIO_BACKEND" in
        alsa) export SDL_AUDIODRIVER=alsa ;;
        pulseaudio) export SDL_AUDIODRIVER=pulseaudio ;;
        pipewire) export SDL_AUDIODRIVER=pipewire ;;
        oss) export SDL_AUDIODRIVER=dsp ;;
        none) unset SDL_AUDIODRIVER 2>/dev/null || true ;;
      esac ;;
    alsa) export SDL_AUDIODRIVER=alsa ;;
    pulse|pulseaudio) export SDL_AUDIODRIVER=pulseaudio ;;
    pipewire) export SDL_AUDIODRIVER=pipewire ;;
    oss|dsp) export SDL_AUDIODRIVER=dsp ;;
    none) unset SDL_AUDIODRIVER 2>/dev/null || true ;;
    *) port_log "unknown PORT_AUDIO_DRIVER=$PORT_AUDIO_DRIVER; leaving SDL automatic" ;;
  esac

  # Never force hw:0,0. Let ALSA use its default/plughw negotiation.
  if [ -n "${PORT_AUDIO_DEVICE:-}" ]; then
    export AUDIODEV="$PORT_AUDIO_DEVICE"
    PORT_AUDIO_DEVICE_DETECTED="$PORT_AUDIO_DEVICE"
  elif [ "${SDL_AUDIODRIVER:-}" = alsa ]; then
    unset AUDIODEV 2>/dev/null || true
    PORT_AUDIO_DEVICE_DETECTED=default
  fi
  export PORT_AUDIO_BACKEND PORT_AUDIO_DEVICE_DETECTED
  port_log "audio=$PORT_AUDIO_BACKEND SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-auto} device=${PORT_AUDIO_DEVICE_DETECTED:-auto}"
  [ -r /proc/asound/cards ] && { cat /proc/asound/cards 2>/dev/null; }
  [ -d /dev/snd ] && ls -la /dev/snd 2>/dev/null || true
}

port_detect_video() {
  PORT_VIDEO_BACKEND=none
  PORT_GPU_BACKEND=none
  # Detecta CFW específico (Aurknix/ROCKNIX/ArkOS/muOS) via arquivos/paths conhecidos
  local is_handheld=false
  if [ -f "/opt/bin/emulationstation" ] || [ -d "/opt/roms" ]; then
      is_handheld=true
      PORT_VIDEO_BACKEND=kmsdrm
  elif [ -f "/opt/muos/bin/muos-version" ]; then
      is_handheld=true
      PORT_VIDEO_BACKEND=kmsdrm
  elif [ -f "/opt/system/Advanced/Firmware Version.txt" ] || [ -d "/opt/system/bin" ]; then
      is_handheld=true
      PORT_VIDEO_BACKEND=kmsdrm
  elif grep -qi "nextos" /etc/os-release 2>/dev/null; then
      is_handheld=true
      PORT_VIDEO_BACKEND=kmsdrm
  elif [ -e /dev/dri/card0 ] || [ -e /dev/dri/card1 ]; then
      PORT_VIDEO_BACKEND=kmsdrm
  elif [ -n "${WAYLAND_DISPLAY:-}" ]; then
      PORT_VIDEO_BACKEND=wayland
  elif [ -n "${DISPLAY:-}" ]; then
      PORT_VIDEO_BACKEND=x11
  elif [ -e /dev/fb0 ]; then
      PORT_VIDEO_BACKEND=framebuffer
  fi

  if [ -e /dev/dri/card0 ] || [ -e /dev/dri/renderD128 ] || [ -e /dev/mali0 ]; then
      PORT_GPU_BACKEND=drm-or-gpu
  fi

  # Em handhelds CFW, preferir framebuffer se disponível para evitar pageflip -22
  # a menos que o usuário force outro driver via PORT_VIDEO_DRIVER
  if [ "$is_handheld" = true ] && [ -e /dev/fb0 ] && [ "${PORT_VIDEO_DRIVER:-auto}" = "auto" ]; then
      port_log "Handheld CFW detected with /dev/fb0; preferring fbcon to avoid DRM pageflip issues"
      export SDL_VIDEODRIVER=fbcon
      PORT_VIDEO_BACKEND=framebuffer
  else
      case "${PORT_VIDEO_DRIVER:-auto}" in
        auto|"") : ;;
        x11|wayland|kmsdrm|fbcon|directfb) export SDL_VIDEODRIVER="$PORT_VIDEO_DRIVER" ;;
        none) unset SDL_VIDEODRIVER 2>/dev/null || true ;;
        *) port_log "unknown PORT_VIDEO_DRIVER=$PORT_VIDEO_DRIVER; leaving SDL automatic" ;;
      esac
  fi

  export PORT_VIDEO_BACKEND PORT_GPU_BACKEND
  port_log "video=$PORT_VIDEO_BACKEND SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-auto} gpu=$PORT_GPU_BACKEND"
  [ -d /dev/dri ] && ls -la /dev/dri 2>/dev/null || true
  [ -e /dev/fb0 ] && { port_log "framebuffer=/dev/fb0"; cat /sys/class/graphics/fb0/name 2>/dev/null || true; }
}

port_detect_runtime() {
  port_detect_audio
  port_detect_video
  port_log "display=${DISPLAY:-unset} wayland=${WAYLAND_DISPLAY:-unset} resolution=${DISPLAY_WIDTH:-${SDL_VIDEO_WIDTH:-unknown}}x${DISPLAY_HEIGHT:-${SDL_VIDEO_HEIGHT:-unknown}}"
}
