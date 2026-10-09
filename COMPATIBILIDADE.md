# Compatibilidade — The Sims 3 Android port

## Resumo

O port roda em **qualquer handheld ARM Linux com:**
- ✅ Processador ARM (ARMv7 ou AArch64 com suporte ARM32)
- ✅ GLIBC 2.22 ou posterior
- ✅ Bibliotecas 32-bit: **SDL2**, **libEGL**, **libGLESv2**
- ✅ Backend de áudio: ALSA, PulseAudio, ou PipeWire
- ✅ PortMaster (recomendado, mas não obrigatório)
- ✅ **Python 3 com `lzma`** (para a extração NxExtract)

---

## Loader (Binário)

| Aspecto | Detalhes |
|--------|----------|
| **Arquitetura** | ARM 32-bit (ARMv7-A) |
| **FPU** | NEON + VFPv4 (hard-float) |
| **ABI** | Linux GNUEABIHF |
| **Compilação** | `arm-linux-gnueabihf-gcc -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard` |
| **Runtime** | Marmalade/S3E |
| **GLIBC mínimo** | 2.22 (declarado em `port.json` como `min_glibc`) |

**Compatibilidade de CPU:**
- ✅ Snapdragon 410 (muOS, RG351M) — **validado**
- ✅ Snapdragon 665/678 (ArkOS R36S) — **em teste**
- ✅ MediaTek MT8163 (ROCKNIX RG503) — **em teste**
- ✅ Apple A15 com suporte ARM32 (NextOS) — **em teste**
- ❌ Hosts x86/x86_64 — não suportado

---

## Fluxo real (únicas entradas)

Não existem `run.sh`, `run-fallback.sh`, `detect_system.sh`, `port_compat.sh`
nem `gatito-extract/` — foram removidos em commits antigos. O fluxo atual é:

```
The Sims 3.sh  →  NxExtract (nxextract/run-extractor.sh + extractor.json)
               →  loader (sims3_s3e_loader --run --root game game/game.s3e.unpacked)
```

Toda invocação no terminal usa o launcher:

```bash
bash "The Sims 3.sh"
```

---

## CFW Compatível

### muOS ✅ (Validado)

| Item | Status | Notas |
|------|--------|-------|
| Loader | ✅ Roda | ARMv7 32-bit, hard-float |
| Extração | ✅ OK | NxExtract (recipe + UI) |
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
bash "The Sims 3.sh"
```

---

### ArkOS / R36S 🔄 (Em Teste)

| Item | Status | Notas |
|------|--------|-------|
| Loader | ⚠️ Provável | ARM32 libs precisam validação |
| Extração | 🔄 Teste | NxExtract (Python 3 + lzma) |
| Áudio | 🔄 Teste | ALSA ou PulseAudio |
| Vídeo | 🔄 Teste | KMS/DRM ou fbcon |
| Gameplay | 🔄 Teste | Não validado ainda |

**Diagnóstico:**
```bash
# Confirme as libs 32-bit:
readelf -d sims3_s3e_loader | grep NEEDED
bash "The Sims 3.sh"
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
| Extração | 🔄 Teste | Python 3 + NxExtract |
| Áudio | 🔄 Teste | Verificar backend padrão |
| Vídeo | 🔄 Teste | Framebuffer ou DRM |
| Gameplay | 🔄 Teste | Não validado |

**Como testar:**
```bash
ssh root@rocknix
readelf -h sims3_s3e_loader       # deve mostrar ELF32 / Machine: ARM
bash "The Sims 3.sh"
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

**Aviso:** Se o host é AArch64 puro, o loader ARMv7 pode não funcionar sem
camada de compatibilidade ARM32.

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
4. **Python 3 + lzma:** para a extração NxExtract
5. **xz-utils:** opcional (fallback de descompactação)

**Teste:**
```bash
readelf -h sims3_s3e_loader       # ELF32, Machine: ARM
readelf -A sims3_s3e_loader | grep VFP   # hard-float
python3 -c "import lzma"          # extração
bash "The Sims 3.sh"
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

