# The Sims 3 Native Loader

O `sims3_s3e_loader` é um loader nativo para The Sims 3 Android, implementado em C puro.

## O que é

- **Loader nativo ARMv7 + NEON + hard-float** para rodar o jogo em handhelds
- Implementa o runtime Marmalade S3E (originalmente usado no Android)
- Descompacta o S3E LZMA (The Sims 3.s3e) para XE3U format usado pelo runtime
- Toca áudio via ALSA/EGL renderiza gráficos via OpenGL ES

## Como compilar

### ARMv7 (32-bit) - padrão atual
```bash
# Requer toolchain ARM 32-bit (hard-float)
apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# Compila para ARMv7
make

# Limpeza
make clean
```

### AArch64 (64-bit) - experimental
```bash
# Requer toolchain AArch64 64-bit
apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Compilar para AArch64 (modifique Makefile)
# ou defina CC e CFLAGS:
make CC=aarch64-linux-gnu-gcc
CFLAGS="$(echo $CFLAGS | sed 's/-march=armv7-a/-march=armv8-a/')"
```

## Estrutura

```
loader/
├── README.md                  ← este arquivo
├── Makefile                  ← build script
├── sims3_s3e_loader          ← executável compilado
├── include/                  ← cabeçalhos C
│   ├── derbh.h
│   ├── nxmix.h
│   └── ...
├── src/                      ← código-fonte C
│   ├── main.c                ← entry point
│   ├── derbh.c               ← LZMA decompressão
│   ├── s3e_audio.c           ← ALSA audio backend
│   ├── s3e_gl.c              ← OpenGL ES/EGL backend
│   └── ...
└── third_party/              ├── LZMA SDK
    └── lzma/
        └── LzmaDec.c
```

## Interface

```bash
# Para descompactar o S3E
./sims3_s3e_loader --unpack-s3e game/The\ Sims\ 3.s3e game/game.s3e.unpacked

# Para iniciar o jogo
./sims3_s3e_loader --run --root ./game game/game.s3e.unpacked
```

## Dependências

- **Runtime**: SDL2, EGL, libGLESv2
- **Build**: gcc-arm-linux-gnueabihf, arm-linux-gnueabihf-strip
- **De runtime**: libasound2, libegl1-mesa-dev, libgles2-mesa-dev

## Arquitetura suportada

- **ARMv7-a**: NEON, hard-float (padrão)
- **AArch64**: experimental (requer compilação separada)

## Referências

- [Marmalade S3E Documentation](https://docs.madewithmarmalade.com/)
- [LZMA SDK](https://www.7-zip.org/sdk.html)
- [Alsa sound programming](https://www.alsa-project.org/alsa-doc/alsa-lib/)