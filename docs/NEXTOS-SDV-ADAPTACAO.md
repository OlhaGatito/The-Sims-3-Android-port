# Adaptação da arquitetura sdvnextos do NextOS

> **Nota — estado atual:** esta página é um **registro histórico** da adaptação
> de 2026-09-28 (backup `backup/pre-sdvnextos-adaptation-2026-09-28`). Desde
> então o `run.sh` foi **removido**; hoje o runtime é embutido no launcher único
> `The Sims 3.sh`, que chama o NxExtract (`nxextract/run-extractor.sh`) e só
> então o loader. As menções a `run.sh` abaixo descrevem o estado daquela
> adaptação, não o repositório atual. Fluxo atual: ver `NEXTOS-SDK-FLOW.md`.

## Objetivo

Este port passa a seguir a arquitetura de runtime usada pelo `sdvnextos` do NextOS onde ela é aplicável ao The Sims 3:

```
PortMaster
  ↓
wrapper fino / launcher
  ↓
run.sh único
  ↓
control.txt + mod do CFW + get_controls
  ↓
ambiente firmware/PortMaster
  ↓
NXExtract somente quando necessário
  ↓
pm_platform_helper
  ↓
loader ARM32 S3E
```

A referência foi o port oficial Stardew Valley NextOS, especialmente as versões que corrigiram a descoberta do runtime em muOS/RG 40XX-H, abertura antecipada do log, runtime único e handoff por `pm_platform_helper`. O objetivo é adaptar a arquitetura, não copiar componentes específicos do Stardew.

## O que foi implementado

- `run.sh` continua sendo o único runtime interno.
- O launcher permanece fino e resolve o caminho real do port.
- O runtime abre `debug.log` antes de executar PortMaster/NXExtract.
- Adicionados caminhos PortMaster usados por muOS/Anbernic, incluindo `/mnt/mmc/MUOS/PortMaster` e `/mnt/sdcard/MUOS/PortMaster`.
- `control.txt`, `mod_${CFW_NAME}.txt` e `get_controls` são tratados como preâmbulo PortMaster.
- `PORT_32BIT=Y` é exportado para este loader ARM32.
- `SDL_GAMECONTROLLERCONFIG` do PortMaster é preservado.
- Bibliotecas do firmware/PortMaster são priorizadas antes das bibliotecas específicas do port.
- O áudio permanece negociado pelo SDL/firmware por padrão; `SIMS3_AUDIO_DRIVER` ou `AUDIO_DRIVER` podem selecionar ALSA/Pulse/PipeWire para diagnóstico.
- NXExtract continua transacional e só é chamado quando S3E, payload ou marcadores de assets estão ausentes.
- `pm_platform_helper` é chamado imediatamente antes do loader.
- O runtime não gerencia frontend, não mata processos e não usa `exec` para substituir o processo do launcher.
- O código continua BYO-data e não adiciona APK/OBB/assets proprietários ao repositório.

## Backup

Antes desta alteração foi criado o branch:

`backup/pre-sdvnextos-adaptation-2026-09-28`

Ele preserva o estado anterior à adaptação.

Estado preservado:
- `run.sh` blob SHA: `80ceada45b22be38bc25d861b260470883e0016b`
- `The Sims 3.sh` blob SHA: `a7b1636c0b46a0dcfee57ffd4816b7923dc0ef3f`

## Limitação atual

Esta alteração ainda não declara que o problema de áudio está resolvido no RG40XX-H. O aparelho já demonstrou `/dev/snd` e placas ALSA disponíveis, e o launcher antigo funciona nesse mesmo ambiente. Portanto a próxima validação deve comparar o ambiente efetivo do launcher antigo com este runtime, principalmente:

- `LD_LIBRARY_PATH`
- `SDL_AUDIODRIVER`
- `AUDIODEV`
- `ALSOFT_*`
- `SDL_GAMECONTROLLERCONFIG`
- ordem do `pm_platform_helper`
- diretório de trabalho
- variáveis exportadas pelo `control.txt`

## Referência

Arquitetura baseada na documentação oficial do NextOS:

`NextOs-Ports/nextos-framework/ports/stardewvalley-nextos/upstream/README.md`

A adaptação é específica do runtime Marmalade/S3E do The Sims 3; não se assume que shims, OpenAL, Mono/.NET ou componentes exclusivos do Stardew sejam reutilizáveis aqui.
