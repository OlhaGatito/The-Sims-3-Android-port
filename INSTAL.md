# Instalação — The Sims 3 Android Port

## Pré-requisitos

- **Handheld ARM Linux com PortMaster/NextOS** (muOS, ArkOS, ROCKNIX, NextOS ou similar)
- **APK** do The Sims 3 (seu próprio backup). O **OBB** é opcional mas recomendado
  (ver "OBB" abaixo).
- **~2 GB de espaço livre** no cartão (jogo extraído).
- **Bibliotecas 32-bit:** SDL2, libEGL, libGLES (geralmente pré-instaladas).
- **Python 3** com módulo `lzma` (para a extração NxExtract; costuma vir com o CFW).

---

## Passo 1: Copiar os arquivos do port

Existem dois caminhos equivalentes:

```bash
# Opção A — baixar o pacote pronto (recomendado):
#   rode no seu PC de desenvolvimento:
make package          # gera build/nextos/sims3.zip
#   e copie o conteúdo de sims3.zip para a pasta de ports do handheld.

# Opção B — copiar o repositório:
git clone https://github.com/OlhaGatito/The-Sims-3-Android-port.git sims3
cd sims3
```

No handheld, acesse a pasta de ports:

```bash
ssh root@handheld
cd /mnt/mmc/MUOS/PortMaster/ports/  # muOS
# ou
cd /opt/roms/ports/                 # ArkOS
# ou
cd /roms/ports/                     # ROCKNIX / NextOS genérico
```

---

## Estrutura do port

```
sims3/
├── The Sims 3.sh             ← launcher PortMaster (ÚNICA entrada do jogo)
├── port.json                 ← manifesto PortMaster (arch, min_glibc, itens)
├── extractor.json            ← receita NxExtract (extração do APK)
├── nxextract/                ← motor de extração NxExtract
│   ├── nxextract.py          ← engine (recipe/hooks/checkpoints)
│   ├── run-extractor.sh      ← entrada chamada pelo launcher
│   ├── nxextract-runtime-env.sh
│   ├── unpack-s3e.py         ← decoder LZMA→XE3U (migrado de hooks/)
│   └── nxextract-ui          ← UI auxiliar (aarch64)
├── libs/                     ← stubs LD_PRELOAD (ARMv7 hard-float)
│   ├── libs3eAndroidJNI.so
│   └── libs3eVFS.so
├── sims3_s3e_loader          ← loader ARMv7 (executável)
├── loader/                   ← código-fonte do loader (fonte única de verdade)
├── scripts/                  ← build/verify/package/test/reconstruct
├── README.md / INSTAL.md / COMPATIBILIDADE.md
└── game/                     ← criado na extração (gitignored, não versionado)
```

---

## Passo 2: Copiar APK (+ OBB)

```bash
# Coloque seu APK em gamedata/ (ou na raiz da pasta do port):
mkdir -p sims3/gamedata
cp ~/The\ Sims\ 3*.apk sims3/gamedata/

# OBB (recomendado, se sua versão usar):
cp ~/main.*.obb sims3/gamedata/
```

> **Sobre o OBB:** o APK sozinho não contém os assets `res.dz`
> (`assets/LowRes/` e `assets/HighRes/`) que o engine lê a partir do OBB.
> Se o jogo reclamar desses arquivos no dispositivo, forneça o OBB na mesma
> pasta. A extração procura OBB automaticamente.

---

## Passo 3: Primeira execução

### Via PortMaster (recomendado)

```bash
# No menu do PortMaster, procure por "The Sims 3" e inicie.
#
# Na primeira vez o launcher roda a extração NxExtract:
#   - extrai assets/ e o .s3e do APK;
#   - descompacta o .s3e (LZMA) para game/game.s3e.unpacked (payload XE3U);
#   - valida o payload.
# Aguarde "Extração concluída" (pode levar alguns minutos).
# O jogo inicia automaticamente em seguida.
```

### Via terminal

```bash
cd sims3
bash "The Sims 3.sh"
```

Nas próximas vezes a extração é pulada (marcador `.nxextract-sims3.json`) e o
jogo inicia direto.

---

## Estrutura após extração

