# Documentação do port

O projeto tem um launcher PortMaster (`The Sims 3.sh`), uma receita NXExtract (`extractor.json`), o motor em `nxextract/`, um decodificador LZMA em `scripts/unpack-s3e.py`, bibliotecas stub em `libs/` e o loader C em `loader/`.

## Fluxo de dados

1. NXExtract localiza o APK fornecido pelo usuário.
2. Extrai `assets/The Sims 3.s3e`, os recursos de `assets/**` e bibliotecas nativas de `lib/**/*.so`.
3. O decodificador Python LZMA-alone cria `game/game.s3e.unpacked` e valida a assinatura `XE3U`.
4. O launcher verifica os dados necessários e inicia o loader.

As bibliotecas Android extraídas ficam em `game/libs/android/`. Os stubs usados pelo port Linux ficam em `libs/`; não misturar esses dois grupos.

## Verificação

`bash scripts/rebuild-base.sh --check` valida JSON, referências, sintaxe shell e Python. `--build` compila um artefato isolado usando o SDK NextOS. `--package` também empacota o port BYO-data.

O runtime completo não é considerado validado até ocorrer um teste no dispositivo.