**Seleção automática:** o launcher negocia via SDL/firmware; `SIMS3_AUDIO_DRIVER`
ou `AUDIO_DRIVER` podem forçar um backend.

### Vídeo

| Backend | Status | Detectado por |
|---------|--------|---------------|
| KMS/DRM | ✅ | `/dev/dri/card0` |
| Framebuffer | ✅ | `/dev/fb0` |
| X11 | ⚠️ | `$DISPLAY` |
| Wayland | ⚠️ | `$WAYLAND_DISPLAY` |

**Seleção automática:** o launcher tenta KMS → Framebuffer → X11 e pode forçar
`SDL_VIDEODRIVER=fbcon` em ambientes sem GPU.

---

## Como Validar

### 1. Loader

```bash
file sims3_s3e_loader
# Deve mostrar: ELF 32-bit LSB executable, ARM, EABI5 hard-float
readelf -h sims3_s3e_loader
# Procure: Machine: ARM
readelf -l sims3_s3e_loader | grep ld-linux-armhf
```

### 2. Extração

```bash
bash "The Sims 3.sh"
# Primeira vez: NxExtract prepara os dados (game/)
# Depois: menu do jogo
```

Confirme o payload XE3U:
```bash
head -c 4 game/game.s3e.unpacked | od -An -tx1   # 58 45 33 55
```

Se falhar:
```bash
tail -n 50 nxextract.log
rm -rf game/ .nxextract-sims3.json && bash "The Sims 3.sh"   # reextrair
```

---

## Requisitos por Ambiente

### Mínimo (em qualquer CFW)

```
✓ arm-linux-gnueabihf libc
✓ ARMv7-A CPU com NEON + VFPv4
✓ ~2 GB de espaço livre (jogo extraído)
✓ Python 3 + lzma (para NxExtract)
```

### Recomendado (PortMaster)

```
✓ PortMaster instalado (/PortMaster ou /opt/tools/PortMaster)
✓ Acesso a /proc (para diagnóstico)
✓ Permissões de leitura/escrita em /tmp
```

### Opcionais

```
○ xz-utils (fallback de descompactação)
○ OBB do jogo (assets res.dz de LowRes/HighRes)
○ file / readelf (diagnóstico)
```

---

## Troubleshooting

| Problema | Causa | Solução |
|----------|-------|---------|
| "loader is not executable" | Permissões | `chmod +x sims3_s3e_loader` |
| "SDL2 not found" | Libs 32-bit ausentes | Instale `libsdl2:armhf` |
| "EGL/GLES not found" | Drivers gráficos ausentes | Instale `libegl1-mesa:armhf libgles2-mesa:armhf` |
| "incomplete NxExtract integration" | Faltam arquivos de `nxextract/` | Copie o port completo |
| Tela preta | Backend vídeo errado | `export SDL_VIDEODRIVER=fbcon` |
| Sem som | Backend áudio errado | `export SDL_AUDIODRIVER=alsa` |
| Extração falha em LZMA | `lzma` ausente | `python3 -c "import lzma"` |
| Falta `res.dz` no jogo | OBB ausente | Forneça o OBB em `gamedata/` |
| Extrator travou | ZIP corrompida | `rm -rf game/ .nxextract-sims3.json` |
| Python 3 not found | Python ausente | `apt install python3` |

---

## Status de Validação

> Estes resultados são os relatados pelo autor do port. Uma rodada independente
> deve reconfirmar no próprio aparelho; build/ELF sozinhos não provam runtime.

| CFW | Loader | Extração | Gameplay | Data Teste | Tester |
|-----|--------|----------|----------|-----------|--------|
| muOS | ✅ | ✅ | ✅ | 2026-10-06 | OlhaGatito |
| ArkOS | 🔄 | 🔄 | 🔄 | Pendente | Você? |
| ROCKNIX | 🔄 | 🔄 | 🔄 | Pendente | Você? |
| NextOS | 🔄 | 🔄 | 🔄 | Pendente | Você? |

**Ajude:** Teste em seu CFW e reporte a saída do launcher + logs!

---

## Contato

Issues: https://github.com/OlhaGatito/The-Sims-3-Android-port/issues
