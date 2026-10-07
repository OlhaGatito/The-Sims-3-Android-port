#!/bin/bash
# Sims 3 Build Script - NextOS-style compilation
# Build S3E stub libraries for ARMv7 and AArch64

set +u

# Cross-compilation setup (NextOS pattern)
ARMHF_COMPILER="gcc"
AARCH64_COMPILER="gcc"
ARCH_FLAGS=""
TARGET_FLAGS=""

# Default to native compiler if cross-compilation not available
[ -n "$(command -v arm-linux-gnueabihf-gcc)" ] && ARMHF_COMPILER="arm-linux-gnueabihf-gcc"
[ -n "$(command -v aarch64-linux-gnu-gcc)" ] && AARCH64_COMPILER="aarch64-linux-gnu-gcc"

# Build directories
LIBS_ARMHF="libs.armhf"
LIBS_AARCH64="libs.aarch64"
STUBS_DIR="sims3-stubs-src"

# Create directories
mkdir -p "$LIBS_ARMHF" "$LIBS_AARCH64" "$STUBS_DIR"

# Clean previous builds
clean_build() {
  echo "🧹 Cleaning previous builds..."
  rm -f "$LIBS_ARMHF"/libs3e*.so "$LIBS_AARCH64"/libs3e*.so "$STUBS_DIR"/*.o
}

# Build ARMv7 stubs (required for ARMv7 and AArch64 compatibility)
build_armhf_stubs() {
  echo "🔨 Building ARMv7 stubs..."
  cd "$STUBS_DIR" || exit 1
  
  # Compile with ARMv7 flags
  $ARMHF_COMPILER \
    -fPIC -shared -O2 -Wall -Wextra \
    -march=armv7-a -mfpu=vfp -mfloat-abi=hard \
    -D__ARM__ \
    -o "../$LIBS_ARMHF/libs3eAndroidJNI.so" \
    ../sims3-stubs.c \
    2>/dev/null || {
    echo "⚠️  ARMv7 compilation failed, using generic ARM..."
    # Fallback to generic ARM
    $ARMHF_COMPILER \
      -fPIC -shared -O2 -Wall -Wextra \
      -march=arm -mfloat-abi=hard \
      -D__ARM__ \
      -o "../$LIBS_ARMHF/libs3eAndroidJNI.so" \
      ../sims3-stubs.c \
      2>/dev/null || {
      echo "❌ ARM compilation failed - stubs not available"
      return 1
    }
  }
  
  # Copy for VFS stub as well
  cp "../$LIBS_ARMHF/libs3eAndroidJNI.so" "../$LIBS_ARMHF/libs3eVFS.so"
  
  # Verify
  file "../$LIBS_ARMHF/libs3eAndroidJNI.so" | grep -i "shared" && {
    echo "✅ ARMv7 stubs built successfully"
  }
  
  cd ..
}

# Build AArch64 stubs (optional, for AArch64-only systems)
build_aarch64_stubs() {
  echo "🔨 Building AArch64 stubs..."
  cd "$STUBS_DIR" || exit 1
  
  # Check if we should build AArch64 (only if AArch64 loader exists)
  [ -f "$GAMEDIR/sims3_s3e_loader_aarch64" ] || {
    echo "ℹ️  No AArch64 loader found, skipping AArch64 stub build"
    return 0
  }
  
  $AARCH64_COMPILER \
    -fPIC -shared -O2 -Wall -Wextra \
    -march=armv8-a -mtune=generic \
    -D__AARCH64__ \
    -o "../$LIBS_AARCH64/libs3eAndroidJNI.so" \
    ../sims3-stubs.c \
    2>/dev/null || {
    echo "❌ AArch64 compilation failed"
    return 1
  }
  
  # Copy for VFS stub
  cp "../$LIBS_AARCH64/libs3eAndroidJNI.so" "../$LIBS_AARCH64/libs3eVFS.so"
  
  # Verify
  file "../$LIBS_AARCH64/libs3eAndroidJNI.so" | grep -i "shared" && {
    echo "✅ AArch64 stubs built successfully"
  }
  
  cd ..
}

# Generate loader selection helper
generate_loader_helper() {
  echo "📄 Generating loader selection helper..."
  
  cat > "$GAMEDIR/sims3-loader-selector.sh" << 'EOF'
#!/bin/bash
# Automatic loader selection for ARM/AArch64

ARCH="$(uname -m)"
LOADER="sims3_s3e_loader"

case "$ARCH" in
  aarch64|arm64)
    [ -f "sims3_s3e_loader_aarch64" ] && LOADER="sims3_s3e_loader_aarch64"
    ;;
  arm*|aarch32)
    LOADER="sims3_s3e_loader"
    ;;
  *)
    echo "⚠️  Unsupported architecture: $ARCH, trying ARMv7 loader..."
    ;;
esac

echo "$LOADER"
exit 0
EOF

  chmod +x "$GAMEDIR/sims3-loader-selector.sh"
  echo "✅ Loader selector created"
}

# Create library info
create_library_info() {
  echo "📄 Creating library information..."
  
  cat > "$GAMEDIR/libs-info.txt" << EOF
Sims 3 Stub Libraries
=====================

Built: $(date -u)

Available Libraries:
- libs.armhf/libs3eAndroidJNI.so   [ARMv7 required]
- libs.armhf/libs3eVFS.so           [ARMv7 required]
EOF

  if [ -f "$LIBS_AARCH64/libs3eAndroidJNI.so" ]; then
    echo "- libs.aarch64/libs3eAndroidJNI.so [AArch64]" >> "$GAMEDIR/libs-info.txt"
    echo "- libs.aarch64/libs3eVFS.so         [AArch64]" >> "$GAMEDIR/libs-info.txt"
  fi
  
  echo "✅ Library info created"
}

# Main build function
main() {
  echo "🚀 Building Sims 3 S3E Stub Libraries"
  echo "====================================="
  
  # Clean previous builds
  clean_build
  
  # Build ARMv7 stubs (required)
  build_armhf_stubs || exit 1
  
  # Build AArch64 stubs (optional)
  build_aarch64_stubs
  
  # Generate helpers
  generate_loader_helper
  create_library_info
  
  # Verify final structure
  echo ""
  echo "📁 Final Structure:"
  echo "   $LIBS_ARMhf/libs3eAndroidJNI.so ✓"
  echo "   $LIBS_ARMhf/libs3eVFS.so ✓"
  [ -f "$LIBS_AARCH64/libs3eAndroidJNI.so" ] && echo "   $LIBS_AARCH64/libs3eAndroidJNI.so ✓"
  
  echo ""
  echo "✨ Build completed successfully!"
  echo "📖 NextOS stubs are ready for PortMaster integration"
}

# Run main
main "$@"