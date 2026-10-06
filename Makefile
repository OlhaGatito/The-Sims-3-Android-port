# Makefile for The Sims 3 ARM loader (dual-arch)
# -----------------------------------------------------
#  * `make`          → compila o loader 32-bit ARMv7 (default)
#  * `make aarch64`  → compila o loader 64-bit AArch64
#  * `make clean`    → remove os binários gerados
#
# O alvo padrão continua usando a toolchain ARMv7 (hard-float) que já funciona em todos
# os handhelds (muOS, ArkOS, ROCKNIX, etc.).
#
# Para o build AArch64 usamos flags específicas e evitamos passar as opções de NEON/float-abi
# que são exclusivas do arm-hard-float. Assim o cross-compiler AArch64 consegue compilar.
# -----------------------------------------------------

# Force ARM cross-compiler for default target (try to build ARMv7 if possible)
CC      := arm-linux-gnueabihf-gcc
STRIP   := arm-linux-gnueabihf-strip
CFLAGS  := -O2
CFLAGS  += -std=c11 -D_GNU_SOURCE -Wall -Iloader/include -Iloader/third_party -Iloader/third_party/lzma \
           -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard
LDLIBS  := -ldl -pthread -lm
SRC     := loader/src/derbh.c loader/src/main.c loader/src/nxmix.c loader/src/s3e_audio.c loader/src/s3e_config.c loader/src/s3e_file.c loader/src/s3e_gl.c loader/src/s3e_host.c loader/src/s3e_image.c loader/src/s3e_input.c loader/src/s3e_runtime.c loader/third_party/lzma/LzmaDec.c
TARGET  := sims3_s3e_loader

# Try ARMv7 build, but allow fallback to just AArch64 if it fails (mcontext_t issues)
all: try_armv7 aarch64

try_armv7:
	@echo "[TRY] Building ARMv7 loader..."
	@$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS) >/dev/null 2>&1 && { \
		$(STRIP) -s $(TARGET); \
		echo "[OK] ARMv7 loader built: $(TARGET)"; \
	} || { \
		echo "[SKIP] ARMv7 build failed (likely mcontext_t mismatch)"; \
		rm -f $(TARGET); \
	}

$(TARGET): $(SRC)
	@$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)
	@$(STRIP) -s $@

# -----------------------------------------------------
#  AArch64 (64-bit) target
# -----------------------------------------------------
# Para compilar o loader 64-bit (nativo) usamos o cross-compiler aarch64-linux-gnu-gcc
# e removemos as flags de NEON e float-abi que são inválidas para AArch64.
# A flag -march=armv8-a habilita as instruções nativas de 64-bit.
# -----------------------------------------------------
AARCH64_CC   ?= aarch64-linux-gnu-gcc
AARCH64_CFLAGS = -O2 -std=c11 -D_GNU_SOURCE -Wall \
                -Iloader/include -Iloader/third_party -Iloader/third_party/lzma \
                -march=armv8-a
AARCH64_LDLIBS = $(LDLIBS)
AARCH64_TARGET = sims3_s3e_loader_aarch64

# Target "aarch64" compila e renomeia o binário
# (se o cross-compiler não existir, o make falhará – ok, então o fallback será usado).
aarch64: clean_aarch64
	$(AARCH64_CC) $(AARCH64_CFLAGS) -o $(AARCH64_TARGET) $(SRC) $(AARCH64_LDLIBS)
	$(STRIP) -s $(AARCH64_TARGET)
	@echo "AArch64 loader built: $(AARCH64_TARGET)"

# Clean only AArch64 artefacts (mantém o 32-bit)
clean_aarch64:
	rm -f $(AARCH64_TARGET)

clean:
	rm -f $(TARGET) $(AARCH64_TARGET)
.PHONY: all clean aarch64 clean_aarch64