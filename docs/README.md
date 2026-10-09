# Documentation

Documentação técnica do projeto The Sims 3 Android port.

## Estrutura de diretórios

```
The-Sims-3-Android-port/
├── README.md                 ← visão geral do projeto
├── INSTAL.md                 ← instruções de instalação
├── COMPATIBILIDADE.md        ← matriz de compatibilidade
├── The Sims 3.sh             ← launcher PortMaster (única entrada)
├── port.json                 ← manifesto PortMaster (arch, min_glibc, itens)
├── extractor.json            ← receita de extração (NxExtract)
├── nxextract/                ← motor de extração NxExtract
│   ├── nxextract.py          ← engine (recipe/hooks/checkpoints)
│   ├── run-extractor.sh      ← entrada chamada pelo launcher
│   ├── nxextract-runtime-env.sh
│   ├── unpack-s3e.py         ← decoder LZMA→XE3U (migrado de hooks/)
│   └── nxextract-ui          ← UI auxiliar (aarch64)
├── libs/                     ← stubs LD_PRELOAD (ARMv7 hard-float)
├── sims3_s3e_loader          ← loader ARMv7 (executável)
├── loader/                   ← código-fonte do loader (fonte única de verdade)
├── scripts/                  ← build/verify/package/test/reconstruct
├── docs/                     ← este diretório
└── .github/                  ← CI/CD workflows
```

> Os diretórios legados `run.sh`, `run-fallback.sh`, `detect_system.sh`,
> `port_compat.sh`, `gatito-extract/` e `hooks/` **não existem mais** — foram
> removidos. A extração usa `nxextract/`; o decoder de payload vive em
> `nxextract/unpack-s3e.py`.

## Como contribuir

### Erros
1. [Abra uma issue](https://github.com/OlhaGatito/The-Sims-3-Android-port/issues)
2. Anexe a saída do launcher e os logs (`nxextract.log`, `logs/`)
3. Descreva CFW e modelo handheld

### Features
- Fork → implemente → pull request
- Siga o padrão de código existente
- Adicione logs e testes

### Build
```bash
# Compilar loader ARMv7 (SDK NextOS via Docker; sem fallback para GCC nativo)
make loader

# Pipeline completo (extração → build → verificação → pacote → relatório)
make reconstruct APK=/caminho/para/o.apk

# Criar release
git tag vX.Y.Z
git push origin vX.Y.Z
```

## API

### Extração (NxExtract)
A extração é orquestrada pelo launcher via receita `extractor.json`:
```bash
NXEXTRACT_GAME_DIR=. bash nxextract/run-extractor.sh
```
O passo de descompactação (LZMA→XE3U) é `nxextract/unpack-s3e.py`:
```bash
python3 nxextract/unpack-s3e.py game/s3e game/game.s3e.unpacked \
  --loader ./sims3_s3e_loader
```
(O `--loader` é apenas um fallback para sistemas sem `lzma`; o caminho primário
é o decoder Python validado.)

### Loader
```bash
# Rodar jogo (o launcher chama isto):
./sims3_s3e_loader --run --root ./game game/game.s3e.unpacked

# Descompactar S3E (existe no código-fonte loader/; o binário rastreado da
# raiz não expõe a flag — a extração usa nxextract/unpack-s3e.py):
./loader/sims3_s3e_loader --unpack-s3e entrada.s3e saida.xe3u
```

## Licença
MIT - ver [LICENSE](../LICENSE)

---

🐱 Gatito Project by OlhaGatito
