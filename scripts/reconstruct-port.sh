#!/usr/bin/env bash
# The Sims 3 — driver canônico de reconstrução.
#
# Executa o pipeline completo:
#   1 pre-checagens · 2 verificação do APK/hashes · 3 extração (libs+recursos)
#   4 descompactação/validação do .s3e · 5 preparação da estrutura do jogo
#   6 compilação do loader · 7 verificação do launcher/port.json
#   8 empacotamento · 9 relatório final
#
# Garantias (não-destrutivo):
#   * idempotente: reexecutar produz o mesmo resultado sem corromper estado;
#   * NUNCA executa `make clean`, NUNCA instala dependências;
#   * NUNCA apaga nem sobrescreve arquivos rastreados — qualquer substituição
#     (se houver) é feita com backup prévio em build/reconstruct/backup/;
#   * não executa binários ARM no host: apenas compila (SDK) e inspeciona
#     estaticamente (readelf/strings/grep);
#   * registra objetivo, comando, rc e interpretação de cada etapa.
#
# Uso:
#   bash scripts/reconstruct-port.sh [CAMINHO_DO_APK]
#   SIMS3_APK=/caminho/do.apk bash scripts/reconstruct-port.sh
#
# Variáveis:
#   SIMS3_APK          caminho do APK (senão $1; senão, etapas 2-4 são puladas)
#   SIMS3_APK_SHA256   hash esperado do APK (default: fingerprint conhecido)
#   NEXTOS_SDK_IMAGE   imagem do SDK NextOS (default: nextos-public-sdk:1)

set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
WORK="$ROOT/build/reconstruct"
LOGS="$WORK/logs"
BACKUP="$WORK/backup"
REPORT="$ROOT/build/reconstruction-report.txt"
SDK_IMAGE="${NEXTOS_SDK_IMAGE:-nextos-public-sdk:1}"

EXPECTED_APK_SHA256="${SIMS3_APK_SHA256:-07709E5A8D261E7EDAF43CEDAE7DB0FC193532C03546A67E5A62823E2CCA0E18}"

# Hashes de integridade dos binários protegidos (baseline do repositório).
# São apenas registrados e comparados ao final: a reconstrução NÃO os altera.
PROTECTED_BINARIES=(
  "sims3_s3e_loader"
  "loader/sims3_s3e_loader"
  "libs/libs3eAndroidJNI.so"
  "libs/libs3eVFS.so"
)

declare -a STAGE_NAMES=()
declare -a STAGE_STATES=()
declare -a STAGE_NOTES=()
APK=""
EXTRACT_OK=0
BUILD_OK=0

# ---------------------------------------------------------------------------
# utilidades
# ---------------------------------------------------------------------------
log() { printf '[RECONSTRUCT] %s\n' "$*"; }

