# Compatibilidade — The Sims 3 Android port

## O que o binário exige

O loader incluído é um executável Linux ARM de 32 bits, hard-float. O Makefile o compila para ARMv7-A com NEON/VFPv4. Por isso, um processador ARM por si só não basta:

- O firmware precisa executar programas Linux ARM de 32 bits com ABI hard-float.
- A CPU precisa oferecer os recursos ARMv7-A, NEON e VFPv4 usados pelo build.
- O runtime precisa oferecer GLIBC 2.22 ou posterior e as bibliotecas ARM32 SDL2, EGL e OpenGL ES necessárias ao aparelho.
- Um CFW AArch64 só é candidato quando inclui a camada de compatibilidade ARM32 e suas bibliotecas. AArch64 sozinho não garante que o loader possa iniciar.
- Python 3 e xz são necessários na primeira preparação dos dados do jogo.

O loader procura EGL/GLES e SDL2 no runtime do sistema. O launcher mantém as escolhas de áudio e vídeo fornecidas pelo CFW; não há um backend universal que funcione em todos os firmwares.

## Situação por ambiente

| Ambiente | Situação conhecida | O que falta |
| --- | --- | --- |
| NextOS | Não validado neste pacote | Confirmar ABI ARM32, bibliotecas gráficas/áudio e iniciar o jogo até gameplay |
| ArkOS / R36S | Não validado neste pacote | Confirmar o modelo/firmware exato, dependências ARM32 e gameplay |
| ROCKNIX | Não validado neste pacote | Confirmar suporte ARM32 e dependências do runtime no aparelho |
| muOS | Não validado neste pacote | Confirmar suporte ARM32, dependências e gameplay |
| Outros CFW com PortMaster | Indeterminado | Avaliar os requisitos acima por aparelho e versão |
| dArkOSRE / RG351MP | Registro preliminar: houve validação do loader, mas o resultado de gameplay não está confirmado | Repetir o teste completo e registrar áudio, vídeo, controles e estabilidade |
| Linux x86 / x86_64 sem emulação ARM | Incompatível com o loader incluído | Exige outro loader/binário |

PortMaster padroniza a instalação e fornece variáveis de controle, mas não torna um binário ARM32 compatível com todos os processadores, bibliotecas e backends de cada CFW. Os caminhos variam; o launcher contempla /PortMaster/, os caminhos muOS /mnt/mmc/MUOS/PortMaster/ e /mnt/sdcard/MUOS/PortMaster/, /opt/system/Tools/PortMaster/ e /opt/tools/PortMaster/.

## Como interpretar os resultados

- **Loader validado** significa somente que o executável iniciou ou passou por uma verificação técnica; não comprova que o jogo chegou ao menu ou que seja jogável.
- **Gameplay validado** exige inicialização até o jogo, teste de áudio, imagem, controles e encerramento normal.
- Toda validação precisa registrar o aparelho, CFW e versão, modelo do processador, backend SDL, resultado e log do port.

Esta revisão foi estática. Não houve execução em um portátil; os ambientes listados continuam sem certificação de gameplay.