```
sims3/
├── game/
│   ├── game.s3e.unpacked     ← payload XE3U do jogo (descompactado)
│   ├── s3e                   ← .s3e original extraído (entrada do decoder)
│   └── assets/               ← assets (áudio, modelos, texturas, …)
├── .nxextract-sims3.json     ← marcador de instalação (gitignored)
├── nxextract.log             ← log da extração (gitignored)
└── logs/                     ← logs de execução
```

---

## Passo 4: Se houver problema

### Extração falhou

```bash
# Confirme a presença dos arquivos da receita:
ls -la extractor.json nxextract/run-extractor.sh nxextract/unpack-s3e.py

# Veja o log da extração:
tail -n 50 nxextract.log

# Limpe e reextraia:
rm -rf game/ .nxextract-sims3.json
bash "The Sims 3.sh"
```

### Tela preta / sem som

```bash
# Force backends específicos:
export SDL_VIDEODRIVER=fbcon
export SDL_AUDIODRIVER=alsa
bash "The Sims 3.sh"
```

### Erro "incomplete NxExtract integration"

O launcher exige `extractor.json` + todos os arquivos de `nxextract/`. Copie o
port completo (ou o `sims3.zip` gerado por `make package`) em vez de arquivos
solto.

---

## Limpeza (reinstalar)

```bash
# Remova apenas os dados extraídos (APK/OBB são mantidos):
rm -rf sims3/game/ sims3/.nxextract-sims3.json
# A próxima execução reextrai automaticamente.
```

---

## Variáveis de ambiente (avançado)

```bash
# Forçar backend de áudio / vídeo
export SDL_AUDIODRIVER=alsa        # alsa, pulseaudio, pipewire, dsp
export SDL_VIDEODRIVER=fbcon       # fbcon, kmsdrm, x11, wayland

# Resolução customizada
export SIMS3_W=1280
export SIMS3_H=720

# Python customizado para a extração
export NXEXTRACT_PYTHON=/usr/bin/python3.10

# Log customizado
export SIMS3_LOG_DIR=/tmp/sims3-logs

bash "The Sims 3.sh"
```

---

## Validação pós-instalação

```bash
# A extração terminou? (deve conter o payload XE3U)
ls -la sims3/game/game.s3e.unpacked
head -c 4 sims3/game/game.s3e.unpacked | od -An -tx1
# Esperado: 58 45 33 55  (XE3U)

# Loader presente e ELF32 ARM:
file sims3/sims3_s3e_loader
# Esperado: ELF 32-bit LSB executable, ARM, EABI5 hard-float
```

> **Nota:** um build bem-sucedido e a inspeção ELF **não** provam que o jogo
> roda. A validação real (gráficos, áudio, input, saves, gameplay) exige o
> dispositivo. Não marque compatibilidade como aprovada só porque compilou.

---

## Rebuild do loader (desenvolvedor)

A **fonte única de verdade** do loader é `loader/`. O binário rastreado na raiz
(`sims3_s3e_loader`) é um artefato pré-compilado; o artefato canônico é o gerado
pelo build do SDK:

```bash
make loader      # compila em build/nextos/sims3_s3e_loader (SDK NextOS)
make verify      # verifica ELF32/ARM/hard-float/GLIBC
make test        # bash -n em todos os scripts
make package     # gera build/nextos/sims3.zip
make reconstruct # pipeline completo: extração → build → verificação → pacote
```

Todos usam o Docker do NextOS (`nextos-public-sdk:1`); **não** fazem fallback
para o GCC nativo.

---

## Troubleshooting

| Erro | Solução |
|------|---------|
| `loader is not executable` | `chmod +x sims3_s3e_loader` |
| `incomplete NxExtract integration` | Copie o port completo (faltam arquivos de `nxextract/`) |
| `SDL2 not found` | Instale `libsdl2:armhf` (depende do CFW) |
| `Python 3 not found` | Instale Python 3 no seu CFW |
| Extração falha em LZMA | Confirme `python3 -c "import lzma"` |
| Tela preta | Force `SDL_VIDEODRIVER=fbcon` |
| Sem som | Force `SDL_AUDIODRIVER=alsa` |
| Jogo trava ao carregar | Veja `logs/` e `nxextract.log` |

---

## Suporte

Se tiver problemas:
1. Anote a saída do launcher e os logs (`nxextract.log`, `logs/`).
2. Abra uma [issue](https://github.com/OlhaGatito/The-Sims-3-Android-port/issues)
   com CFW, modelo handheld e logs.

---

**Bom jogo!** 🎮
