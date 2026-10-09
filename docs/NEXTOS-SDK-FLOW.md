# Fluxo de build do The Sims 3 usando o SDK NextOS

## Fonte de verdade

O guia de compilação do workspace documenta `nextos-public-sdk:1` e o compilador `arm-linux-gnueabihf-gcc` como ambiente de build ARMv7 hard-float. Os scripts abaixo seguem essa regra e **não** fazem fallback silencioso para o GCC nativo do WSL.

Referência interna: `Guia-compilacao-By-NextOS-lembrancas/docs/ambiente/SDK-NEXTOS-REGRA-DE-BUILD.md`.

## Fluxo reproduzível

Na raiz do repositório:

```bash
bash scripts/nextos-bootstrap.sh
bash scripts/build-loader-nextos.sh
bash scripts/verify-loader-nextos.sh
bash scripts/test-shell-scripts.sh
bash scripts/package-nextos-port.sh

# Ou o pipeline completo (extração → build → verificação → pacote → relatório):
bash scripts/reconstruct-port.sh /caminho/para/o.apk
```

O pacote executável é escrito em `build/nextos/sims3.zip`. O loader de build fica em `build/nextos/sims3_s3e_loader`. O relatório da reconstrução fica em `build/reconstruction-report.txt`.

### O que cada etapa faz

- `nextos-bootstrap.sh`: verifica Docker, daemon, imagem do SDK e ferramentas necessárias dentro do container. Não altera os loaders versionados.
- `build-loader-nextos.sh`: compila somente o loader ARMv7 hard-float com o SDK NextOS e escreve o artefato em `build/nextos/`.
- `verify-loader-nextos.sh`: verifica ELF32/ARM, interpretador ARM hard-float, atributo VFP, ligação dinâmica e versão máxima de GLIBC (padrão 2.28).
- `test-shell-scripts.sh`: executa `bash -n` em todos os scripts shell do repositório; não executa ações de dispositivo.
- `package-nextos-port.sh`: repete build, verificação e teste de sintaxe antes de montar um ZIP BYO-data. Não inclui APK, OBB nem assets proprietários.

É possível definir `NEXTOS_SDK_IMAGE` para apontar a uma tag local equivalente, e `MAX_GLIBC` para mudar explicitamente o limite de GLIBC do verificador. O padrão é `nextos-public-sdk:1` e GLIBC 2.28.

## Limites da validação

Build bem-sucedido e inspeção ELF não provam que o jogo inicia nem que gráficos, áudio, input, saves ou gameplay funcionam no aparelho. Esses pontos exigem execução no dispositivo e logs reais. Não marcar compatibilidade de runtime como aprovada apenas com base na compilação.

## Limpeza de scripts legados

O launcher `The Sims 3.sh` é a única entrada principal do jogo e mantém o padrão PortMaster/NextOS. Foram removidos os caminhos redundantes `run.sh`, `run-fallback.sh`, `port_compat.sh`, `detect_system.sh`, `sims3-port-bootstrap.sh` e `build-stubs.sh`. Na reconstrução também foi removido o diretório `hooks/` (o decoder de payload foi migrado para `nxextract/unpack-s3e.py`) e o `sims3-stubs.c` órfão, e `libs.armhf/` foi consolidado em `libs/`. Permanecem apenas os scripts de extração chamados pelo launcher/recipe (`nxextract/run-extractor.sh`, `nxextract/nxextract-runtime-env.sh` e `nxextract/unpack-s3e.py`) e os scripts de desenvolvimento em `scripts/`. Não foram apagados os loaders nem as bibliotecas versionadas. A compatibilidade em runtime continua pendente de teste real no dispositivo.
