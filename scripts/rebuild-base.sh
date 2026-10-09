#!/usr/bin/env bash
# Preflight and rebuild entry point for the Sims 3 PortMaster port.
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
MODE="${1:---check}"
LOG_DIR="$ROOT/build/diagnostics"
mkdir -p "$LOG_DIR"
LOG="$LOG_DIR/rebuild-base.log"
exec >>"$LOG" 2>&1
die() { printf '[REBUILD][FAIL] %s\n' "$*" >&2; exit 1; }
info() { printf '[REBUILD] %s\n' "$*"; }
case "$MODE" in --check|--build|--package) ;; *) die "uso: $0 [--check|--build|--package]" ;; esac
command -v bash >/dev/null 2>&1 || die "bash ausente"
command -v python3 >/dev/null 2>&1 || die "python3 ausente"
[[ -d "$ROOT/libs" ]] || die "libs/ ausente"
[[ ! -e "$ROOT/libs.armhf" ]] || die "libs.armhf/ antigo ainda existe"
[[ ! -e "$ROOT/hooks" ]] || die "hooks/ antigo ainda existe"
[[ ! -e "$ROOT/sims3-stubs.c" ]] || die "sims3-stubs.c antigo ainda existe"
[[ -s "$ROOT/sims3_s3e_loader" ]] || die "loader versionado ausente"
[[ -x "$ROOT/The Sims 3.sh" ]] || die "launcher ausente ou sem permissão"
python3 -c 'import json,sys; p=json.load(open(sys.argv[1])); e=json.load(open(sys.argv[2])); assert "libs/" in p["items"] and "scripts/" in p["items"]; assert any(x.get("id")=="sims3-android-native-libs" for x in e["extract"]); assert "scripts/unpack-s3e.py" in " ".join(e["hooks"][0]["argv"]); print("[REBUILD][OK] JSON e receita NXExtract válidos")' "$ROOT/port.json" "$ROOT/extractor.json"
bash "$ROOT/scripts/test-shell-scripts.sh"
python3 -c 'import pathlib,sys; r=pathlib.Path(sys.argv[1]); [compile((r/p).read_text(encoding="utf-8"),p,"exec") for p in ("nxextract/nxextract.py","scripts/unpack-s3e.py")]; print("[PY-SYNTAX][OK] NXExtract e decoder")' "$ROOT"
[[ -f "$ROOT/libs/libs3eAndroidJNI.so" && -f "$ROOT/libs/libs3eVFS.so" ]] || die "stubs atuais ausentes em libs/"
if grep -nE 'hooks/unpack-s3e|\$GAMEDIR/libs\.armhf|sims3-stubs\.c' "$ROOT/extractor.json" "$ROOT/port.json" "$ROOT/The Sims 3.sh" "$ROOT/scripts/package-nextos-port.sh"; then die "referência antiga encontrada"; fi
case "$MODE" in
  --check) info "CHECK=PASS; nenhum binário foi substituído" ;;
  --build) bash "$ROOT/scripts/build-loader-nextos.sh"; bash "$ROOT/scripts/verify-loader-nextos.sh" "build/nextos/sims3_s3e_loader"; info "BUILD=PASS" ;;
  --package) bash "$ROOT/scripts/package-nextos-port.sh"; info "PACKAGE=PASS" ;;
esac
info "Log: $LOG"
