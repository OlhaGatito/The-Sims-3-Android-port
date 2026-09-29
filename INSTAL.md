# The Sims 3 — Instalação

## Versão do APK

Use a versão **The Sims 3 Android 1.7.11 (APK Award)** correspondente à versão usada no desenvolvimento deste port.

O APK é usado somente durante a preparação dos dados e deve permanecer fora do repositório GitHub.

## Instalação no PortMaster

Coloque o launcher na pasta de ports:

~~~text
/roms/ports/The Sims 3.sh
~~~

O diretório do port pode ficar, por exemplo:

~~~text
/roms/ports/sims3/
~~~

A estrutura final deve conter o loader, o runtime e a pasta de dados do jogo:

~~~text
Roms/
└── ports/
    ├── The Sims 3.sh
    └── sims3/
        ├── sims3_s3e_loader
        ├── run.sh
        └── game/
~~~

Ao iniciar pelo PortMaster, o runtime verifica os dados instalados e executa o fluxo de preparação quando necessário.

## Dados do jogo

Os dados proprietários necessários pelo jogo devem permanecer fora do repositório GitHub e ser colocados na pasta `game/` da instalação final.

Não publique nem envie ao GitHub:

- APK;
- OBB;
- assets proprietários;
- bibliotecas originais do jogo;
- saves;
- dumps ou outros dados extraídos do jogo.

O repositório contém somente o código, loader, scripts, arquivos de build e componentes técnicos desenvolvidos para o port.
