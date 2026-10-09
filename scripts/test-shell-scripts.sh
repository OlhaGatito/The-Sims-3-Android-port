#!/usr/bin/env bash
# Syntax-check every shell script in the repository without executing device actions.
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
count=0
failed=0

while IFS= read -r -d '' script; do
  rel="${script#"$ROOT"/}"
  if bash -n "$script"; then
    printf '[SHELL-SYNTAX][OK] %s\n' "$rel"
  else
    printf '[SHELL-SYNTAX][FAIL] %s\n' "$rel" >&2
    failed=$((failed + 1))
  fi
  count=$((count + 1))
done < <(find "$ROOT" -type f -name '*.sh' -not -path "$ROOT/.git/*" -print0)

printf '[SHELL-SYNTAX] scripts=%d failures=%d\n' "$count" "$failed"
(( count > 0 )) || { echo "[SHELL-SYNTAX][ERRO] Nenhum script .sh encontrado." >&2; exit 1; }
(( failed == 0 ))
