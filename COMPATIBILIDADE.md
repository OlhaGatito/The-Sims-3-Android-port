# Compatibilidade — The Sims 3 Android port

## Resumo

O port roda em **qualquer handheld ARM Linux com:**
- ✅ Processador ARM (ARMv7 ou AArch64 com suporte ARM32)
- ✅ GLIBC 2.22 ou posterior
- ✅ Bibliotecas 32-bit: **SDL2**, **libEGL**, **libGLESv2**
- ✅ Backend de áudio: ALSA, PulseAudio, ou PipeWire
- ✅ PortMaster (recomendado, mas não obrigatório)

---

## Loader (Binário)

| Aspecto | Detalhes |
|--------|----------|
| **Arquitetura** | ARM 32-bit (ARMv7-A) |
| **FPU** | NEON + VFPv4 (hard-float) |
| **ABI** | Linux GNUEABIHF |
| **Compilação** | `arm-linux-gnueabihf-gcc -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard` |
| **Runtime** | Marmalade/S3E |

**Compatibilidade de CPU:**
- ✅ Snapdragon 410 (muOS, RG351M) — **validado**
- ✅ Snapdragon 665/678 (ArkOS R36S) — **em teste**
- ✅ MediaTek MT8163 (ROCKNIX RG503) — **em teste**
- ✅ Apple A15 com suporte ARM32 (NextOS) — **em teste**
- ❌ Hosts x86/x86_64 — não suportado

---

## CFW Compatível

### muOS ✅ (Validado)

| Item | Status | Notas |
|------|--------|-------|
| Loader | ✅ Roda | ARMv7 32-bit, hard-float |
| Extração | ✅ OK | Gatito Extractor com UI |
| Áudio | ✅ ALSA | Funcional |
| Vídeo | ✅ KMS/DRM | Funcional |
| Controles | ✅ OK | Via PortMaster |
| Gameplay | ✅ Testado | Menu + gameplay confirmado |

**Como usar:**
```bash
# Via PortMaster:
cp The\ Sims\ 3.sh /mnt/mmc/MUOS/PortMaster/ports/
cp -r * /mnt/mmc/MUOS/PortMaster/ports/sims3/

# Ou direto:
./run.sh
```

---

### ArkOS / R36S 🔄 (Em Teste)

| Item | Status | Notas |
|------|--------|-------|
| Loader | ⚠️ Provável | ARM32 libs precisam validação |
| Extração | 🔄 Teste | Usar Gatito ou fallback |
| Áudio | 🔄 Teste | ALSA ou PulseAudio |
| Vídeo | 🔄 Teste | KMS/DRM ou fbcon |
| Gameplay | 🔄 Teste | Não validado ainda |

**Diagnóstico:**
```bash
./detect_system.sh
# Procure por: "ARM32 libs: OK" e "SDL2: found"

./run.sh
# Se falhar: ./run-fallback.sh
```

**Requisitos (instale se faltar):**
```bash
ssh root@arkos
apt update
apt install libsdl2-2.0-0:armhf libegl1-mesa:armhf libgles2-mesa:armhf
```

---

### ROCKNIX / RG503 🔄 (Em Teste)

| Item | Status | Notas |
|------|--------|-------|
| Loader | ⚠️ Provável | Validar suporte ARM32 |
| Extração | 🔄 Teste | Python 3 + Gatito |
| Áudio | 🔄 Teste | Verificar backend padrão |
| Vídeo | 🔄 Teste | Framebuffer ou DRM |
| Gameplay | 🔄 Teste | Não validado |

**Como testar:**
```bash
ssh root@rocknix
./detect_system.sh
# Verifique: Architecture, SDL2 path, EGL/GLES paths

./run.sh
```

---

### NextOS 🔄 (Em Teste)

| Item | Status | Notas |
|------|--------|-------|
| Loader | ⚠️ Provável | Tipo AArch64 — ARM32 compatibilidade TBD |
| Extração | 🔄 Teste | Python 3 obrigatório |
| Áudio | 🔄 Teste | Qual driver padrão? |
| Vídeo | 🔄 Teste | Qual backend padrão? |
| Gameplay | 🔄 Teste | Não validado |

**Aviso:** Se o host é AArch64 puro, o loader ARMv7 pode não funcionar sem camada ARM32 compatibilidade.

---

### Genérico (PortMaster) ⚠️ (Suporte Parcial)

Funciona em qualquer lugar que PortMaster esteja disponível, **desde que:**

1. **CPU:** ARM 32-bit com ARMv7-A, NEON, VFPv4
2. **Glibc:** 2.22 ou posterior
3. **Bibliotecas 32-bit:**
   ```bash
   dpkg -l | grep "libsdl2\|libegl\|libgles"
   # Todas devem estar presentes em versão `:armhf` ou genérica
   ```
