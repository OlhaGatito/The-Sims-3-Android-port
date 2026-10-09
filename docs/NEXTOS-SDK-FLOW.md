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
```

O pacote executável é escrito em `build/nextos/sims3.zip`. O loader de build fica em `build/nextos/sims3_s3e_loader`.

### O que cada etapa faz

- `nextos-bootstrap.sh`: verifica Docker, daemon, imagem do SDK e ferramentas necessárias dentro do container. Não altera os loaders versionados.
- `build-loader-nextos.sh`: compila somente o loader ARMv7 hard-float com o SDK NextOS e escreve o artefato em `build/nextos/`.
- `verify-loader-nextos.sh`: verifica ELF32/ARM, interpretador ARM hard-float, atributo VFP, ligação dinâmica e versão máxima de GLIBC (padrão 2.28).
- `test-shell-scripts.sh`: executa `bash -n` em todos os scripts shell do repositório; não executa ações de dispositivo.
- `package-nextos-port.sh`: repete build, verificação e teste de sintaxe antes de montar um ZIP BYO-data. Não inclui APK, OBB nem assets proprietários.

É possível definir `NEXTOS_SDK_IMAGE` para apontar a uma tag local equivalente, e `MAX_GLIBC` para mudar explicitamente o limite de GLIBC do verificador. O padrão é `nextos-public-sdk:1` e GLIBC 2.28.

## Limites da validação

Build bem-sucedido e inspeção ELF não provam que o jogo inicia nem que gráficos, áudio, input, saves ou gameplay funcionam no aparelho. Esses pontos exigem execução no dispositivo e logs reais. Não marcar compatibilidade de runtime como aprovada apenas com base na compilação.

## Por que os scripts legados não foram apagados nesta etapa

Os scripts existentes incluem o launcher, runtime, detecção de ambiente, fallback e integração NXExtract. Removê-los sem provar suas referências e sem teste no aparelho poderia quebrar o port. A limpeza deve ocorrer depois de mapear chamadas/referências e testar o fluxo de instalação e inicialização. `build-stubs.sh` foi removido porque podia selecionar GCC nativo e gerar stubs sem validar o contrato S3E. O Makefile da raiz agora encaminha os alvos de desenvolvimento aos scripts do SDK NextOS. Os stubs e bibliotecas versionados permanecem preservados e não são regenerados automaticamente até que suas interfaces sejam validadas.
