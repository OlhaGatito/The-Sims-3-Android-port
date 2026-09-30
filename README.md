# The Sims 3 — port Android para Linux ARM

Pesquisa de porting e loader Linux ARM para a versão Android de **The Sims 3**, baseada no runtime Marmalade/S3E.

> **Estado:** o loader ARMv7 hard-float está compilado para o ambiente Linux ARM. Gráficos, áudio, entrada e demais dependências seguem em validação; este repositório não afirma compatibilidade completa nem gameplay confirmado.

## Instalação e preparação dos dados

Consulte [`INSTAL.md`](INSTAL.md) para a versão de origem, preparação dos arquivos fornecidos pelo usuário e instalação do port.

## Mapa dos componentes

| Componente | Conteúdo e função |
|:--|:--|
| 🧩 **Loader**<br>`loader/` | Código e artefatos do loader Marmalade/S3E. |
| 🛠️ **Build**<br>`Makefile` | Regras de compilação disponíveis. |
| 🎮 **Launchers**<br>`The Sims 3.sh`<br>`The Sims 3 Universal.sh` | Entradas de execução do port. |
| 🔌 **Integração**<br>`hooks/` | Rotinas auxiliares e integração do projeto. |
| 📦 **Extração**<br>`nxextract/`<br>`extractor.json` | Componente e configuração usados na preparação dos dados. |
| 📚 **Documentação**<br>`docs/`<br>`INSTAL.md` | Guias técnicos, arquitetura e instruções de instalação. |

## Dados proprietários

O repositório contém artefatos técnicos do port, mas não deve incluir APK, OBB, dados extraídos, assets ou bibliotecas originais, saves ou dumps proprietários. Os dados necessários a testes são fornecidos separadamente pelo usuário e ficam fora do Git.

## Licença

Consulte [LICENSE](LICENSE) para os termos aplicáveis ao código publicado. A licença do código não concede direitos sobre The Sims 3 ou seus materiais proprietários.
