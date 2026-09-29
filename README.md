# The Sims 3 — Android Port

Porting research and Linux ARM loader for the Android version of **The Sims 3**.

## Versão do APK

Para este port, use a versão **The Sims 3 Android 1.7.11 (APK Award)**, correspondente ao APK utilizado durante o desenvolvimento e aos dados S3E atualmente analisados.

**Não use outra versão do APK sem validar novamente o formato .s3e, os símbolos/imports e os dados do jogo.** Versões diferentes podem conter um executável Marmalade/S3E diferente e não são consideradas compatíveis automaticamente.

O arquivo esperado dentro do APK é:

~~~text
assets/The Sims 3.s3e
~~~

O port utiliza a imagem S3E descomprimida como:

~~~text
game/game.s3e.unpacked
~~~

### Importante

O APK, OBB e os dados extraídos do jogo **não devem ser enviados para este repositório**. Cada usuário deve obter legalmente sua própria cópia do APK e manter os dados proprietários fora do Git.

## Instalação

### 1. Obtenha o APK correto

Tenha localmente o APK **The Sims 3 Android 1.7.11 (APK Award)**.

Não é necessário enviar o APK para o GitHub.

### 2. Prepare o diretório do port

A instalação pode ficar, por exemplo:

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

### 3. Extraia o executável S3E do APK

Dentro do APK, localize:

~~~text
assets/The Sims 3.s3e
~~~

Copie esse arquivo para uma área de trabalho fora do repositório e descomprima o conteúdo LZMA. O resultado deve ser:

~~~text
game/game.s3e.unpacked
~~~

Exemplo no Linux/WSL:

~~~bash
mkdir -p game
unzip -p "The Sims 3 1.7.11.apk" "assets/The Sims 3.s3e" > game/The-Sims-3.s3e
xz --format=lzma -d -c game/The-Sims-3.s3e > game/game.s3e.unpacked
rm -f game/The-Sims-3.s3e
~~~

Confirme o cabeçalho:

~~~bash
xxd -l 16 game/game.s3e.unpacked
~~~

O arquivo deve começar com:

~~~text
58 45 33 55
~~~

correspondente ao cabeçalho Marmalade/S3E XE3U.

### 4. Instale os dados do jogo

Os demais dados proprietários necessários pelo jogo devem permanecer no diretório game/ conforme a estrutura esperada pelo loader.

**Não copie APK, OBB, assets proprietários ou bibliotecas originais para este repositório GitHub.**

### 5. Instale o launcher no PortMaster

Coloque o launcher na pasta de ports do sistema:

~~~text
/roms/ports/The Sims 3.sh
~~~

e o diretório do port, por exemplo:

~~~text
/roms/ports/sims3/
~~~

O launcher procura o diretório do jogo e executa o loader quando os arquivos necessários estão presentes.

## Estado atual da instalação

O loader ARMv7 hard-float é compilado para o ambiente Linux ARM compatível com o port. A presença de sims3_s3e_loader e game/game.s3e.unpacked não significa, por si só, que todos os subsistemas do jogo estejam finalizados; gráficos, áudio, entrada e demais dependências continuam sendo validados separadamente.

## Public repository scope

This repository contains only technical artifacts developed for the port:

- source code;
- loader;
- build files;
- launcher scripts;
- compiled port/loader binaries;
- permitted technical `.unpacked` artifacts used by the loader;
- documentation and configuration.

### Proprietary game data is not included

The repository must never contain:

- APK files;
- OBB files;
- extracted APK/OBB game data or assets;
- original proprietary libraries/assets;
- saves, dumps or equivalent proprietary content.

Game data required for testing is supplied separately by the user and remains outside the repository.

## Current project

The current loader targets the Marmalade/S3E executable found in the Android release.

Architecture and runtime details are documented as the implementation advances.

## License

See [LICENSE](LICENSE).
