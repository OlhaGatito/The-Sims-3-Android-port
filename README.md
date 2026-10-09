# The Sims 3 - NextOS Universal Port
**BYO-data** • **NextOS workflow in progress** • **ARMv7 hard-float**

> Universal port for handheld Linux devices (muOS, ArkOS, ROCKNIX, NextOS, PortMaster)

---

## 📦 Package Structure

```
sims3/
├── The Sims 3.sh           # Launcher PortMaster / runtime ativo
├── port.json               # PortMaster metadata
├── extractor.json          # NXExtract recipe (ainda precisa de validação real)
├── hooks/                  # Hook S3E necessário à extração
├── nxextract/              # Engine, UI, runner e ambiente isolado NXExtract
├── libs.armhf/             # Bibliotecas/stubs ARMv7 versionados
└── sims3_s3e_loader        # Loader ARMv7 hard-float
```

Os scripts de desenvolvimento ficam em `scripts/` e não são necessários no dispositivo.

---

## 🎯 Compatibility Matrix

| CFW | Caminho esperado | Status |
|-----|-----------------|--------|
| muOS | Loader ARMv7 hard-float | Requer teste no aparelho |
| ArkOS | Loader ARMv7 hard-float | Requer teste no aparelho |
| ROCKNIX | ARMv7 via compatibilidade de 32 bits | Requer teste no aparelho |
| NextOS | ARMv7 via compatibilidade de 32 bits | Requer teste no aparelho |
| PortMaster | Depende do firmware e do suporte ARM32 | Requer teste no aparelho |

**Importante:** build e inspeção ELF não equivalem a compatibilidade de runtime. Esta tabela não declara gameplay, áudio, vídeo, controles ou saves aprovados.

---

## 🚀 Quick Start

### Installation (BYO-data)

1. Use seu próprio backup legal do APK do The Sims 3 Android.
2. Extraia os dados do jogo conforme as instruções em `INSTAL.md`.
3. Instale os arquivos do port no diretório `ports/sims3` do PortMaster.
4. Inicie `The Sims 3.sh` pelo PortMaster.

### First Launch

O launcher detecta o firmware, valida os arquivos e chama o runner NXExtract quando a instalação precisa ser feita. **A recipe `extractor.json` ainda precisa de validação contra uma cópia real do APK do Sims 3**; não assuma que a extração está aprovada apenas porque o runner existe.

O payload esperado pelo loader é criado em `game/game.s3e.unpacked`. Mantenha APK/OBB e dados proprietários fora do Git.

---

## 🔧 Components

### 1. `The Sims 3.sh` — launcher único

O launcher PortMaster contém o fluxo ativo: detecção/configuração do firmware, trava de instância, etapa NXExtract, validação do payload, configuração de bibliotecas, fallback de vídeo/áudio e encerramento por sinais.

### 2. `nxextract/` e `hooks/` — dependências de instalação

- `nxextract/nxextract.py`: engine de instalação.
- `nxextract/nxextract-ui`: interface.
- `nxextract/run-extractor.sh`: entrada shell chamada pelo launcher.
- `nxextract/nxextract-runtime-env.sh`: isolamento do ambiente do extrator.
- `hooks/unpack-s3e.sh`: hook de conversão do payload S3E.

Esses scripts permanecem porque são dependências ativas, não launchers duplicados.

### 3. `libs.armhf/` — bibliotecas versionadas

Os arquivos `libs3eAndroidJNI.so` e `libs3eVFS.so` atualmente têm conteúdo idêntico. Isso não prova que implementem corretamente as interfaces JNI/VFS; validar símbolos e comportamento antes de depender deles.

### 4. `scripts/` — fluxo de build NextOS

- `scripts/nextos-bootstrap.sh`: valida Docker, a imagem SDK e ferramentas necessárias.
- `scripts/build-loader-nextos.sh`: compila o loader ARMv7 hard-float com o SDK.
- `scripts/verify-loader-nextos.sh`: verifica arquitetura, ABI, ligação dinâmica e GLIBC.
- `scripts/test-shell-scripts.sh`: valida sintaxe dos scripts shell.
- `scripts/package-nextos-port.sh`: gera o pacote BYO-data sem dados proprietários.

Os scripts legados `build-stubs.sh`, `detect_system.sh`, `port_compat.sh`, `run.sh`, `run-fallback.sh` e `sims3-port-bootstrap.sh` foram removidos. Suas rotas eram redundantes e não eram chamadas pelo launcher principal. O launcher ativo não foi substituído.

---

## 📊 PortMaster Integration

O pacote mantém `port.json` e a entrada `The Sims 3.sh` para integração com PortMaster. A configuração do firmware é carregada do `control.txt` quando disponível.

---

## 🐛 Troubleshooting

### Executável ausente
Verifique se `sims3_s3e_loader` foi instalado e está executável.

### Tela preta
Confira `log.txt` no diretório do port. O launcher detecta alguns casos de framebuffer e erros de pageflip, mas o fallback precisa de teste no dispositivo.

### Extração falha
Consulte os logs em `logs/` e confirme que o backup do jogo é compatível com `extractor.json`. A recipe específica ainda não foi aprovada por teste real do APK.

---

## 🧰 Build reproduzível com o SDK NextOS

Na raiz do repositório:

```bash
bash scripts/nextos-bootstrap.sh
bash scripts/build-loader-nextos.sh
bash scripts/verify-loader-nextos.sh
bash scripts/test-shell-scripts.sh
bash scripts/package-nextos-port.sh
```

O artefato compilado fica em `build/nextos/sims3_s3e_loader` e o pacote BYO-data em `build/nextos/sims3.zip`. Os loaders versionados na raiz e em `loader/` não são sobrescritos por esse fluxo.

O SDK padrão é `nextos-public-sdk:1`; o limite de GLIBC do verificador é 2.28 por padrão. Build e inspeção ELF não provam execução do jogo: gráficos, áudio, controles, saves e gameplay exigem testes no dispositivo com logs reais.

---

## 📄 License

**Game:** EA / The Sims 3 (proprietary)  
**Port:** MIT License (see LICENSE)

**BYO-data:** este repositório não inclui APK, OBB nem dados proprietários do jogo.
