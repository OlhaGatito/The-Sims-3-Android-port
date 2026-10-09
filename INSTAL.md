# Instalação — The Sims 3 PortMaster

## Requisitos

- Dispositivo Linux portátil com PortMaster e suporte ao loader ARMv7 hard-float.
- Cópia própria do APK compatível do The Sims 3 para Android.
- Espaço livre suficiente para os dados extraídos (aproximadamente 2 GB como estimativa inicial; o tamanho real depende da cópia).
- Não é necessário baixar APK/OBB deste repositório: ele não distribui arquivos proprietários da EA.

## Instalação

1. Baixe o repositório e copie os arquivos do port para a pasta de ports do firmware.
2. Coloque seu APK em `gamedata/` ou na pasta do port, conforme a configuração de busca do `extractor.json`.
3. Abra **The Sims 3** pelo PortMaster.
4. Aguarde o NXExtract concluir a extração e a validação do payload.

A receita espera encontrar `assets/The Sims 3.s3e`. Ela também extrai `assets/**` e as bibliotecas nativas `lib/**/*.so`. O decodificador LZMA grava `game/game.s3e.unpacked` somente se o payload for maior que 1 MiB e começar com a assinatura `XE3U`.

## Estrutura relevante

- `The Sims 3.sh`: launcher.
- `port.json`: metadados do pacote.
- `extractor.json`: receita NXExtract.
- `nxextract/`: motor e interface de extração.
- `scripts/unpack-s3e.py`: descompressor LZMA.
- `libs/`: stubs usados pelo port Linux.
- `game/libs/android/`: bibliotecas nativas extraídas do APK; não são substitutas automáticas dos stubs Linux.
- `loader/`: código-fonte do loader.
- `scripts/`: validação, build e empacotamento.

## Diagnóstico

No diretório do projeto, execute:

```bash
bash scripts/rebuild-base.sh --check
```

Para compilar no ambiente SDK NextOS:

```bash
bash scripts/rebuild-base.sh --build
```

Para compilar, validar e gerar o ZIP BYO-data:

```bash
bash scripts/rebuild-base.sh --package
```

Os modos `--build` e `--package` exigem Docker e a imagem SDK `nextos-public-sdk:1` (ou `NEXTOS_SDK_IMAGE` configurada). O script não instala dependências nem substitui os loaders versionados.

## Limitações conhecidas

- Compilação e extração estáticas não provam que o jogo funciona no dispositivo.
- Compatibilidade de áudio, vídeo, controles, ABI e dados externos precisa ser confirmada por teste real.
- Se a extração falhar, preserve o log do NXExtract e o relatório `build/diagnostics/rebuild-base.log`.
