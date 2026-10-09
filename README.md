# The Sims 3 - NextOS Universal Port
**BYO-data** • **NextOS workflow in progress** • **ARMv7 hard-float**

> Universal port for handheld Linux devices (muOS, ArkOS, ROCKNIX, NextOS, PortMaster)

---

## 📦 Package Structure

```
sims3/
├── The Sims 3.sh           # Launcher PortMaster
├── port.json               # PortMaster metadata
├── extractor.json          # NXExtract recipe (ainda precisa de validação real)
├── hooks/                  # Hooks específicos do S3E
├── nxextract/              # Engine, UI, runner e ambiente isolado NXExtract
├── libs.armhf/             # Stubs ARMv7 atualmente versionados
├── run.sh                  # Runtime principal
├── run-fallback.sh         # Fallback de compatibilidade
├── port_compat.sh           # Detecção de áudio/vídeo
├── detect_system.sh         # Diagnóstico do ambiente
├── sims3-port-bootstrap.sh # Bootstrap PortMaster/NextOS
└── sims3_s3e_loader        # Loader ARMv7 hard-float

```

---

## 🎯 Compatibility Matrix

| CFW | Caminho esperado | Status |
|-----|-----------------|--------|
| muOS | Loader ARMv7 hard-float | Requer teste no aparelho |
| ArkOS | Loader ARMv7 hard-float | Requer teste no aparelho |
| ROCKNIX | ARMv7 via compatibilidade de 32 bits | Requer teste no aparelho |
| NextOS | ARMv7 via compatibilidade de 32 bits | Requer teste no aparelho |
| PortMaster | Depende do firmware e do suporte ARM32 | Requer teste no aparelho |

**Importante:** build e inspeção ELF não equivalem a compatibilidade de runtime. Esta tabela não declara gameplay, áudio, vídeo, controles ou saves aprovados.

---

## 🚀 Quick Start

### Installation (BYO-data)

```bash
# 1. Download: game.s3e (The Sims 3 Android APK)
#    From: https://store... (Android Play Store backup)

# 2. Extract APK → get game.s3e
#    Using: 7-Zip, Android Extractor, etc.

# 3. Copy to your device:
#    /storage/roms/ports/sims3/game.s3e

# 4. Run:
#    cd /storage/roms/ports/sims3
#    chmod +x "The Sims 3.sh"
#    ./"The Sims 3.sh"
```

### First Launch

O launcher detecta o firmware, valida os arquivos e chama o runner NXExtract quando a instalação precisa ser feita. **A recipe `extractor.json` ainda precisa de validação contra uma cópia real do APK do Sims 3**; não assuma que a extração está aprovada apenas porque o runner existe.

O payload esperado pelo loader é criado em `game/game.s3e.unpacked`. Mantenha APK/OBB e dados proprietários fora do Git.

---

## 🔧 Components

### 1. **The Sims 3.sh** (Launcher)
- **Role:** PortMaster-compatible wrapper
- **Features:**
  - Instance lock (prevents double-launch)
  - NXExtract owner-data phase (Gatito Extractor)
  - Smart S3E extension stubs (LD_PRELOAD)
  - Video/audio fallback detection
  - Signal handling (graceful shutdown)

### 2. **nxextract/** (Extractor Engine)
- **Engine:** `nxextract/nxextract.py`
- **UI:** `nxextract/nxextract-ui`
- **Runner:** `nxextract/run-extractor.sh`
- **Runtime boundary:** `nxextract/nxextract-runtime-env.sh`
- **Recipe:** `extractor.json` define como localizar e validar os dados do APK; a recipe específica do Sims 3 ainda exige teste real.
- **Hook:** `hooks/unpack-s3e.sh` tenta produzir o payload S3E esperado.

### 3. **libs.armhf/** (Libraries)
- **S3E Stubs:**
  - `libs3eAndroidJNI.so` - JNI interface stub
  - `libs3eVFS.so` - Virtual file system stub
- **Limite:** os dois stubs ARMHF atualmente versionados têm conteúdo idêntico; isso não prova que implementem JNI/VFS nem que evitem crashes. Validar símbolos exportados e comportamento antes de depender deles.