# Converte caminhos Windows (C:\x) para /mnt/c/x quando rodando no WSL/Linux.
normalize_path() {
  local p="$1"
  case "$p" in
    [A-Za-z]:\\*|[A-Za-z]:/*)
      local drive="${p:0:1}"
      local rest="${p:3}"
      rest="${rest//\\//}"
      printf '/mnt/%s/%s' "${drive,,}" "$rest"
      ;;
    [A-Za-z]:) printf '/mnt/%s' "${p:0:1}" ;;
    *) printf '%s' "$p" ;;
  esac
}

# sha256 portátil (sha256sum no Linux; fallback para python3).
sha256_of() {
  local f="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$f" | awk '{print toupper($1)}'
  else
    python3 - "$f" <<'PY'
import hashlib, sys
print(hashlib.sha256(open(sys.argv[1], "rb").read()).hexdigest().upper())
PY
  fi
}

have() { command -v "$1" >/dev/null 2>&1; }

record_stage() { # name state note
  STAGE_NAMES+=("$1"); STAGE_STATES+=("$2"); STAGE_NOTES+=("$3")
  log "etapa $1 -> $2${3:+ ($3)}"
}

json_valid() { python3 - "$1" <<'PY'
import json, sys
json.load(open(sys.argv[1], encoding="utf-8"))
PY
}

# ---------------------------------------------------------------------------
# etapa 1: pre-checagens de ferramentas e arquivos
# ---------------------------------------------------------------------------
stage1_prechecks() {
  local missing=()
  for tool in bash python3; do
    have "$tool" || missing+=("$tool")
  done
  # ferramentas de build/empacotamento (opcionais: avisam, não bloqueiam etapa)
  local optional=(git docker readelf make)
  local opt_missing=()
  for tool in "${optional[@]}"; do have "$tool" || opt_missing+=("$tool"); done

  local required_files=(
    "The Sims 3.sh" "port.json" "extractor.json" "Makefile"
    "nxextract/nxextract.py" "nxextract/run-extractor.sh"
    "nxextract/unpack-s3e.py"
    "loader/Makefile" "loader/src/main.c"
    "scripts/build-loader-nextos.sh" "scripts/package-nextos-port.sh"
    "libs/libs3eAndroidJNI.so" "libs/libs3eVFS.so"
  )
  local files_missing=()
  for rel in "${required_files[@]}"; do
    [ -e "$ROOT/$rel" ] || files_missing+=("$rel")
  done

  {
    echo "== etapa 1: pre-checagens =="
    echo "root=$ROOT"
    echo "obrigatorios OK: $((${#required_files[@]} - ${#files_missing[@]}))/${#required_files[@]}"
    echo "ferramentas obrigatorias ausentes: ${missing[*]:-nenhuma}"
    echo "ferramentas opcionais ausentes: ${opt_missing[*]:-nenhuma}"
    echo "arquivos obrigatorios ausentes: ${files_missing[*]:-nenhum}"
  } >"$LOGS/stage1.log"

  if [ ${#missing[@]} -ne 0 ] || [ ${#files_missing[@]} -ne 0 ]; then
    record_stage "1-prechecks" "FALHOU" "faltam: ${missing[*]:-} ${files_missing[*]:-}"
    return 1
  fi
  if [ ${#opt_missing[@]} -ne 0 ]; then
    record_stage "1-prechecks" "OK" "opcionais ausentes: ${opt_missing[*]}"
  else
    record_stage "1-prechecks" "OK" "todas as ferramentas presentes"
  fi
  return 0
}

# ---------------------------------------------------------------------------
# etapa 2: verificação do APK e dos hashes
# ---------------------------------------------------------------------------
stage2_verify_apk() {
  {
    echo "== etapa 2: verificacao do APK e hashes =="
  } >"$LOGS/stage2.log"

  # integridade dos binários protegidos (baseline; não são alterados aqui)
  {
    echo "-- hash binarios protegidos (baseline) --"
    for rel in "${PROTECTED_BINARIES[@]}"; do
      if [ -f "$ROOT/$rel" ]; then
        printf '%s  %s\n' "$(sha256_of "$ROOT/$rel")" "$rel"
      else
        printf 'AUSENTE  %s\n' "$rel"
      fi
    done
  } >>"$LOGS/stage2.log"

  if [ -z "$APK" ]; then
    echo "APK nao fornecido — etapas 2-4 de extracao serao PULADAS." >>"$LOGS/stage2.log"
    record_stage "2-apk" "PULADO" "APK nao fornecido"
    return 2
  fi
  if [ ! -f "$APK" ]; then
    echo "APK nao encontrado: $APK" >>"$LOGS/stage2.log"
    record_stage "2-apk" "FALHOU" "APK nao encontrado"
    return 1
  fi

  local actual
  actual="$(sha256_of "$APK")"
  {
    echo "apk=$APK"
    echo "tamanho=$(wc -c <"$APK" | tr -d ' ')"
    echo "sha256=$actual"
    echo "esperado=$EXPECTED_APK_SHA256"
  } >>"$LOGS/stage2.log"

  # integridade ZIP do APK
  if ! python3 - "$APK" >>"$LOGS/stage2.log" 2>&1 <<'PY'
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
bad = z.testzip()
print("entradas=%d" % len(z.namelist()))
print("zip_testzip=%s" % ("OK" if bad is None else "CORROMPIDO:" + str(bad)))
sys.exit(0 if bad is None else 1)
PY
  then
    record_stage "2-apk" "FALHOU" "zip integridade"
    return 1
  fi

  if [ "$actual" = "$EXPECTED_APK_SHA256" ]; then
    record_stage "2-apk" "OK" "hash e integridade conferem"
    return 0
  fi
  echo "AVISO: hash do APK difere do fingerprint conhecido." >>"$LOGS/stage2.log"
  record_stage "2-apk" "OK" "integro; hash difere do esperado (aviso)"
  return 0
}

# ---------------------------------------------------------------------------
# etapa 3: extração de bibliotecas e recursos
#   3a)NxExtract (recipe) -> game/s3e + game/assets + hook de unpack
#   3b)extração das libs nativas do APK -> libs/ (achatadas, sem subpastas)
# ---------------------------------------------------------------------------
stage3_extract() {
  : >"$LOGS/stage3.log"
  if [ -z "$APK" ] || [ ! -f "$APK" ]; then
    echo "APK indisponivel — extracao pulada." >>"$LOGS/stage3.log"
    record_stage "3-extract" "PULADO" "APK indisponivel"
    return 2
  fi

  local gdir="$WORK/install"
  mkdir -p "$gdir/gamedata" "$gdir/nxextract"
  # Preserva o APK já presente em gamedata/ entre execuções locais; copia
  # somente quando ausente ou quando o APK informado tem tamanho diferente.
  local apk_bytes
  apk_bytes="$(wc -c <"$APK" | tr -d ' ')"
  if [ ! -f "$gdir/gamedata/game.apk" ] || \
     [ "$(wc -c <"$gdir/gamedata/game.apk" | tr -d ' ')" != "$apk_bytes" ]; then
    cp -f "$APK" "$gdir/gamedata/game.apk"
    log "APK copiado -> $gdir/gamedata/game.apk ($apk_bytes bytes)"
  else
    log "APK preservado em $gdir/gamedata/game.apk (nao recopiado)"
  fi
  # NxExtract exige a receita como arquivo regular dentro do game dir; o hook
  # resolve {recipe_dir}/nxextract/unpack-s3e.py, então ambos vão ao workspace.
  cp -f "$ROOT/extractor.json" "$gdir/extractor.json"
  cp -f "$ROOT/nxextract/unpack-s3e.py" "$gdir/nxextract/unpack-s3e.py"

  echo "-- NxExtract (recipe extractor.json) --" >>"$LOGS/stage3.log"
  # NXEXTRACT_PYTHON garante que usemos o python3 do ambiente (pode ser3.14 no host).
  if ! NXEXTRACT_GAME_DIR="$gdir" NXEXTRACT_PYTHON="${PYTHON_BIN:-python3}" \
       bash "$ROOT/nxextract/run-extractor.sh" >>"$LOGS/stage3.log" 2>&1; then
    record_stage "3-extract" "FALHOU" "NxExtract install"
    return 1
  fi

  # Extração das libs nativas do APK para libs/ (achatadas, sem subpastas).
  # Proteções: os stubs glibc do port nunca são sobrescritos, e é recusado
  # qualquer nome que colida com uma lib que o loader carrega em tempo de
  # execução (libs/ está no LD_LIBRARY_PATH, ver The Sims 3.sh).
  echo "-- extracao libs nativas do APK -> libs/ (sem subpastas) --" >>"$LOGS/stage3.log"
  if ! python3 - "$APK" "$ROOT/libs" >>"$LOGS/stage3.log" 2>&1 <<'PY'
import hashlib, os, struct, sys, zipfile

def elf_desc(path):
    with open(path, "rb") as fh:
        head = fh.read(20)
    if head[:4] != b"\x7fELF":
        return "nao-ELF"
    cls = {1: "ELF32", 2: "ELF64"}.get(head[4], "ELF?")
    endian = head[5]
    machine = struct.unpack("<H" if endian == 1 else ">H", head[18:20])[0]
    m = {40: "ARM", 3: "x86", 62: "x86-64", 183: "AArch64"}.get(machine, str(machine))
    return "%s/%s" % (cls, m)

def sha256(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest().upper()

# Stubs glibc do port (LD_PRELOAD): nunca podem ser substituidos por uma
# lib Android/Bionic extraida do APK.
PROTECTED = {"libs3eAndroidJNI.so", "libs3eVFS.so"}

# Nomes que o loader carrega em tempo de execucao via dlopen. Como libs/ esta
# no LD_LIBRARY_PATH, um .so Android com um desses nomes sombrearia o do
# sistema e quebraria a carga de EGL/GLES/SDL/zlib.
LOADER_DLOPEN = {
    "libEGL.so", "libEGL.so.1",
    "libGLESv1_CM.so", "libGLESv1_CM.so.1",
    "libGLESv2.so", "libGLESv2.so.2",
    "libSDL2.so", "libSDL2-2.0.so.0",
    "libSDL2_mixer.so", "libSDL2_mixer-2.0.so.0",
    "libz.so", "libz.so.1",
    "libmali.so",
}

apk, out_dir = sys.argv[1], sys.argv[2]
os.makedirs(out_dir, exist_ok=True)
z = zipfile.ZipFile(apk)
# Flatten: o destino final e sempre o basename, sem diretorios de ABI.
libs = sorted(n for n in z.namelist()
              if n.startswith("lib/") and n.endswith(".so"))
print("libs nativas no APK: %d" % len(libs))
if not libs:
    print("AVISO: APK sem bibliotecas em lib/; nada a extrair")

blocked = []
for name in libs:
    base = os.path.basename(name)
    if base in PROTECTED:
        blocked.append((name, "sobrescreveria o stub glibc do port"))
        continue
    if base in LOADER_DLOPEN:
        blocked.append((name, "colide com lib carregada pelo loader via dlopen"))
        continue
    data = z.read(name)
    dest = os.path.join(out_dir, base)
    with open(dest, "wb") as fh:
        fh.write(data)
    print("  EXTRAIDA %-38s -> libs/%-24s bytes=%-9d formato=%s sha256=%s"
          % (name, base, len(data), elf_desc(dest), sha256(dest)))

print("libs do port (glibc, LD_PRELOAD) - nunca substituir pelas Android:")
for name in sorted(os.listdir(out_dir)):
    p = os.path.join(out_dir, name)
    if os.path.isfile(p) and name.endswith(".so"):
        kind = "STUB" if name in PROTECTED else "APK "
        print("  %s    %-38s bytes=%-9d formato=%s sha256=%s"
              % (kind, name, os.path.getsize(p), elf_desc(p), sha256(p)))

for name, why in blocked:
    print("  BLOQUEADA %-38s %s" % (name, why))
if blocked:
    raise SystemExit("ERRO: %d lib(s) do APK recusada(s) em libs/" % len(blocked))
PY
  then
    record_stage "3-extract" "FALHOU" "extracao de libs do APK"
    return 1
  fi

  # valida saidas do recipe
  local s3e="$gdir/game/s3e" assets="$gdir/game/assets"
  if [ ! -f "$s3e" ]; then
    echo "game/s3e ausente" >>"$LOGS/stage3.log"
    record_stage "3-extract" "FALHOU" "game/s3e ausente"
    return 1
  fi
  local n_assets
  n_assets="$(find "$assets" -type f 2>/dev/null | wc -l | tr -d ' ')"
  echo "game/s3e bytes=$(wc -c <"$s3e" | tr -d ' ') sha256=$(sha256_of "$s3e")" >>"$LOGS/stage3.log"
  echo "game/assets arquivos=$n_assets" >>"$LOGS/stage3.log"
  if [ "$n_assets" -lt 1 ]; then
    record_stage "3-extract" "FALHOU" "game/assets vazio"
    return 1
  fi
  EXTRACT_OK=1
  record_stage "3-extract" "OK" "s3e + $n_assets assets + libs do APK em libs/"
  return 0
}

# ---------------------------------------------------------------------------
# etapa 4: descompactação e validação do .s3e (XE3U)
# ---------------------------------------------------------------------------
stage4_unpack() {
  : >"$LOGS/stage4.log"
  local gdir="$WORK/install"
  local s3e="$gdir/game/s3e"
  local out="$gdir/game/game.s3e.unpacked"
  if [ -z "$APK" ] || [ ! -f "$s3e" ]; then
    echo "entrada game/s3e indisponivel — pulada." >>"$LOGS/stage4.log"
    record_stage "4-unpack" "PULADO" "sem game/s3e"
    return 2
  fi

  # (o hook do recipe já rodou; aqui validamos explicitamente de forma idempotente)
  if ! python3 "$ROOT/nxextract/unpack-s3e.py" "$s3e" "$out" \
       --loader "$gdir/sims3_s3e_loader" >>"$LOGS/stage4.log" 2>&1; then
    record_stage "4-unpack" "FALHOU" "decoder"
    return 1
  fi

  python3 - "$out" >>"$LOGS/stage4.log" 2>&1 <<'PY'
import sys
p = sys.argv[1]
data = open(p, "rb").read()
magic = data[:4].hex().upper()
ok = magic == "58453355" and len(data) >= 1048576
print("bytes=%d magic=%s (XE3U esperado=58453355) valido=%s" % (len(data), magic, ok))
sys.exit(0 if ok else 1)
PY
  local rc=$?
  if [ "$rc" -eq 0 ]; then
    record_stage "4-unpack" "OK" "XE3U validado"
    return 0
  fi
  record_stage "4-unpack" "FALHOU" "XE3U invalido"
  return 1
}

# ---------------------------------------------------------------------------
# etapa 5: preparação da estrutura do jogo
# ---------------------------------------------------------------------------
stage5_structure() {
  : >"$LOGS/stage5.log"
  local gdir="$WORK/install/game"
  local required=("game.s3e.unpacked" "assets" "s3e")
  local missing=()
  for rel in "${required[@]}"; do
    [ -e "$gdir/$rel" ] || missing+=("$rel")
  done
  {
    echo "estrutura esperada em $gdir:"
    for rel in "${required[@]}"; do
      if [ -e "$gdir/$rel" ]; then echo "  OK   $rel"; else echo "  FALTA $rel"; fi
    done
  } >>"$LOGS/stage5.log"

  if [ "$EXTRACT_OK" -eq 0 ]; then
    record_stage "5-structure" "PULADO" "extracao nao executada"
    return 2
  fi
  if [ ${#missing[@]} -ne 0 ]; then
    record_stage "5-structure" "FALHOU" "faltam: ${missing[*]}"
    return 1
  fi
  record_stage "5-structure" "OK" "game/ completa"
  return 0
}

# ---------------------------------------------------------------------------
# etapa 6: compilação do loader (SDK) + verificação estática
# ---------------------------------------------------------------------------
stage6_build() {
  : >"$LOGS/stage6.log"
  echo "-- build via scripts/build-loader-nextos.sh (SDK=$SDK_IMAGE) --" >>"$LOGS/stage6.log"
  if ! NEXTOS_SDK_IMAGE="$SDK_IMAGE" bash "$ROOT/scripts/build-loader-nextos.sh" \
       >>"$LOGS/stage6.log" 2>&1; then
    record_stage "6-build" "FALHOU" "build-loader-nextos.sh"
    return 1
  fi

  local bin="$ROOT/build/nextos/sims3_s3e_loader"
  if [ ! -f "$bin" ]; then
    echo "binario de build ausente: $bin" >>"$LOGS/stage6.log"
    record_stage "6-build" "FALHOU" "binario nao gerado"
    return 1
  fi

  # verificações estáticas: unpack, ELF, ABI, glibc
  if ! python3 - "$bin" "$ROOT/port.json" >>"$LOGS/stage6.log" 2>&1 <<'PY'
import json, re, subprocess, sys
bin_path, port_path = sys.argv[1], sys.argv[2]
raw = open(bin_path, "rb").read()

# 1) opção de descompactação existe no loader GERADO?
has_unpack = b"--unpack-s3e" in raw
print("has_unpack_flag=%s" % has_unpack)

# 2) ELF32 ARM + interpreter + hard-float
def run(*args):
    return subprocess.run(args, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True).stdout

hdr = run("readelf", "-h", bin_path)
prog = run("readelf", "-l", bin_path)
abi = run("readelf", "-A", bin_path)
elf32 = "Class:" in hdr and "ELF32" in hdr
arm = "Machine:" in hdr and "ARM" in hdr
interp = "/lib/ld-linux-armhf.so.3" in prog
hardfloat = "Tag_ABI_VFP_args: VFP registers" in abi
print("elf32=%s arm=%s interp_armhf=%s hardfloat=%s" % (elf32, arm, interp, hardfloat))

# 3) glibc máximo exigido vs port.json min_glibc
vers = set(re.findall(rb"GLIBC_([0-9]+\.[0-9]+)", raw))
vers = {tuple(int(x) for x in v.split(b".")) for v in vers}
maxv = max(vers) if vers else (0, 0)
min_glibc = tuple(int(x) for x in
                  json.load(open(port_path))["attr"]["min_glibc"].split("."))
maxv_s = ".".join(map(str, maxv))
ok_glibc = maxv <= min_glibc
print("glibc_max=%s port_min=%s compativel=%s" % (maxv_s, ".".join(map(str, min_glibc)), ok_glibc))

ok = has_unpack and elf32 and arm and interp and hardfloat and ok_glibc
sys.exit(0 if ok else 1)
PY
  then
    record_stage "6-build" "FALHOU" "verificacao estatica do loader"
    return 1
  fi

  echo "sha256 build=$(sha256_of "$bin")" >>"$LOGS/stage6.log"
  echo "sha256 rastreado(root)=$(sha256_of "$ROOT/sims3_s3e_loader" 2>/dev/null || echo n/a)" >>"$LOGS/stage6.log"
  echo "nota: build novo difere do binario rastreado (fonte loader/ evoluiu);" >>"$LOGS/stage6.log"
  echo "      binarios rastreados PRESERVADOS; o pacote envia o build novo." >>"$LOGS/stage6.log"
  BUILD_OK=1
  record_stage "6-build" "OK" "loader compilado e verificado"
  return 0
}

# ---------------------------------------------------------------------------
# etapa 7: verificação do launcher e do port.json
# ---------------------------------------------------------------------------
stage7_verify() {
  : >"$LOGS/stage7.log"
  local errs=()

  if ! bash -n "$ROOT/The Sims 3.sh" >>"$LOGS/stage7.log" 2>&1; then
    errs+=("sintaxe do launcher")
  fi
  for json in port.json extractor.json; do
    if ! json_valid "$ROOT/$json" >>"$LOGS/stage7.log" 2>&1; then
      errs+=("json invalido: $json")
    fi
  done

  # todos os itens do port.json existem?
  if ! python3 - "$ROOT" >>"$LOGS/stage7.log" 2>&1 <<'PY'
import json, os, sys
root = sys.argv[1]
port = json.load(open(os.path.join(root, "port.json"), encoding="utf-8"))
missing = []
for item in port.get("items", []):
    p = os.path.join(root, item.rstrip("/"))
    if not os.path.exists(p):
        missing.append(item)
# items_opt are optional by definition: report absence but never fail on them.
optional = [i for i in port.get("items_opt", [])
            if not os.path.exists(os.path.join(root, i.rstrip("/")))]
print("itens obrigatorios port.json=%d ausentes=%s"
      % (len(port.get("items", [])), missing or "nenhum"))
print("itens opcionais ausentes (permitido): %s" % (optional or "nenhum"))
# itens removidos/devem sumir
forbidden = ["hooks/", "libs.armhf/"]
present = [f for f in forbidden
           if os.path.exists(os.path.join(root, f.rstrip("/")))]
print("itens que deveriam ter sido removidos ainda presentes: %s"
      % (present or "nenhum"))
sys.exit(1 if (missing or present) else 0)
PY
  then
    errs+=("port.json itens")
  fi

  # launcher não deve mais referenciar libs.armhf do GAMEDIR
  if grep -q 'GAMEDIR/libs\.armhf\|GAMEDIR/libs\.aarch64' "$ROOT/The Sims 3.sh"; then
    errs+=("launcher ainda referencia GAMEDIR/libs.armhf|aarch64")
  fi
  # hook do recipe aponta para a ferramenta migrada
  if ! grep -q 'nxextract/unpack-s3e.py' "$ROOT/extractor.json"; then
    errs+=("extractor.json nao referencia nxextract/unpack-s3e.py")
  fi
  if [ -d "$ROOT/hooks" ]; then
    errs+=("diretorio hooks/ ainda existe")
  fi

  # libs/ do port: precisam existir, ser ELF32 ARM hard-float e NAO interpor
  # a libdl. A versao anterior exportava dlopen/dlsym/dlclose e o dlsym chamava
  # a si mesmo ("bl <dlsym>" com RTLD_NEXT antes do ponteiro original estar
  # resolvido) -> recursao infinita na primeira chamada do loader. Ver
  # sims3-stubs.c (fonte corrigida) e build/libs-backup/ (versao anterior).
  local stub sym
  for stub in libs3eAndroidJNI.so libs3eVFS.so; do
    if [ ! -s "$ROOT/libs/$stub" ]; then
      errs+=("libs/$stub ausente ou vazio")
      continue
    fi
    if ! have readelf; then
      echo "AVISO: readelf ausente; libs/$stub nao validado" >>"$LOGS/stage7.log"
      continue
    fi
    local hdr attrs defs
    hdr="$(readelf -h "$ROOT/libs/$stub" 2>/dev/null)"
    attrs="$(readelf -A "$ROOT/libs/$stub" 2>/dev/null)"
    # -W: sem ele o readelf trunca nomes longos (s3eAndroidJNIIni[...]) e a
    # comparacao abaixo vira falso positivo.
    defs="$(readelf -W --dyn-syms "$ROOT/libs/$stub" 2>/dev/null \
      | awk '($4=="FUNC"||$4=="OBJECT") && $7!="UND" {n=$8; sub(/@.*/,"",n); if(n!="") print n}' \
      | sort -u)"
    echo "$hdr" | grep -q 'Class:.*ELF32' \
      || errs+=("libs/$stub nao e ELF32")
    echo "$hdr" | grep -Eq 'hard-float ABI' \
      || errs+=("libs/$stub nao e hard-float")
    echo "$attrs" | grep -q 'Tag_ABI_VFP_args: VFP registers' \
      || errs+=("libs/$stub nao usa registradores VFP para args FP")
    for sym in dlopen dlsym dlclose; do
      if printf '%s\n' "$defs" | grep -qx "$sym"; then
        errs+=("libs/$stub interpoe $sym (quebra o loader)")
      fi
    done
    for sym in s3eAndroidJNIInitialize s3eAndroidJNITerminate s3eVFSInit \
               s3eVFSTerm s3eGetSystemProperty s3eDeviceYield; do
      printf '%s\n' "$defs" | grep -qx "$sym" \
        || errs+=("libs/$stub nao exporta $sym")
    done
    echo "libs/$stub: $(wc -c <"$ROOT/libs/$stub" | tr -d ' ') bytes sha256=$(sha256sum "$ROOT/libs/$stub" | cut -c1-16)" \
      >>"$LOGS/stage7.log"
    echo "libs/$stub exports: $(printf '%s ' $defs)" >>"$LOGS/stage7.log"
  done

  if [ ${#errs[@]} -ne 0 ]; then
    echo "erros: ${errs[*]}" >>"$LOGS/stage7.log"
    record_stage "7-verify" "FALHOU" "${errs[*]}"
    return 1
  fi
  record_stage "7-verify" "OK" "launcher/port/extractor consistentes"
  return 0
}

# ---------------------------------------------------------------------------
# etapa 8: empacotamento + validação de integridade
# ---------------------------------------------------------------------------
stage8_package() {
  : >"$LOGS/stage8.log"
  if ! bash "$ROOT/scripts/package-nextos-port.sh" >>"$LOGS/stage8.log" 2>&1; then
    record_stage "8-package" "FALHOU" "package-nextos-port.sh"
    return 1
  fi

  local zip="$ROOT/build/nextos/sims3.zip"
  if [ ! -f "$zip" ]; then
    record_stage "8-package" "FALHOU" "sims3.zip ausente"
    return 1
  fi

  if ! python3 - "$zip" "$ROOT/extractor.json" >>"$LOGS/stage8.log" 2>&1 <<'PY'
import json, sys, zipfile
zpath, extractor = sys.argv[1], sys.argv[2]
z = zipfile.ZipFile(zpath)
bad = z.testzip()
names = z.namelist()
print("zip_integridade=%s arquivos=%d" % ("OK" if bad is None else "CORROMPIDO:"+str(bad), len(names)))
need = ["sims3/The Sims 3.sh", "sims3/port.json", "sims3/extractor.json",
        "sims3/sims3_s3e_loader"]
missing = [n for n in need if n not in names]
print("itens obrigatorios ausentes: %s" % (missing or "nenhum"))
hooks = [n for n in names if "/hooks/" in n]
print("residuos hooks/ no zip: %s" % (hooks or "nenhum"))
libs = [n for n in names if n.startswith("sims3/libs/")]
print("libs/ no zip: %s" % (libs or "nenhum"))
old = [n for n in names if "libs.armhf" in n]
print("residuos libs.armhf no zip: %s" % (old or "nenhum"))
residues = [n for n in names
            if "__pycache__" in n or n.endswith((".pyc", ".pyo"))]
print("residuos python (pycache/pyc) no zip: %s" % (residues or "nenhum"))
datafiles = [n for n in names if n.lower().endswith((".apk", ".obb", ".log"))]
print("dados/logs (apk/obb/log) no zip: %s" % (datafiles or "nenhum"))
sys.exit(0 if (bad is None and not missing and not hooks and not old
               and not residues and not datafiles and libs) else 1)
PY
  then
    record_stage "8-package" "FALHOU" "validacao do zip"
    return 1
  fi
  record_stage "8-package" "OK" "sims3.zip integro e consistente"
  return 0
}

# ---------------------------------------------------------------------------
# etapa 9: relatório final
# ---------------------------------------------------------------------------
stage9_report() {
  mkdir -p "$(dirname "$REPORT")"
  local pass=0 fail=0 skip=0 i
  for i in "${!STAGE_STATES[@]}"; do
    case "${STAGE_STATES[$i]}" in
      OK) pass=$((pass + 1)) ;;
      FALHOU) fail=$((fail + 1)) ;;
      PULADO) skip=$((skip + 1)) ;;
    esac
  done

  {
    echo "=================================================================="
    echo " The Sims 3 — Relatorio de reconstrucao"
    echo " data: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    echo " repo: $ROOT"
    echo "=================================================================="
    echo
    echo "RESUMO: $pass OK · $fail FALHOU · $skip PULADO"
    echo
    printf '%-16s %-8s %s\n' "ETAPA" "ESTADO" "DETALHE"
    printf '%-16s %-8s %s\n' "------" "-------" "-------"
    for i in "${!STAGE_NAMES[@]}"; do
      printf '%-16s %-8s %s\n' "${STAGE_NAMES[$i]}" "${STAGE_STATES[$i]}" "${STAGE_NOTES[$i]}"
    done
    echo
    echo "SUCESSOS:"
    for i in "${!STAGE_NAMES[@]}"; do
      [ "${STAGE_STATES[$i]}" = "OK" ] && echo "  [OK] ${STAGE_NAMES[$i]} — ${STAGE_NOTES[$i]}"
    done
    echo
    echo "FALHAS:"
    local any_fail=0
    for i in "${!STAGE_NAMES[@]}"; do
      if [ "${STAGE_STATES[$i]}" = "FALHOU" ]; then
        any_fail=1; echo "  [FALHOU] ${STAGE_NAMES[$i]} — ${STAGE_NOTES[$i]}"
      fi
    done
    [ "$any_fail" -eq 0 ] && echo "  (nenhuma)"
    echo
    echo "PENDENCIAS (nao resolvidas por esta rodada):"
    echo "  - OBB/res.dz: o APK nao contem LowRes/HighRes/res.dz; um OBB externo"
    echo "    provavelmente e necessario. Nenhum OBB foi encontrado nem exigido"
    echo "    aqui; se o jogo pedir esses assets no dispositivo, forneca o OBB."
    echo "  - Runtime: NAO TESTADO — nenhum binario ARM foi executado nesta"
    echo "    rodada (sem qemu/dispositivo). Validacao real pendente."
    echo "  - Binarios rastreados preservados; a divergencia fonte(loader/) vs"
    echo "    binario rastreado esta documentada; o pacote envia o build novo."
    echo
    echo "LOGS por etapa: build/reconstruct/logs/stage*.log"
    echo "=================================================================="
  } >"$REPORT"

  log "relatorio: $REPORT"
  log "RESUMO: $pass OK · $fail FALHOU · $skip PULADO"
  [ "$fail" -eq 0 ]
}

# ---------------------------------------------------------------------------
# execução
# ---------------------------------------------------------------------------
main() {
  local input="${1:-${SIMS3_APK:-}}"
  if [ -n "$input" ]; then APK="$(normalize_path "$input")"; fi

  mkdir -p "$LOGS" "$BACKUP"
  # Limpa apenas as SAÍDAS de execução anterior, preservando gamedata/ (APK) e
  # os backups: idempotente e não destrutivo. build/ nunca é apagado por inteiro.
  if [ -d "$WORK/install" ]; then
    find "$WORK/install" -mindepth 1 -maxdepth 1 ! -name gamedata \
      -exec rm -rf {} + 2>/dev/null || true
  fi
  rm -rf "$WORK/apk-libs"

  log "root=$ROOT"
  log "apk=${APK:-<nao fornecido>}"

  # cada etapa roda mesmo se a anterior falhou/pulou (relatório completo);
  # o rc agregado é decido no relatório final.
  stage1_prechecks || true
  stage2_verify_apk || true
  stage3_extract    || true
  stage4_unpack     || true
  stage5_structure  || true
  stage6_build      || true
  stage7_verify     || true
  stage8_package    || true
  stage9_report
}

main "$@"
