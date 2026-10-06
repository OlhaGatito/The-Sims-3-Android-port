# Instalação — The Sims 3 Android Port

## Pré-requisitos

- **Handheld com PortMaster** (muOS, ArkOS, ROCKNIX, NextOS ou similar)
- **APK + OBB** do The Sims 3 (seu próprio backup)
- **~2 GB de espaço livre** no cartão (jogo extraído)
- **Bibliotecas 32-bit:** SDL2, libEGL, libGLES (geralmente pré-instaladas)

---

## Passo 1: Preparar pasta do port

```bash
# SSH no handheld
ssh root@handheld

# Acesse a pasta PortMaster
cd /mnt/mmc/MUOS/PortMaster/ports/  # muOS
# ou
cd /opt/roms/ports/                 # ArkOS
# ou
cd /roms/ports/                     # ROCKNIX / NextOS genérico
```

---

## Passo 2: Copiar arquivos do port

```bash
# Clone ou copie para sims3/
git clone https://github.com/OlhaGatito/The-Sims-3-Android-port.git sims3
# ou
cp -r ~/The-Sims-3-Android-port sims3
cd sims3
```

Estrutura final:

```
sims3/
├── sims3_s3e_loader          # Executável (ARMv7)
├── run.sh                    # Launcher principal
├── run-fallback.sh           # Fallback (ALSA + fbcon)
├── The Sims 3.sh             # Wrapper PortMaster
├── detect_system.sh          # Diagnóstico
├── port_compat.sh            # Detecção automática de backends
├── extractor.json            # Receita de extração
├── gatito-extract/           # Motor de extração + UI
│   ├── run.sh
│   ├── gatito-extract-v3.py
│   └── BUILD.ui/
├── hooks/                    # Scripts de processamento
├── loader/                   # Código-fonte (opcional)
└── game/                     # Será criado (dados extraídos)
```

---

## Passo 3: Copiar APK + OBB

```bash
# Coloque seus arquivos na raiz da pasta sims3:
cp ~/The\ Sims\ 3*.apk sims3/
cp ~/main.*.obb sims3/
# ou crie pasta gamedata:
mkdir -p sims3/gamedata
cp ~/The\ Sims\ 3*.apk sims3/gamedata/
cp ~/main.*.obb sims3/gamedata/
```

---

## Passo 4: Primeira execução

### Via PortMaster (recomendado)

```bash
# No menu do PortMaster, procure por "The Sims 3"
# Clique para iniciar

# Na primeira vez: Gatito Extractor abrirá (UI gráfica)
# Aguarde até aparecer "Extração concluída" (pode levar 5-10 min)
# O jogo iniciará automaticamente
```

### Via terminal (se não estiver no PortMaster)

```bash
cd sims3
./run.sh

# Primeira vez: Gatito Extractor
# Próximas vezes: Jogo direto
```

---

## Passo 5: Se houver problema

### Gatito travou / não avança

```bash
# 1. Verifique o sistema:
./detect_system.sh

# 2. Tente fallback (backends conservadores):
./run-fallback.sh

# 3. Verifique logs:
tail logs/debug.log
tail logs/gatito-extract-ui.log
```

### Tela preta / sem som

```bash
# Tente com backend específico:
export SDL_VIDEODRIVER=fbcon
export SDL_AUDIODRIVER=alsa
./run.sh

# Ou use fallback:
./run-fallback.sh
```

### Extrator travou no meio

```bash
# Limpe e recomece:
rm -rf game/
./run.sh
```

---

## Estrutura de diretórios após extração

```
sims3/
├── game/
│   ├── game.s3e.unpacked/        # Dados do jogo descompactados
│   ├── assets/                   # Assets (texturas, modelos, etc.)
│   └── ...
├── logs/
│   ├── debug.log
│   ├── gatito-extract-ui.log
│   └── fallback.log
└── ...
```

---

## Limpeza (se precisar reinstalar)

```bash
# Remova apenas os dados extraídos:
rm -rf sims3/game/

# Ou limpe tudo (mantém APK/OBB):
cd sims3
./run.sh  # Reextrairá automaticamente
```

---

## Variáveis de ambiente (avançado)

```bash
# Forçar backend de áudio
export SDL_AUDIODRIVER=alsa       # alsa, pulseaudio, pipewire, dsp

# Forçar backend de vídeo
export SDL_VIDEODRIVER=fbcon      # fbcon, kmsdrm, x11, wayland

# Resolução customizada
export SIMS3_W=1280
export SIMS3_H=720

# Python customizado (se instalado em lugar não padrão)
export NXEXTRACT_PYTHON=/usr/bin/python3.10

# Log customizado
export SIMS3_LOG_DIR=/tmp/sims3-logs
export SIMS3_EXTRACTOR_LOG=/tmp/sims3-extractor.log

# Executar:
./run.sh
```

---

## Troubleshooting

| Erro | Solução |
|------|---------|
| `loader is not executable` | `chmod +x sims3_s3e_loader` |
| `SDL2 not found` | Instale `libsdl2:armhf` (depends of CFW) |
| `Python 3 not found` | Instale Python 3 no seu CFW |
| Gatito não abre | `./detect_system.sh` — confirme se SDL2 está presente |
| Tela preta | `./run-fallback.sh` ou mude `SDL_VIDEODRIVER` |
| Sem som | Mude `SDL_AUDIODRIVER` ou use fallback |
| Jogo trava ao carregar | Verifique logs em `logs/debug.log` |

---

## Validação pós-instalação

```bash
# Confirme que a extração terminou:
ls -la sims3/game/
# Deve conter: game.s3e.unpacked/, assets/, etc.

# Confirme que o loader funciona:
file sims3/sims3_s3e_loader
# Deve mostrar: ELF 32-bit LSB executable, ARM

# Diagnóstico completo:
./detect_system.sh
# Procure por: Architecture, SDL2, EGL/GLES (tudo deve estar "found")
```

---

## Suporte

Se tiver problemas:
1. Execute `./detect_system.sh` e anote a saída
2. Procure em `logs/` pelos arquivos de log
3. Abra uma [issue](https://github.com/OlhaGatito/The-Sims-3-Android-port/issues) com CFW, modelo, e logs

---

**Bom jogo!** 🎮
