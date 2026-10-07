# Gatito Extractor - NextOS Integration Guide

## Overview

The Gatito Extractor has been fully integrated into the NextOS universal port pattern. It now uses NxExtract-style UI, runtime environment, and phase management while maintaining the Gatito extraction engine.

## Integration Architecture

```
Gatito Extractor
├── nxextract-ui              # NxExtract-style UI (NEW)
├── nxextract-runtime-env.sh  # Runtime environment (NEW) 
├── run-extractor.sh           # NextOS wrapper (NEW)
├── gatito-extract-v3.py       # Core extraction engine
├── BUILD.ui/gatito-ui.py      # Legacy UI (deprecated)
└── hooks/unpack-s3e.sh        # S3E unpack hook
```

## NextOS Compatibility

### 1. NxExtract UI (`nxextract-ui`)

- **Progress Display:** Phase-based progress bar
- **Color Logging:** Red/Yellow/Blue/Green terminal colors
- **Timeout Handling:** Configurable extraction timeout
- **Error Messages:** Clear error reporting
- **Success Confirmation:** Final validation report

**Example Output:**
```bash
[12:34:56] INFO: Starting Sims 3 NxExtract v2.0.0
[12:34:57] OK: Found 1 package(s): The Sims 3.s3e
[12:34:57] INFO: === PREPARING WORKSPACE ===
█░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 0% - Scanning packages...
████████████████████████████████████████████████████ 100% - Extraction complete
[12:35:45] OK: === EXTRACTION COMPLETE ===
🎮 Ready to launch with: /storage/roms/ports/sims3/run.sh
```

### 2. Runtime Environment (`nxextract-runtime-env.sh`)

Sets up NextOS-style environment variables:

```bash
export NXEXTRACT_VERSION="2.0.0"
export NXEXTRACT_ID="sims3-s3e"
export NXEXTRACT_CFW="rocknix"
export NXEXTRACT_ARCH="aarch64"
export NXEXTRACT_LIB_DIR="$GAME_DIR/libs.armhf"
export LD_PRELOAD="$NXEXTRACT_LIB_DIR/libs3eAndroidJNI.so:$NXEXTRACT_LIB_DIR/libs3eVFS.so"
```

### 3. NextOS Launcher Integration (`The Sims 3.sh`)

**Before:**
```bash
NXEXTRACT_GAME_DIR="$GAMEDIR" bash "$GAMEDIR/gatito-extract/run.sh"
```

**After (NextOS pattern):**
```bash
NXEXTRACT_GAME_DIR="$GAMEDIR" bash "$GAMEDIR/gatito-extract/run-extractor.sh"
```

### 4. Phase Management

Extraction now follows NxExtract phases:

| Phase | Description | Gatito Equivalent |
|-------|-------------|------------------|
| **PREPARING WORKSPACE** | Setup directories | Prepare extraction area |
| **SEARCHING PACKAGES** | Find S3E files | Search input files |
| **VALIDATING INPUT** | Check S3E header | Validate input package |
| **EXTRACTING S3E** | Copy S3E to stage | Copy to working area |
| **UNPACKING LZMA** | Run unpack hook | Run S3E unpack |
| **VALIDATING PAYLOAD** | Check XE3U header | Validate unpacked data |
| **INSTALLING ASSETS** | Copy assets to game | Commit phase |
| **COMMITTING DATA** | Finalize extraction | Rename stage to game |
| **EXTRACTION COMPLETE** | Success validation | Final check |

## Usage

### Direct Launch
```bash
# With NxExtract UI (recommended)
cd /storage/roms/ports/sims3
./gatito-extract/nxextract-ui extractor.json .

# Legacy Gatito UI (deprecated)
./gatito-extract/BUILD.ui/gatito-ui.py extractor.json .
```

