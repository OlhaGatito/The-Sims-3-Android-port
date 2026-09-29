# The Sims 3 — Instalação

## Versão do APK

Use a versão **The Sims 3 Android 1.7.xx (APK Award)**, correspondente ao APK utilizado durante o desenvolvimento deste port.

Não use outra versão sem validar novamente o executável Marmalade/S3E e os dados correspondentes.

## Onde o APK deve estar

O APK é necessário apenas durante a preparação dos arquivos e deve permanecer fora do repositório GitHub.

Ele pode ficar, por exemplo, em uma pasta de trabalho no PC:

~~~text
sims3/
├── The Sims 3 1.7.11.apk
└── game/
~~~

O APK não deve ser copiado para a instalação final do PortMaster.

## Preparação do S3E

Dentro do APK existe o arquivo:

~~~text
assets/The Sims 3.s3e
~~~

Extraia esse arquivo e descomprima o conteúdo LZMA para obter:

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

Verifique o resultado:

~~~bash
xxd -l 16 game/game.s3e.unpacked
~~~

O arquivo deve começar com:

~~~text
58 45 33 55
~~~

que corresponde ao cabeçalho Marmalade/S3E XE3U.

## Estrutura de instalação

Depois da preparação, a instalação pode ficar assim:

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

O diretório do port deve ficar, por exemplo, em:

~~~text
/roms/ports/sims3/
~~~

O launcher procura os arquivos necessários e executa o loader.

## Dados do jogo

Os demais dados proprietários necessários pelo jogo devem permanecer fora do repositório GitHub e ser colocados no diretório game/ conforme a estrutura esperada pelo loader.

O APK, OBB, assets proprietários, bibliotecas originais, saves e outros dados proprietários não devem ser publicados neste repositório.

## Observação

A preparação do APK e dos dados é separada da compilação do loader. O repositório contém o código, loader, scripts e demais componentes desenvolvidos para o port, enquanto os dados proprietários são fornecidos separadamente pelo usuário.
