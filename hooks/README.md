# The Sims 3 Hooks

Scripts de pós-processamento para Gatito Extractor.

## Hooks disponíveis

### unpack-s3e.sh
Descompacta o arquivo `.s3e` para formato XE3U usado pelo loader.

```bash
# Como usar (automático pelo Gatito)
./unpack-s3e.sh
```

## Interface Gatito

O Gatito chama hooks automaticamente quando:
- Extrator atinge checkpoint (`commit`)
- Hook tem atributo `checkpoint: true`

```json
{
  "hooks": [{
    "id": "unpack-s3e",
    "argv": ["bash", "{recipe_dir}/hooks/unpack-s3e.sh"],
    "checkpoint": true
  }]
}
```

## Ambiente

Variáveis de ambiente disponíveis nos hooks:

- `NXEXTRACT_STAGE`: Diretório temporário de estágio
- `NXEXTRACT_GAME_DIR`: Diretório final do jogo
- `SIMS3_HOOK_LOG`: Log do hook (padrão: `logs/hook-log`)

## Estrutura

```
hooks/
├── README.md                  ← este arquivo
└── unpack-s3e.sh             ← único hook atual
```

## Erros de retorno

| Código | Significado |
|--------|-------------|
| 0 | Sucesso |
| 10 | Arquivo S3E não encontrado no stage |
| 11 | Payload descompactado vazio |
| 12 | Header XE3U inválido |
| 13 | LZMA decode falhou |
| 69 | Loader não encontrado/executável |
| 127 | Falha no sistema |

---

⚠️ Não modifique hooks diretamente — atualize o repositório upstream.