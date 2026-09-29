# The Sims 3 — Instalação

## Versão do APK

Use a versão **The Sims 3 Android 1.7.11 (APK Award)**, correspondente ao APK utilizado durante o desenvolvimento deste port.

## Onde o APK deve estar

O APK é necessário apenas durante a preparação dos arquivos e deve permanecer fora do repositório GitHub.

Ele pode ficar em uma pasta de trabalho no PC. O APK não deve ser copiado para a instalação final do PortMaster.

## Estrutura de instalação

A instalação pode ficar assim:

~~~text
Roms/
└── ports/
    ├── The Sims 3.sh
    └── sims3/
        ├── sims3_s3e_loader
        ├── run.sh
        └── game/
            └── game.s3e.unpacked
~~~

O launcher deve ser colocado em:

~~~text
/roms/ports/The Sims 3.sh
~~~

O diretório do port deve ficar, por exemplo:

~~~text
/roms/ports/sims3/
~~~

O launcher procura os arquivos necessários e executa o loader.

## Dados do jogo

Os dados proprietários necessários pelo jogo devem permanecer fora do repositório GitHub e ser colocados no diretório game/ conforme a estrutura esperada pelo loader.

O APK, OBB, assets proprietários, bibliotecas originais, saves e outros dados proprietários não devem ser publicados neste repositório.

## Observação

O repositório dedicado contém o código, loader, scripts e demais componentes desenvolvidos para o port. Os dados proprietários são fornecidos separadamente pelo usuário.
