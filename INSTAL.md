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
        ├── run-extractor.sh
        ├── extractor.json
        ├── hooks/
        ├── nxextract/
        ├── gamedata/
        └── game/
~~~

Ao iniciar pelo PortMaster, o runtime verifica os dados instalados e executa o fluxo de preparação quando necessário.

## Dados do jogo

Copie o APK/APKM/APKS/XAPK/OBB que você possui para a pasta gamedata dentro de sims3. Na primeira inicialização, o launcher abre o NXExtract e prepara os arquivos dentro de game. O instalador procura primeiro em gamedata e depois na raiz de sims3.

Os dados proprietários necessários pelo jogo devem permanecer fora do repositório GitHub e ser colocados na pasta `game/` da instalação final.

Não publique nem envie ao GitHub:

- APK;
- OBB;
- assets proprietários;
- bibliotecas originais do jogo;
- saves;
- dumps ou outros dados extraídos do jogo.

O repositório contém somente o código, loader, scripts, arquivos de build e componentes técnicos desenvolvidos para o port.

## Requisitos de execução

- PortMaster com control.txt funcional e Bash.
- Usuário Linux ARM de 32 bits, hard-float, com ARMv7-A, NEON e VFPv4.
- Se o CFW for AArch64, ele também precisa oferecer execução de binários ARM de 32 bits e as respectivas bibliotecas.
- GLIBC 2.22 ou compatível, SDL2, EGL e OpenGL ES disponíveis no ambiente do CFW.
- Python 3 e xz para preparar o APK na primeira inicialização.
- Espaço livre suficiente no cartão para o APK e os dados extraídos; a extração mantém arquivos temporários durante a validação.

O port preserva os backends SDL escolhidos pelo PortMaster/CFW. O modo de compatibilidade alternativo força configurações de áudio/vídeo e deve ser ativado manualmente apenas para diagnóstico, definindo SIMS3_RETRY_FALLBACK=1 no ambiente antes de iniciar o port.

Compatibilidade por firmware e aparelho ainda precisa ser validada em hardware. Consulte [COMPATIBILIDADE.md](COMPATIBILIDADE.md).
