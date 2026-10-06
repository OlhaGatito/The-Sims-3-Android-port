# Documentation

Documentação técnica do projeto The Sims 3 Android port.

## Estrutura de diretórios

```
Sims3/
├── README.md                 ← visão geral do projeto
├── INSTAL.md                 ← instruções de instalação
├── COMPATIBILIDADE.md       ← matrix de compatibilidade
├── run.sh                    ← runtime principal
├── port_compat.sh           ← compatibilidade CFW
├── detect_system.sh         ← diagnóstico do sistema
├── The Sims 3.sh            ← wrapper PortMaster
├── run-fallback.sh           ← fallback para resolução de problemas
├── extractor.json            ← configuração extração
├── sims3_s3e_loader         ← loader ARMv7
├── loader/                  ← código fonte do loader
├── gatito-extract/          ├── engine de extração + UI
├── hooks/                   ├── scripts de pós-processamento
├── docs/                    ├── este diretório
└── .github/                 ├── CI/CD workflows
```

## Como contribuir

### Erros
1. [Abra uma issue](https://github.com/OlhaGatito/The-Sims-3-Android-port/issues)
2. Anexe logs (`./run.sh` + `debug.log`)
3. Descreva CFW e modelo handheld

### Features
- Fork → implemente → pull request
- Siga o padrão de código existente
- Adicione logs e testes

### Build
```bash
# Compilar loader ARMv7
make -C loader

# Criar release
git tag vX.Y.Z
git push origin vX.Y.Z
```

## API

### Gatito Extractor
Chamada padrão:
```bash
python3 gatito-extract-v3.py recipe.json --game-dir ./sims3
```

### Loader
```bash
# Descompactar S3E
./sims3_s3e_loader --unpack-s3e entrada.s3e saida.xe3u

# Rodar jogo
./sims3_s3e_loader --run --root ./game game/game.s3e.unpacked
```

## Licença
MIT - ver [LICENSE](../LICENSE)

---

🐱 Gatito Project by OlhaGatito