### 4. **run.sh** (Runtime)
- **Role:** Smart launcher with fallback
- **Features:**
  - CFW detection
  - ARMv7 loader selection (AArch64 hosts require ARM32 compatibility)
  - S3E validation
  - Auto fallback (fbcon/kmsdrm/wayland)
  - Dual-mount data reuse

---

## 📊 PortMaster Integration

```bash
# Drop this in PortMaster config:
/opt/system/Tools/PortMaster/040_nextos.source.json

# Then browse and install via PortMaster UI
```

### Control.txt Support

```
CFW_NAME=muOS              # CFW detection
NXINPUT_ANALOG_STICKS_HINT=2  # Analog stick count
sdl_controllerconfig=...  # SDL controller bindings
esudo=sudo              # Sudo command
cur_tty=/dev/tty1        # Console device
```

---

## 🐛 Troubleshooting

### "Executable is missing or unsafe"

```bash
# Ensure loader has correct permissions:
chmod +x sims3_s3e_loader
```

### "S3E extensions missing" (Crash)

```
# Stubs are auto-loaded via LD_PRELOAD
# If still crashes, check: LD_PRELOAD in log.txt
```

### Black screen / Tela preta

```bash
# Smart fallback should auto-detect
# Check log.txt for:
# - "ERROR: Could not queue pageflip"
# - "Smart fallback: auto detected"
#
# Force fbcon manually:
export SDL_VIDEODRIVER=fbcon
export SDL_AUDIODRIVER=alsa
```

### Extraction fails

```bash
# 1. Remove corrupted data:
rm -rf game/

# 2. Re-run:
./"The Sims 3.sh"

# 3. Verify game.s3e header:
hexdump -C game/game.s3e | head -1
# Expected: 58453355 (XE3U)
```

---

## 📖 NextOS Patterns Used

| Pattern | Description | Implementation |
|---------|-------------|----------------|
| **port.json** | Port metadata | ✅ Sims 3 |
| **NXExtract owner-data** | Extraction before game | ✅ Gatito Extractor |
| **Instance lock** | Prevent double-launch | ✅ .nxbootstrap-* flocks |
| **Manifest validation** | Required files check | ✅ NXBOOTSTRAP_REQUIRED_FILES |
| **Capabilities declaration** | Hardware requirements | ✅ NXCOMPAT_REQUIRED_CAPABILITIES |
| **BYO-data** | No game included | ✅ S3E extraction required |
| **LD_PRELOAD stubs** | Missing extensions | ✅ S3E stub libraries |
| **Smart fallback** | Auto video/audio detection | ✅ run.sh smart mode |

---

## 📝 Metadata

```json
{
  "name": "sims3",
  "version": 1,
  "arch": ["armhf", "aarch64"],
  "min_glibc": "2.17",
  "title": "The Sims 3"
}
```

A entrada `aarch64` descreve hosts AArch64 que conseguem executar o loader ARM32; ela não significa que exista um loader AArch64 nativo neste repositório.

---

## 🤝 Contributing

Issues, questions, or improvements? Check out the official port:

**GitHub:** https://github.com/OlhaGatito/The-Sims-3-Android-port

---

## 📄 License

**Game:** EA / The Sims 3 (proprietary)
**Port:** MIT License (see LICENSE)

---

**⚠️ BYO-data warning:** This port requires extracting The Sims 3 Android APK (game.s3e) from your own backup. No game files are included.

---

## 🧰 Build reproduzível com o SDK NextOS

O build de desenvolvimento do loader ARMv7 hard-float deve usar a imagem documentada `nextos-public-sdk:1`; os scripts não substituem o SDK por GCC nativo do WSL.

```bash
bash scripts/nextos-bootstrap.sh
bash scripts/build-loader-nextos.sh
bash scripts/verify-loader-nextos.sh
bash scripts/test-shell-scripts.sh
bash scripts/package-nextos-port.sh
```

O artefato compilado fica em `build/nextos/sims3_s3e_loader` e o pacote BYO-data em `build/nextos/sims3.zip`. Os loaders versionados na raiz e em `loader/` não são sobrescritos por esse fluxo. Consulte [docs/NEXTOS-SDK-FLOW.md](docs/NEXTOS-SDK-FLOW.md) para as verificações e limitações.

A verificação ELF confirma propriedades de compilação/ABI, não a execução do jogo. Compatibilidade de gráficos, áudio, controles, saves e gameplay só pode ser confirmada com testes no dispositivo e logs reais.
