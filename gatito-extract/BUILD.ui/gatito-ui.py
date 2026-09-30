#!/usr/bin/env python3
"""Controller-friendly UI for Gatito Extractor."""
import argparse, subprocess, sys
from pathlib import Path
BAR = 36
def draw(pct, stage):
    sys.stdout.write("\033[2J\033[H")
    print("+" + "-" * 60 + "+")
    print("|" + "GATITO EXTRACTOR".center(60) + "|")
    print("|" + "".center(60) + "|")
    n = max(0, min(BAR, int(BAR * pct / 100)))
    print("|" + ("[" + "█" * n + "░" * (BAR-n) + "] %3d%%" % pct).center(60) + "|")
    print("|" + stage[:60].center(60) + "|")
    print("+" + "-" * 60 + "+")
    sys.stdout.flush()
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("recipe")
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--abi")
    ns = ap.parse_args()
    root = Path(__file__).resolve().parents[1]
    engine = root / "gatito-extract-v2.py"
    cmd = [sys.executable, str(engine), ns.recipe, "--game-dir", ns.game_dir, "--validation-delay", "0.15"]
    if ns.abi: cmd += ["--abi", ns.abi]
    draw(0, "Iniciando — procurando pacote")
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
    for line in p.stdout:
        line = line.rstrip()
        if line.startswith("GATITO_STAGE|"):
            _, pct, msg = line.split("|", 2); draw(int(pct), msg)
        elif line.startswith("GATITO_XZ|"):
            draw(75, line.split("|", 1)[1])
        elif line: print(line, flush=True)
    rc = p.wait()
    draw(100, "PARAR — todas as rotas falharam" if rc else "OK — extração concluída e validada")
    return rc
if __name__ == "__main__": raise SystemExit(main())