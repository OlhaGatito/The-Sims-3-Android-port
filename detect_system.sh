#!/bin/bash
# The Sims 3 — System detection and compatibility helper
# Detects: CFW, CPU architecture, audio/video backends, library paths

set -u

detect_cfw() {
    # Detect Custom Firmware
    if [ -f "/opt/muos/bin/muos-version" ]; then
        echo "muos"
    elif [ -f "/opt/system/Advanced/Firmware Version.txt" ] || [ -d "/opt/system/bin" ]; then
        echo "arkos"
    elif [ -f "/opt/bin/emulationstation" ] || [ -d "/opt/roms" ]; then
        echo "rocknix"
    elif [ -f "/mnt/vendor/etc/os-release" ] || grep -q "NextOS" /etc/os-release 2>/dev/null; then
        echo "nextos"
    elif [ -d "/mnt/SDCARD" ] || [ -d "/mnt/sdcard" ]; then
        echo "generic"
    else
        echo "unknown"
    fi
}

detect_arch() {
    # Detect CPU architecture
    local machine
    machine=$(uname -m)
    
    case "$machine" in
        aarch64|arm64)
            echo "aarch64"
            ;;
        armv7l|armv7|arm)
            echo "armv7"
            ;;
        x86_64)
            echo "x86_64"
            ;;
        i686|i386)
            echo "x86"
            ;;
        *)
            echo "$machine"
            ;;
    esac
}

detect_arch_abi() {
    # Check if system supports ARM 32-bit
    if [ -f /proc/cpuinfo ]; then
        if grep -q "Features.*neon.*vfpv4" /proc/cpuinfo 2>/dev/null; then
            echo "armv7_neon_vfpv4"
        elif grep -q "Features.*neon" /proc/cpuinfo 2>/dev/null; then
            echo "armv7_neon"
        elif grep -q "arm" /proc/cpuinfo 2>/dev/null; then
            echo "armv7"
        fi
    fi
    
    # Check glibc version
    if command -v ldd >/dev/null 2>&1; then
        ldd --version 2>/dev/null | head -1 | grep -oP '(\d+\.\d+)' | head -1 || echo "unknown"
    fi
}

detect_libs_paths() {
    # Find standard library paths for current architecture
    local arch=$1
    local paths=""
    
    case "$arch" in
        aarch64)
            paths="/lib64:/lib/aarch64-linux-gnu:/usr/lib/aarch64-linux-gnu"
            ;;
        armv7)
            paths="/lib/arm-linux-gnueabihf:/usr/lib/arm-linux-gnueabihf:/lib:/usr/lib"
            ;;
        x86_64)
            paths="/lib64:/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu"
            ;;
        *)
            paths="/lib:/usr/lib"
            ;;
    esac
    
    echo "$paths"
}

detect_sdl2() {
    # Find SDL2 library path
    local arch=$1
    local lib_paths=$2
    
    IFS=':' read -ra paths <<< "$lib_paths"
    for path in "${paths[@]}"; do
        if [ -f "$path/libSDL2.so" ] || [ -f "$path/libSDL2-3.0.so" ]; then
            echo "$path"
            return 0
        fi
    done
    echo ""
}

detect_egl_gles() {
    # Find EGL/GLES libraries
    local arch=$1
    local lib_paths=$2
    
    IFS=':' read -ra paths <<< "$lib_paths"
    for path in "${paths[@]}"; do
        if [ -f "$path/libEGL.so" ] && [ -f "$path/libGLESv2.so" ]; then
            echo "$path"
            return 0
        fi
    done
    echo ""
}

main() {
    local cfw arch abi lib_paths sdl2_path egl_path
    
    echo "=== The Sims 3 System Detection ==="
    echo ""
    
    cfw=$(detect_cfw)
    echo "CFW: $cfw"
    
    arch=$(detect_arch)
    echo "Architecture: $arch"
    
    abi=$(detect_arch_abi)
    echo "ABI/Features: $abi"
    
    echo "Device: $(cat /etc/hostname 2>/dev/null || echo 'unknown')"
    echo "OS: $(grep '^NAME=' /etc/os-release 2>/dev/null | cut -d= -f2 || echo 'unknown')"
    
    echo ""
    echo "=== Library Detection ==="
    
    lib_paths=$(detect_libs_paths "$arch")
    echo "Library paths: $lib_paths"
    
    sdl2_path=$(detect_sdl2 "$arch" "$lib_paths")
    if [ -n "$sdl2_path" ]; then
        echo "SDL2: found at $sdl2_path"
    else
        echo "SDL2: NOT FOUND (required for graphics)"
    fi
    
    egl_path=$(detect_egl_gles "$arch" "$lib_paths")
    if [ -n "$egl_path" ]; then
        echo "EGL/GLES: found at $egl_path"
    else
        echo "EGL/GLES: NOT FOUND (required for rendering)"
    fi
    
    echo ""
    echo "=== Compatibility Check ==="
    
    case "$arch" in
        armv7)
            echo "✓ Architecture: ARMv7 (loader available)"
            ;;
        aarch64)
            echo "⚠ Architecture: AArch64 (loader only for ARMv7 - needs ARM32 support)"
            if ldd --help 2>/dev/null | grep -q "x32"; then
                echo "  System appears to support 32-bit ARM"
            else
                echo "  WARNING: System may not support 32-bit ARM libraries"
            fi
            ;;
        *)
            echo "✗ Architecture: $arch (not supported - loader is ARM only)"
            return 1
            ;;
    esac
    
    if [ -z "$sdl2_path" ] || [ -z "$egl_path" ]; then
        echo "✗ Missing required graphics libraries"
        echo "  Install: libsdl2 libgl1-mesa-glx libegl1"
        return 1
    fi
    
    echo ""
    echo "=== Summary ==="
    echo "CFW=$cfw"
    echo "ARCH=$arch"
    echo "SDL2_PATH=$sdl2_path"
    echo "EGL_PATH=$egl_path"
    echo "LIB_PATHS=$lib_paths"
}

main "$@"
