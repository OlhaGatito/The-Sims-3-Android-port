```sh
#!/bin/sh
# The Sims 3 — muOS / Anbernic RG40XX H
#
# O loader recebe a RAIZ dos dados (onde vive assets/) e a imagem .s3e ja
# descomprimida. `exec` preserva o PID que o launcher supervisiona.
#
# TELA CHEIA: o RG40XX H possui resolução nativa de 640x480.
# Se SIMS3_W/SIMS3_H não forem definidos pelo ambiente, tentamos obter
# a resolução ativa pelo framebuffer. O fallback é 640x480.

GAMEDIR="$(cd "$(dirname "$0")" && pwd)"
cd "$GAMEDIR" || exit 1

if [ -z "$SIMS3_W" ] || [ -z "$SIMS3_H" ]; then
  fb_w=""
  fb_h=""

  # Tenta obter o modo ativo do framebuffer.
  if [ -r /sys/class/graphics/fb0/modes ]; then
    mode=$(head -n1 /sys/class/graphics/fb0/modes 2>/dev/null)
    fb_w=$(echo "$mode" | sed -n 's/.*[:_]\([0-9]\{3,4\}\)x\([0-9]\{3,4\}\).*/\1/p')
    fb_h=$(echo "$mode" | sed -n 's/.*[:_]\([0-9]\{3,4\}\)x\([0-9]\{3,4\}\).*/\2/p')
  fi

  # RG40XX H: 640x480
  case "$fb_w" in ''|*[!0-9]*) fb_w=640 ;; esac
  case "$fb_h" in ''|*[!0-9]*) fb_h=480 ;; esac

  [ "$fb_w" -ge 320 ] 2>/dev/null && [ "$fb_w" -le 4096 ] 2>/dev/null || fb_w=640
  [ "$fb_h" -ge 240 ] 2>/dev/null && [ "$fb_h" -le 4096 ] 2>/dev/null || fb_h=480

  SIMS3_W=$fb_w
  SIMS3_H=$fb_h
fi

export SIMS3_W SIMS3_H

echo "sims3: surface nativa ${SIMS3_W}x${SIMS3_H} (tela cheia)"

exec "$GAMEDIR/sims3_s3e_loader" \
  --run \
  --root "$GAMEDIR/game" \
  "$GAMEDIR/game/game.s3e.unpacked"
```

