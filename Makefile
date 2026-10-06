# Cross-compile ARMv7 hard-float (roda em userspace armhf 32 bits).
# Ex.: make CC=arm-linux-gnueabihf-gcc
CC      ?= arm-linux-gnueabihf-gcc
STRIP   ?= $(patsubst %gcc,%strip,$(CC))
CFLAGS  ?= -O2
CFLAGS  += -std=c11 -D_GNU_SOURCE -Wall -Iloader/include -Iloader/third_party -Iloader/third_party/lzma \
           -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard
LDLIBS  += -ldl -pthread -lm
SRC     := loader/src/derbh.c loader/src/main.c loader/src/nxmix.c loader/src/s3e_audio.c loader/src/s3e_config.c loader/src/s3e_file.c loader/src/s3e_gl.c loader/src/s3e_host.c loader/src/s3e_image.c loader/src/s3e_input.c loader/src/s3e_runtime.c loader/third_party/lzma/LzmaDec.c
TARGET  := sims3_s3e_loader

all: $(TARGET)
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)
	$(STRIP) -s $@

# AArch64 native build (64-bit)
# Ex.: make aarch64
aarch64:
	$(MAKE) clean
	$(MAKE) CC=aarch64-linux-gnu-gcc CFLAGS="$(subst -march=armv7-a,-march=armv8-a,$(CFLAGS)) -mfloat-abi=lp64"
	@mv $(TARGET) $(TARGET)_aarch64
	@echo "Built $(TARGET)_aarch64"

clean:
	rm -f $(TARGET) $(TARGET)_aarch64

.PHONY: all clean aarch64