4. **Python 3:** Para Gatito Extractor
5. **xz-utils:** Para descompactação (fallback)

**Teste:**
```bash
./detect_system.sh
# Saída deve mostrar:
#   Architecture: armv7 (OU aarch64 com ARM32 support)
#   SDL2: found at /lib/arm-linux-gnueabihf
#   EGL/GLES: found at /lib/arm-linux-gnueabihf
```

Se tudo OK:
```bash
./run.sh
```

---

## Backends Suportados

### Áudio

| Backend | Status | Detectado por |
|---------|--------|---------------|
| ALSA | ✅ | `/dev/snd` |
| PulseAudio | ✅ | `$PULSE_SERVER` ou `pactl` |
| PipeWire | ✅ | `$PIPEWIRE_REMOTE` ou `pw-cli` |
| OSS | ⚠️ | `/dev/dsp` (raro) |

**Seleção automática:** `port_compat.sh` detecta e configura `SDL_AUDIODRIVER`.

### Vídeo

| Backend | Status | Detectado por |
|---------|--------|---------------|
| KMS/DRM | ✅ | `/dev/dri/card0` |
| Framebuffer | ✅ | `/dev/fb0` |
| X11 | ⚠️ | `$DISPLAY` |
| Wayland | ⚠️ | `$WAYLAND_DISPLAY` |

**Seleção automática:** `port_compat.sh` tenta KMS → Framebuffer → X11.

---

## Como Validar

### 1. Sistema

```bash
./detect_system.sh
```

Procure por:
- ✅ `Architecture: armv7` ou `aarch64`
- ✅ `SDL2: found at ...`
- ✅ `EGL/GLES: found at ...`
- ✅ `audio=alsa|pulseaudio|pipewire`
- ✅ `video=kmsdrm|fbcon|x11`

Se alguma falhar → seu CFW não tem dependências instaladas.

### 2. Loader

```bash
file sims3_s3e_loader
# Deve mostrar: ELF 32-bit LSB executable, ARM, EABI5 hard-float
readelf -h sims3_s3e_loader
# Procure: Machine: ARM
```

### 3. Extração

```bash
./run.sh
# Primeira vez: Gatito Extractor abre
# Aguarde a preparação dos dados
# Se OK: menu do jogo aparece
```

Se falhar:
```bash
./run-fallback.sh
tail logs/fallback.log
```

---

## Requisitos por Ambiente

### Mínimo (em qualquer CFW)

```
✓ arm-linux-gnueabihf libc
✓ ARMv7-A CPU com NEON + VFPv4
✓ ~2 GB de espaço livre (jogo extraído)
✓ Python 3 (para Gatito)
```

### Recomendado (PortMaster)

```
✓ PortMaster instalado (/PortMaster ou /opt/tools/PortMaster)
✓ Acesso a /proc (para diagnóstico)
✓ Permissões de leitura/escrita em /tmp
```

### Opcionais

```
○ xz-utils (para fallback de descompactação)
○ file (para diagnóstico)
○ readelf (para diagnóstico)
```

---

## Troubleshooting

| Problema | Causa | Solução |
|----------|-------|---------|
| "loader is not executable" | Permissões | `chmod +x sims3_s3e_loader` |
| "SDL2 not found" | Libs 32-bit ausentes | Instale `libsdl2:armhf` |
| "EGL/GLES not found" | Drivers gráficos ausentes | Instale `libegl1-mesa:armhf libgles2-mesa:armhf` |
| Tela preta | Backend vídeo errado | `./run-fallback.sh` |
| Sem som | Backend áudio errado | `export SDL_AUDIODRIVER=alsa && ./run.sh` |
| Extrator travou | Falha na ZIP | Limpe: `rm -rf game/` e recomece |
| Python 3 not found | Python ausente | `apt install python3` |

---

## Status de Validação

| CFW | Loader | Extração | Gameplay | Data Teste | Tester |
|-----|--------|----------|----------|-----------|--------|
| muOS | ✅ | ✅ | ✅ | 2026-10-06 | OlhaGatito |
| ArkOS | 🔄 | 🔄 | 🔄 | Pendente | Você? |
| ROCKNIX | 🔄 | 🔄 | 🔄 | Pendente | Você? |
| NextOS | 🔄 | 🔄 | 🔄 | Pendente | Você? |

**Ajude:** Teste em seu CFW e reporte `./detect_system.sh` + log!

---

## Contato

Issues: https://github.com/OlhaGatito/The-Sims-3-Android-port/issues