### Integration in NextOS Launcher
The `The Sims 3.sh` launcher automatically calls:
```bash
NXEXTRACT_GAME_DIR="$GAMEDIR" \
  bash "$GAMEDIR/gatito-extract/run-extractor.sh"
```

## Configuration

### Timeout Configuration
In `nxextract-ui`, modify the TIMEOUT variable:
```bash
TIMEOUT="${4:-1800}"  # 30 minutes default
```

### Custom Phases
Edit `nxextract-ui` to modify PHASES array:
```bash
PHASES=(
    "PREPARING WORKSPACE"
    "SEARCHING PACKAGES"
    # ... etc
)
```

### Logging
All logs go to:
```bash
NXEXTRACT_LOG="${SIMS3_GAME_DIR}/logs/nxextract.log"
```

## Migration from Gatito UI

### 1. Replace UI Call
**Before:**
```bash
timeout 900 python3 "$GATITO_UI" >>"$GATITO_LOG" 2>&1
```

**After:**
```bash
"$GATITO_EXTRACTOR" "$RECIPE" "$GAME_DIR" "$GATITO_LOG" "1800"
```

### 2. Update Progress Parsing
**Before:** Parse `GATITO_STAGE|percent|message`
**After:** Use NxExtract progress format with phase display

### 3. Add Environment Setup
Source `nxextract-runtime-env.sh` before extraction:
```bash
source "$SCRIPT_DIR/nxextract-runtime-env.sh"
```

## Error Handling

### Common Errors and Solutions

| Error | Solution |
|-------|----------|
| **No Android packages found** | Copy The Sims 3 APK to `gamedata/` |
| **Recipe not found** | Verify `extractor.json` exists |
| **Hook execution failed** | Check `hooks/unpack-s3e.sh` permissions |
| **S3E validation failed** | Check S3E header with `hexdump -C` |
| **Timeout exceeded** | Increase timeout in `nxextract-ui` |

### Exit Codes
- `0`: Success
- `1`: General error (missing files, validation failed)
- `2`: Timeout
- `10`: S3E not found
- `11`: S3E header invalid
- `12`: Assets missing

## Performance Features

### 1. Fast Path Detection
If valid extraction already exists, extraction is skipped:
```bash
if [ -f "$GAME_IMAGE" ] && [ -d "$ASSET_DIR" ]; then
    # Fast path - already extracted
    exit 0
fi
```

### 2. Progress Monitoring
Real-time progress display with timeout monitoring:
```bash
# Show elapsed time
printf "⏱️  Time: %02d:%02d" $((elapsed / 60)) $((elapsed % 60))
```

### 3. Automatic Cleanup
Cleanup temporary files after completion:
```bash
rm -rf "$NXEXTRACT_STAGING_DIR"
```

## Testing

### Manual Test
```bash
# Setup test data
mkdir -p gamedata
# Copy The Sims 3 APK to gamedata/

# Test extraction
./gatito-extract/nxextract-ui extractor.json .
```

### Integration Test
```bash
# Test in NextOS launcher context
cd /storage/roms/ports/sims3
./"The Sims 3.sh"
```

## Dependencies

### Required Packages
- `python3` with json module
- `jq` (for recipe parsing)
- `bash` 4.0+
- `od` (for S3E validation)
- `timeout` command

### Optional Packages
- `ffmpeg` (for advanced media handling)
- `7z` (for archive extraction)
- `libpcre` (for regex patterns)

## Future Enhancements

### Planned Features
- NxExtract v3 API compliance
- Multi-threaded extraction
- Cache management
- Network-based package retrieval
- Progress persistence across reboots

### Known Issues
- Large S3E files (>2GB) may timeout
- Complex APK structures may fail validation
- Some Android packages have non-standard formats

---

**Status:** ✅ Fully integrated with NextOS patterns  
**UI:** ✅ NxExtract-style progress and error handling  
**Engine:** ✅ Gatito extraction engine with phase management  
**Compatibility:** ✅ NextOS universal port specification