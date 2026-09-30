#!/usr/bin/env python3
import argparse, hashlib, json, os, shutil, subprocess, tempfile, time, zipfile
from pathlib import Path, PurePosixPath

VERSION = "0.3.0"

class ExtractError(Exception):
    pass

def safe(p):
    x = PurePosixPath(str(p))
    if x.is_absolute() or ".." in x.parts:
        raise ExtractError("unsafe path: " + str(p))
    return Path(*x.parts)

def sha(p):
    h = hashlib.sha256()
    with p.open("rb") as f:
        for b in iter(lambda: f.read(1048576), b""):
            h.update(b)
    return h.hexdigest()

def emit(n, msg):
    print(f"GATITO_STAGE|{max(0,min(100,int(n)))}|{msg}", flush=True)

def zips(p):
    try:
        return zipfile.ZipFile(p)
    except (zipfile.BadZipFile, OSError):
        return None

def find(paths, patterns):
    for src in paths:
        z = zips(src)
        if z:
            try:
                for pat in patterns:
                    for name in [x for x in z.namelist() if not x.endswith("/")]:
                        if PurePosixPath(name).match(pat):
                            return src, name
            finally:
                z.close()
        for pat in patterns:
            if src.name == pat or PurePosixPath(src.name).match(pat):
                return src, ""
    raise ExtractError("source not found: " + ",".join(patterns))

def copyone(src, name, dst):
    dst.parent.mkdir(parents=True, exist_ok=True)
    if name:
        with zipfile.ZipFile(src) as z, z.open(name) as i, dst.open("wb") as o:
            shutil.copyfileobj(i, o)
    else:
        shutil.copy2(src, dst)

def validate(root, r):
    p = root / safe(r["path"])
    typ = r.get("type", "file")
    if typ == "dir":
        if not p.is_dir() or not any(p.iterdir()):
            raise ExtractError("missing/empty: " + r["path"])
        return
    if not p.is_file() or p.stat().st_size == 0:
        raise ExtractError("missing/empty: " + r["path"])
    if "min_size" in r and p.stat().st_size < int(r["min_size"]):
        raise ExtractError("size below minimum: " + r["path"])
    if "size" in r and p.stat().st_size != int(r["size"]):
        raise ExtractError("size mismatch: " + r["path"])
    if "magic_hex" in r:
        magic = bytes.fromhex(r["magic_hex"])
        with p.open("rb") as f:
            if f.read(len(magic)) != magic:
                raise ExtractError("magic mismatch: " + r["path"])
    if "sha256" in r:
        allowed = r["sha256"] if isinstance(r["sha256"], list) else [r["sha256"]]
        if sha(p) not in allowed:
            raise ExtractError("sha256 mismatch: " + r["path"])

def render(v, recipe, game, stage):
    return str(v).replace("{recipe_dir}", str(recipe.parent)).replace(
        "{game_dir}", str(game)).replace("{stage}", str(stage))

def run_command(argv, cwd, env, label):
    p = subprocess.Popen(argv, cwd=cwd, env=env, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True, bufsize=1)
    for line in p.stdout:
        line = line.rstrip()
        if line:
            emit(74, f"{label}: {line[:180]}")
    return p.wait()

def checkpoints(stage, checks):
    for cp in checks:
        validate(stage, cp)

