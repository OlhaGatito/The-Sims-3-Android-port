#!/usr/bin/env python3
"""Controller-friendly Gatito Extractor UI with PortMaster GUI support."""
import argparse, os, subprocess, sys, time
from pathlib import Path
BAR=36
def draw(pct,stage):
    if not sys.stdout.isatty(): return
    sys.stdout.write("\033[2J\033[H")
    print("+"+"-"*60+"+"); print("|"+"GATITO EXTRACTOR".center(60)+"|")
    n=max(0,min(BAR,int(BAR*pct/100)))
    print("|"+("["+"█"*n+"░"*(BAR-n)+"] %3d%%"%pct).center(60)+"|")
    print("|"+stage[:60].center(60)+"|"); print("+"+"-"*60+"+"); sys.stdout.flush()
def find_controlfolder():
    candidates=[os.environ.get("controlfolder"),"/PortMaster","/mnt/mmc/MUOS/PortMaster","/mnt/sdcard/MUOS/PortMaster","/opt/system/Tools/PortMaster","/opt/tools/PortMaster",os.path.join(os.environ.get("XDG_DATA_HOME",""),"PortMaster"),"/roms/ports/PortMaster"]
    for p in candidates:
        if p and os.path.isfile(os.path.join(p,"control.txt")): return p
    return None
CONTROLFOLDER=find_controlfolder()
def pm_message(message):
    if not CONTROLFOLDER: return False
    script='source "$1/control.txt" && pm_message "$2"'
    try:
        return subprocess.run(["bash","-c",script,"gatito-ui",CONTROLFOLDER,message[:240]],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=2).returncode==0
    except Exception: return False
def update(pct,msg):
    if not pm_message(f"Gatito Extractor — {pct}%\n{msg}"): draw(pct,msg)
def main():
    ap=argparse.ArgumentParser(); ap.add_argument("recipe"); ap.add_argument("--game-dir",required=True); ap.add_argument("--abi"); ns=ap.parse_args()
    root=Path(__file__).resolve().parents[1]
    cmd=[sys.executable,str(root/"gatito-extract-v3.py"),ns.recipe,"--game-dir",ns.game_dir,"--validation-delay","0.15"]
    if ns.abi: cmd += ["--abi",ns.abi]
    update(0,"Iniciando — procurando pacote")
    p=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
    for line in p.stdout:
        line=line.rstrip()
        if line.startswith("GATITO_STAGE|"):
            _,pct,msg=line.split("|",2); update(int(pct),msg)
        elif line.startswith("GATITO_XZ|"): update(75,line.split("|",1)[1])
        elif line and not CONTROLFOLDER: print(line,flush=True)
    rc=p.wait(); update(100,"ERRO — extração/validação falhou" if rc else "OK — extração concluída e validada")
    if CONTROLFOLDER: time.sleep(0.8)
    return rc
if __name__=="__main__": raise SystemExit(main())
