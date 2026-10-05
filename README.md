# 🎮 The Sims 3 — Android → Linux ARM

> Port experimental de **The Sims 3 (Android)** para handhelds Linux (ARMv7 hard‑float), usando runtime **Marmalade/S3E** e sistema de extração **NXExtract**.

---

## ⚡ Estado atual

| 🚦 | Condição |
|----|----------|
| 🧪 | **Em validação** – loader compilado, mas renderização e áudio ainda precisam de testes em hardware. |
| 📅 | Última atualização: **2026‑10‑05** |

---

## 📦 Como usar (usuário final)

> ⚠️ Nenhum arquivo proprietário (APK, OBB, assets) está neste repositório.  
> Você deve providenciar os arquivos legalmente a partir do seu próprio backup.

### 1️⃣ Preparação

Copie seu **APK** e **OBB** para a pasta `data/`:

```bash
cp ~/Downloads/the-sims-3-*.apk data/
cp ~/Downloads/main.*.obb data/
```

Execute o *setup*:

```bash
./setup.sh
```

Ele vai:
- Extrair o APK
- Descompactar o `.s3e` (usando **NXExtract**)
- Validar os arquivos
- Preparar o *stage* para o loader

### 2️⃣ Execução

Rode o port:

```bash
./run.sh
```

Se houver problema:

```bash
./run-fallback.sh      # versão alternativa
./run-extractor.sh     # apenas prepara dados, sem iniciar o jogo
```

---

## 🗂️ Estrutura do repositório

| 📁 Pasta | Conteúdo |
|---------|----------|
| `loader/` | Runtime Marmalade/S3E compilado (ARMv7‑A hard‑float) |
| `nxextract/` | Motor de extração transacional (stage → hooks → validation → commit) |
| `hooks/` | Scripts que transformam o payload (ex.: descompactação LZMA) |
| `data/` | **Coloque aqui seu APK e OBB!** |
| `sims3_s3e_loader` | Binário do loader pronto |
| `port.json` | Metadata (versão, ABI, reciprocidade) |
| `run*.sh` | Scripts de execução e *fallback* |
| `docs/` | Documentação de arquitetura (veja **Main**) |

---

## 🔍 Como testar

```bash
# Verifique o loader:
ls -l sims3_s3e_loader
file sims3_s3e_loader
readelf -h sims3_s3e_loader

# Rode o *setup* para preparar os dados:
./setup.sh

# Verifique o *stage* preparado:
ls -la stage/game/
```

> Se tudo estiver OK, você verá `stage/game/game.s3e.unpacked` com header **`XE3U`**.

---

## 🛠️ Configuração gráfica (opcional)

Se precisar ajustar a renderização, defina variáveis antes de rodar:

```bash
export SDL_VIDEODRIVER=wayland    # ou fbcon, drm, etc.
export SDL_VIDEO_WIDTH=800
export SDL_VIDEO_HEIGHT=600
./run.sh
```

> Geralmente o CFW já define isso. Ajuste apenas se o jogo não iniciar.

---

## 📚 Documentação detalhada (centralizada)

Veja a documentação técnica completa em:
- **`Main/docs/INSTAL_SIMS3.md`** – instalação passo‑a‑passo
- **`Main/docs/ARCHITECTURE_SIMS3.md`** – arquitetura do loader e NXExtract
- **`Main/docs/CHECKLIST_SIMS3.md`** – diagnóstico completo
- **`Main/docs/COMPATIBILITY-REPORT-TEMPLATE.md`** – como reportar compatibilidade

---

## 🧑‍💻 Como contribuir

1. **Fork** do repositório **Main** (código‑fonte e documentação).
2. Corrija bugs no loader, melhore scripts ou atualize compatibilidade com novos CFWs.
3. Abra *pull‑request* descrevendo o teste realizado.
4. Quando validado pelo CI, uma nova versão será atualizada aqui.

---

## ⚖️ Licença

Código‑fonte sob **GPL‑2.0‑or‑later** (não cobre direitos do jogo).  
Consulte `LICENSE`.

---