def run_hook(recipe, game, stage, hook, base):
    env = os.environ.copy()
    env["NXEXTRACT_STAGE"] = str(stage)
    env["NXEXTRACT_GAME_DIR"] = str(game)
    env["GATITO_STAGE_DIR"] = str(stage)

    attempts = [("principal", hook)] + [
        (x.get("id", f"fallback-{i+1}"), x)
        for i, x in enumerate(hook.get("fallbacks", []))
    ]
    last = "nenhuma tentativa executada"

    for idx, (label, spec) in enumerate(attempts):
        argv = [render(x, recipe, game, stage) for x in spec.get("argv", [])]
        if not argv:
            last = f"{label}: comando vazio"
            emit(base, f"ERRO: {last}")
            continue

        emit(base + min(idx * 3, 12),
             f"Tentativa {idx+1}/{len(attempts)}: {spec.get('title', label)}")

        try:
            rc = run_command(argv, game, env, label)
        except Exception as exc:
            rc = 127
            last = f"{label}: {exc}"

        if rc == 0:
            try:
                checkpoints(stage, spec.get("checkpoint", hook.get("checkpoint", [])))
                emit(base + 16,
                     f"OK: {spec.get('title', label)} — checkpoint validado")
                return
            except Exception as exc:
                last = f"{label}: checkpoint: {exc}"
        else:
            last = f"{label}: exit={rc}"

        emit(base + min(idx * 3, 12), f"ERRO: {last}")
        if idx + 1 < len(attempts):
            emit(base + min(idx * 3 + 1, 13), "Tentando rota alternativa...")
            time.sleep(0.15)

    raise ExtractError(
        f"hook failed: {hook.get('id','hook')}; todas as rotas falharam; último erro: {last}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("recipe")
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--input", action="append", default=[])
    ap.add_argument("--abi")
    ap.add_argument("--validation-delay", type=float, default=0.0)
    a = ap.parse_args()

    recipe = Path(a.recipe).resolve()
    r = json.loads(recipe.read_text())
    game = Path(a.game_dir).resolve()
    (game / ".gatito-extract").mkdir(parents=True, exist_ok=True)

    paths = [Path(x).resolve() for x in a.input] if a.input else []
    if not paths:
        for d in r.get("input", {}).get("search_dirs", ["gamedata", "."]):
            q = game / safe(d)
            if q.is_dir():
                paths += [x for x in sorted(q.iterdir()) if x.is_file()]

    if not paths:
        raise ExtractError("no APK/OBB/input found")

    emit(3, "Procurando pacote fornecido pelo usuário")
    emit(5, "Entrada encontrada: " + ", ".join(x.name for x in paths[:4]))

    abi = a.abi or (r.get("abi_order") or [""])[0]
    stage = Path(tempfile.mkdtemp(prefix="stage-", dir=game / ".gatito-extract"))

    try:
        rules = r.get("extract", [])
        total = max(1, len(rules))

        for i, rule in enumerate(rules):
            base = 8 + int(i * 52 / total)
            ident = rule.get("id", rule.get("destination", "item"))
            emit(base, f"Etapa {i+1}/{total}: procurando {ident}")

            src, name = find(paths, rule["source"]["patterns"])
            dst = stage / safe(rule["destination"].replace("{abi}", abi))
            emit(base + 5, f"Fonte encontrada: {src.name}")

            if rule["source"].get("kind") == "entries":
                z = zips(src)
                if not z:
                    raise ExtractError("input is not a ZIP/APK/OBB: " + src.name)
                count = 0
                try:
                    for info in z.infolist():
                        if info.is_dir():
                            continue
                        if not any(PurePosixPath(info.filename).match(p)
                                   for p in rule["source"]["patterns"]):
                            continue
                        rel = info.filename
                        strip = rule["source"].get("strip_prefix", "")
                        if strip and rel.startswith(strip.rstrip("/") + "/"):
                            rel = rel[len(strip.rstrip("/") + "/"):]
                        out = dst / safe(rel)
                        out.parent.mkdir(parents=True, exist_ok=True)
                        with z.open(info) as inp, out.open("wb") as outp:
                            shutil.copyfileobj(inp, outp)
                        count += 1
                finally:
                    z.close()
                if not count:
                    raise ExtractError("no entries matched: " + ident)
            else:
                copyone(src, name, dst)

            check = {
                "path": rule["destination"],
                "type": "dir" if rule["source"].get("kind") == "entries" else "file"
            }
            if check["type"] == "file":
                check["min_size"] = 1
            validate(stage, check)
            emit(base + 18, f"OK: {rule['destination']} validado")
            if a.validation_delay:
                time.sleep(a.validation_delay)

        for hi, hook in enumerate(r.get("hooks", [])):
            base = 68 + min(hi * 8, 10)
            emit(base, "Executando etapa: " + hook.get("id", "hook"))
            run_hook(recipe, game, stage, hook, base)

        for rule in r.get("validate", []):
            validate(stage, rule)

        emit(92, "Todos os artefatos foram validados")

        commit = r.get("commit") or {}
        root = commit.get("root", "game") if isinstance(commit, dict) else "game"
        target = game / safe(root)
        old = target.with_name(target.name + ".gatito-old")

        if old.exists():
            shutil.rmtree(old)
        if target.exists():
            target.rename(old)

        try:
            stage.rename(target)
        except Exception:
            if old.exists():
                old.rename(target)
            raise

        if old.exists():
            shutil.rmtree(old)

        emit(100, "Pronto: dados extraídos e validados")
        return 0
    except Exception:
        shutil.rmtree(stage, ignore_errors=True)
        raise

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as e:
        print("GATITO EXTRACT ERROR: " + str(e), file=os.sys.stderr)
        raise SystemExit(1)
