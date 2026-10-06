# Gatito Extractor

Motor de extração gráfica para The Sims 3 Android.  
Responsável por descompactar APK, extrair assets, e rodar hooks.

## O que faz

- Busca APK/OBB nos padrões Android
- Extraída assets para `game/assets`
- Descompacta S3E (com hook do loader)
- Interface gráfica (SDL2 ou framebuffer)
- Checagem de checksum e validação

## Como usar

```bash
# Extrator simples (sem UI)
python3 gatito-extract-v3.py recipe.json --game-dir ./sims3

# Com interface gráfica (UI)
./run.sh

# Modo diagnóstico
SIMS3_UI_EXIT_TIMEOUT=5 ./run.sh  # UI fecha após 5s
```

## Estrutura

```
gatito-extract/
├── README.md                 ← este arquivo
├── run.sh                    ← launcher do UI
├── gatito-extract-v3.py      ← engine principal
├── BUILD.ui/                 ← UI em Python + Pygame
│   ├── gatito-ui.py          ← interface gráfica
│   └── font.py               ├── fonte pixelada 5x7
├── hooks/                    ├── scripts de processamento
│   └── unpack-s3e.sh         ├── hook S3E unpack
└── assets/                   ├── ícones, etc. para UI
```

## Dependências

- **Python 3.8+**
- **Pygame**: `pip install pygame`
- **Toolchain**: unzip, xz, 7z

## Fluxo de extração

1. **Busca de entrada**: Procura em `gamedata/` e raiz por APK/OBB
2. **Extração**: Descompacta assets para `game/assets`
3. **S3E unpack**: Hook do loader descompacta The Sims 3.s3e
4. **Validação**: Checa checksum e estrutura
5. **UI exit**: Espera `Start+Select` ou timeout auto

## UI features

- **Padrão Android**: Layout similar ao Play Store
- **Multi idioma**: Pt/En detectado automaticamente
- **Start+Select**: Para sair imediatamente
- **Timeout auto**: Sai após 20s (configurável)
- **Feedback visual**: Barra de progresso

## Variáveis de ambiente

```bash
SIMS3_UI_EXIT_TIMEOUT=10  # tempo de espera do UI (padrão 20s)
SDL_VIDEODRIVER=fbcon    # força framebuffer em handhelds
SIMS3_LOG_DIR=./logs     # customiza local dos logs
```

## Erros comuns

| Erro | Solução |
|------|---------|
| `SDL2 not found` | Instale `pip install pygame` |
| `ERROR: no APK/OBB found` | Coloque o arquivo na pasta `gamedata/` ou raiz |
| `pageflip -22` | Rode com `SDL_VIDEODRIVER=fbcon` |

## Fontes

- [Fonte pixelada](font.py): 5x7 customizada para UI
- [Pygame docs](https://www.pygame.org/docs/)

---

⭐ Star este projeto se te ajudar com The Sims 3 em handhelds!