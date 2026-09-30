#!/usr/bin/env python3
import subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
engine=root/"gatito-extract-v3.py"
recipe=root.parent/"extractor.json"
game=root.parent
cmd=[sys.executable,str(engine),str(recipe),"--game-dir",str(game)]
print("\033[2J\033[HGATITO EXTRACTOR — The Sims 3\n")
p=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
for line in p.stdout:
 line=line.rstrip()
 if line.startswith("GATITO_STAGE|"):
  _,pct,msg=line.split("|",2)
  n=int(int(pct)*36/100)
  print("\033[2J\033[HGATITO EXTRACTOR — The Sims 3\n\n["+"█"*n+"░"*(36-n)+f"] {pct}%\n\n{msg}",flush=True)
 else:
  print(line,flush=True)
raise SystemExit(p.wait())
