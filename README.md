# The Sims 3 - NextOS Universal Port
**BYO-data** • **NextOS-compatible** • **ARMv7/AArch64**

> Universal port for handheld Linux devices (muOS, ArkOS, ROCKNIX, NextOS, PortMaster)

---

## 📦 Package Structure

```
sims3/
├── The Sims 3.sh          # Launcher (PortMaster compatible)
├── port.json              # PortMaster metadata (NextOS)
├── extractor.json         # Gatito extraction recipe
├── gatito-extract/        # Gatito extractor engine
├── hooks/                 # NXExtract hooks (unpack-s3e.sh)
├── libs.armhf/            # ARMv7 libraries + S3E stubs
├── libs.aarch64/          # AArch64 libraries + S3E stubs
├── run.sh                 # Runtime with smart fallback
├── run-fallback.sh        # Compatibility fallback
├── port_compat.sh         # Audio/video detection
├── sims3-port-bootstrap.sh # PortMaster bootstrap
├── sims3_s3e_loader       # ARMv7 loader (required)
├── sims3_s3e_loader_aarch64 # AArch64 loader (optional)
└── docs/                  # Documentation
```

---

## 🎯 Compatibility Matrix

| CFW | Kernel | Loader | Status |
|-----|--------|--------|--------|
| muOS | ARMv7 | ARMv7 | ✅ Full |
| ArkOS | ARMv7 | ARMv7 | ✅ Full |
| ROCKNIX | AArch64 | ARMv7 (32-bit compat) | ✅ Full |
| NextOS | AArch64 | ARMv7 (32-bit compat) | ✅ Full |
| PortMaster | Mixed | ARMv7 (default) | ✅ Full |

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

```bash
# The launcher will:
# 1. Detect CFW (muOS/ROCKNIX/NextOS/ArkOS)
# 2. Check for existing extraction
# 3. Launch Gatito Extractor (GUI, ~15 min)
# 4. Validate S3E header (XE3U)
# 5. Start game

# Data is stored in: /storage/roms/ports/sims3/game/
```

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

### 2. **gatito-extract/** (Extractor Engine)
- **Engine:** `gatito-extract-v3.py`
- **UI:** `BUILD.ui/gatito-ui.py`
- **Pattern:** Follows NXExtract v3 specification
- **Hooks:** `hooks/unpack-s3e.sh` (S3E LZMA → XE3U)

### 3. **libs.armhf/** & **libs.aarch64/** (Libraries)
- **S3E Stubs:**
  - `libs3eAndroidJNI.so` - JNI interface stub
  - `libs3eVFS.so` - Virtual file system stub
- **Purpose:** Prevent crashes when extensions are missing

### 4. **run.sh** (Runtime)
- **Role:** Smart launcher with fallback
- **Features:**
  - CFW detection
  - Loader selection (ARMv7/AArch64)
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

O artefato compilado fica em `build/nextos/sims3_s3e_loader` e o pacote BYO-data em `build/nextos/sims3-nextos-port.zip`. Os loaders versionados na raiz e em `loader/` não são sobrescritos por esse fluxo. Consulte [docs/NEXTOS-SDK-FLOW.md](docs/NEXTOS-SDK-FLOW.md) para as verificações e limitações.

A verificação ELF confirma propriedades de compilação/ABI, não a execução do jogo. Compatibilidade de gráficos, áudio, controles, saves e gameplay só pode ser confirmada com testes no dispositivo e logs reais.
