# 🎮 The Sims 3 — Android → Linux ARM (Multi-CFW)

> Port de **The Sims 3 (Android)** para handhelds Linux ARM com suporte **multi-firmware**.  
> Compatível com: **muOS**, **ArkOS**, **ROCKNIX**, **NextOS** e outros CFWs com PortMaster.

---

## ⚡ Estado atual

| Status | Detalhes |
|--------|----------|
| ✅ **Loader** | Compilado para ARMv7 hard-float (NEON, VFPv4) |
| ✅ **Extração** | Gatito Extractor com UI gráfica |
| ✅ **Compatibilidade** | Detecção automática de CFW e bibliotecas |
| 🔄 **Testes** | Validado em muOS; teste em andamento em ArkOS, ROCKNIX, NextOS |
| 📅 | Última atualização: **2026-10-06** |

---

## 🚀 Início Rápido

> ⚠️ **Você precisa:** APK + OBB legais do jogo (seu próprio backup)

### 1. Copie seus arquivos

```bash
# Coloque na raiz da pasta do port:
cp ~/Downloads/the-sims-3-*.apk .
cp ~/Downloads/main.*.obb .
```

### 2. Execute

```bash
./run.sh
```

**Primeira execução:** Gatito Extractor preparará os dados automaticamente (pode levar 5-10 min).  
**Próximas execuções:** O jogo inicia direto.

### 3. Se houver problema

```bash
# Fallback com backends padrão:
./run-fallback.sh

# Diagnóstico completo:
./detect_system.sh
```

---

## 📁 Estrutura

```
.
├── run.sh                    # Launcher principal (recomendado)
├── run-fallback.sh           # Fallback com backends conservadores
├── detect_system.sh          # Diagnóstico: CFW, arquitetura, libs
├── The Sims 3.sh             # Wrapper PortMaster
├── port_compat.sh            # Detecção automática de áudio/vídeo
├── sims3_s3e_loader          # Runtime Marmalade/S3E (ARMv7 hard-float)
├── loader/                   # Código-fonte do loader (C)
├── gatito-extract/           # Motor de extração com UI gráfica
│   ├── run.sh               # Launcher da UI
│   ├── gatito-extract-v3.py # Engine de extração (Python)
│   └── BUILD.ui/            # Interface gráfica
├── hooks/                    # Scripts de processamento (unpack, validação)
├── extractor.json            # Receita de extração
├── COMPATIBILIDADE.md        # Matriz de suporte por CFW
├── INSTAL.md                 # Instruções de instalação
└── docs/                     # Documentação técnica
```

---

## 🔧 Configuração (Opcional)

### Áudio
```bash
export SIMS3_AUDIO_DRIVER=alsa      # ou pulseaudio, pipewire
./run.sh
```

### Vídeo
```bash
export SDL_VIDEODRIVER=kmsdrm       # ou fbcon, x11, wayland
export SIMS3_W=640
export SIMS3_H=480
./run.sh
```

### Bypass da UI (extração silenciosa)
```bash
export SIMS3_SKIP_UI=1
./run.sh
```

---

## 🧪 Suporte por CFW

| CFW | Status | Notas |
|-----|--------|-------|
| **muOS** | ✅ Testado | Funcionando |
| **ArkOS** | 🔄 Em teste | Validar ARM32 + libs gráficas |
| **ROCKNIX** | 🔄 Em teste | Validar suporte ARM32 |
| **NextOS** | 🔄 Em teste | Confirmar ABI e backends |
| **Genérico** | ⚠️ Parcial | Requer GLIBC 2.22+, libSDL2, libEGL, libGLES |

**Como testar em seu CFW:**

```bash
# 1. Faça diagnóstico:
./detect_system.sh

# 2. Se tudo OK, tente:
./run.sh

# 3. Se falhar, tente fallback:
./run-fallback.sh

# 4. Reporte com:
cat logs/debug.log
```

---

## 🐛 Troubleshooting

### Erro: "loader is not executable"
```bash
chmod +x sims3_s3e_loader
./run.sh
```

### Erro: "SDL2 not found"
**Seu CFW não tem bibliotecas 32-bit.** Instale:
```bash
# muOS: geralmente pré-instalado
# ArkOS/ROCKNIX: `apt install libsdl2-dev:armhf` (ou equivalente)
```

### Tela preta / sem som
```bash
./run-fallback.sh          # Tenta ALSA + framebuffer
./detect_system.sh         # Confirma drivers disponíveis
```

### Extrator travou
```bash
# Limpe e recomece:
rm -rf game/
./run.sh
```

---

## 🔍 Diagnóstico

```bash
# Sistema:
./detect_system.sh

# Loader:
file sims3_s3e_loader
readelf -h sims3_s3e_loader

# Dados extraídos:
ls -la game/
file game/game.s3e.unpacked       # Deve ser "XE3U"

# Logs:
tail -50 logs/debug.log
tail -50 logs/fallback.log
```

---

## 📚 Documentação Técnica

- **`COMPATIBILIDADE.md`** — Matriz detalhada por CFW e requisitos
- **`INSTAL.md`** — Instalação passo-a-passo
- **`loader/Makefile`** — Como recompilar o loader
- **`extractor.json`** — Receita de extração (validações, hooks)

---

## 🤝 Como Contribuir

1. Teste em seu CFW
2. Execute `./detect_system.sh` e anote o resultado
3. Execute `./run.sh` e reporte sucesso/erro
4. Abra issue com: CFW, modelo, logs, resultado

---

## ⚖️ Licença

- **Código-fonte:** GPL-2.0-or-later
- **Assets do jogo:** Não inclusos (você fornece legalmente)

Veja `LICENSE`.

---

**Questions?** Abra uma [issue](https://github.com/OlhaGatito/The-Sims-3-Android-port/issues).
