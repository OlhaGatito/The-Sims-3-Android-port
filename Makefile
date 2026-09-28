# Cross-compile ARMv7 hard-float (roda em userspace armhf 32 bits).
# Ex.: make CC=arm-linux-gnueabihf-gcc
CC      ?= arm-linux-gnueabihf-gcc
STRIP   ?= $(patsubst %gcc,%strip,$(CC))
CFLAGS  ?= -O2
CFLAGS  += -std=c11 -D_GNU_SOURCE -Wall -Iinclude -Ithird_party -Ithird_party/lzma \
           -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard
LDLIBS  += -ldl -pthread -lm
SRC     := src/derbh.c src/main.c src/nxmix.c src/s3e_audio.c src/s3e_config.c src/s3e_file.c src/s3e_gl.c src/s3e_host.c src/s3e_image.c src/s3e_input.c src/s3e_runtime.c  third_party/lzma/LzmaDec.c
TARGET  := sims3_s3e_loader

all: $(TARGET)
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)
	$(STRIP) -s $@
clean:
	rm -f $(TARGET)
.PHONY: all clean
