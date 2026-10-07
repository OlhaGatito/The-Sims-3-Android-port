# Sims 3 Build System - NextOS-style compatibility
# ARMv7 + AArch64 stub compilation
CC ?= gcc
CFLAGS ?= -fPIC -shared -O2 -Wall -Wextra
STUBS_DIR = libs.armhf
AArch64_DIR = libs.aarch64

# Directories
LIBS_ARMHF = $(STUBS_DIR)
LIBS_AARCH64 = $(AArch64_DIR)
LIBS_COMMON = libs

# Create library directories
$(LIBS_ARMHF) $(LIBS_AARCH64) $(LIBS_COMMON):
	mkdir -p $@

# Compile ARMv7 stubs
STUBS_ARMHF = $(STUBS_DIR)/libs3eAndroidJNI.so $(STUBS_DIR)/libs3eVFS.so
$(STUBS_ARMHF): $(LIBS_ARMHF) sims3-stubs.c
	$(CC) $(CFLAGS) -march=armv7-a -mfpu=vfp -mfloat-abi=hard $< -o $@
	@echo "Built ARMv7 stub: $@"

# Compile AArch64 stubs
STUBS_AARCH64 = $(AArch64_DIR)/libs3eAndroidJNI.so $(AArch64_DIR)/libs3eVFS.so
$(STUBS_AARCH64): $(LIBS_AARCH64) sims3-stubs.c
	$(CC) $(CFLAGS) -march=armv8-a -mtune=generic $< -o $@
	@echo "Built AArch64 stub: $@"

# Common utility libraries (SDL fallback, etc.)
COMMON_UTILS = $(LIBS_COMMON)/sims3-fbcon-helper
$(COMMON_UTILS): libs
	@echo "Creating fbcon helper for fallback mode"
	@echo '#!/bin/bash' > $@
	@echo 'export SDL_VIDEODRIVER=fbcon' >> $@
	@echo 'export SDL_AUDIODRIVER=alsa' >> $@
	@echo 'exec "$$@"' >> $@
	@chmod +x $@

# Build all
all: $(STUBS_ARMHF) $(STUBS_AARCH64) $(COMMON_UTILS)
	@echo "All stubs and utilities built successfully"

# Build for specific architecture
armhf: $(STUBS_ARMHF)
	@echo "ARMv7 stubs built"

aarch64: $(STUBS_AARCH64)
	@echo "AArch64 stubs built"

# Clean
clean:
	rm -rf $(STUBS_ARMHF) $(STUBS_AARCH64) $(LIBS_COMMON)/sims3-fbcon-helper
	@echo "Cleaned stubs and utilities"

# Install to system (for development)
install: all
	@echo "Installing stubs to system paths"
	sudo cp -r $(STUBS_DIR)/* /usr/lib/ 2>/dev/null || true
	sudo cp -r $(AArch64_DIR)/* /usr/lib/ 2>/dev/null || true
	sudo ldconfig 2>/dev/null || true

# Test stubs
test: all
	@echo "Testing stub loading"
	@echo "Testing ARMv7 stub:"
	LD_PRELOAD=$(STUBS_DIR)/libs3eAndroidJNI.so readelf -d $(STUBS_DIR)/libs3eAndroidJNI.so | grep NEEDED || echo "No dependencies found - stub is self-contained"
	@echo "Testing AArch64 stub:"
	LD_PRELOAD=$(AArch64_DIR)/libs3eAndroidJNI.so readelf -d $(AArch64_DIR)/libs3eAndroidJNI.so | grep NEEDED || echo "No dependencies found - stub is self-contained"

.PHONY: all armhf aarch64 clean install test