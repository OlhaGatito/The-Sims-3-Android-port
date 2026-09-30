#!/usr/bin/env python3
"""Small controller-friendly Gatito Extractor UI for an existing port extractor."""
import os, shutil, subprocess, sys, time
from pathlib import Path

BAR=36
def clear():
    sys.stdout.write("\033[2J\033[H")
def render(title, pct, stage, detail=""):
    clear()
    w=60
    print("+"+"-"*w+"+")
    print("|"+"GATITO EXTRACTOR".center(w)+"|")
    print("|"+title.center(w)+"|")
    print("|"+"".center(w)+"|")
    filled=max(0,min(BAR,int(BAR*pct/100)))
    bar="█"*filled+"░"*(BAR-filled)
    print("|"+("["+bar+"] %3d%%"%pct).center(w)+"|")
    print("|"+stage.center(w)+"|")
    if detail:
        print("|"+detail[:w].center(w)+"|")
    print("+"+"-"*w+"+")
    sys.stdout.flush()

def check_file(path, min_size=1, magic=None):
    p=Path(path)
    if not p.is_file() or p.stat().st_size < min_size:
        return False
    if magic:
        with p.open("rb") as f:
            if f.read(len(magic)) != magic:
                return False
    return True

def main():
    game=Path(os.environ.get("GAMEDIR","." )).resolve()
    log=game/"logs"/"gatito-extract.log"
    log.parent.mkdir(parents=True,exist_ok=True)
    if not (game/"gamedata").is_dir():
        render("The Sims 3",0,"Falha","gamedata/ não encontrado")
        return 72

    render("The Sims 3",5,"Preparando","Dados do usuário detectados")
    time.sleep(.35)
    render("The Sims 3",12,"Validação inicial","Verificando entrada Android")
    time.sleep(.35)

    cmd=[sys.executable, str(game/"gatito-extract"/"run-backend.py")]
    with log.open("a",encoding="utf-8") as lf:
        lf.write("\n=== Gatito Extractor / Sims 3 ===\n")
        proc=subprocess.Popen(cmd,stdout=lf,stderr=subprocess.STDOUT,cwd=game)

    render("The Sims 3",25,"Extraindo dados","NXExtract em execução")
    rc=proc.wait()

    if rc:
        render("The Sims 3",100,"Extração interrompida",f"código {rc}")
        return rc

    render("The Sims 3",82,"Validando game image","Verificando game.s3e.unpacked")
    time.sleep(.35)
    if not check_file(game/"game/game.s3e.unpacked",1048576,b"XE3U"):
        render("The Sims 3",100,"Falha de validação","game.s3e.unpacked inválido/ausente")
        return 73

    render("The Sims 3",90,"Validando assets","Verificando game/assets")
    time.sleep(.35)
    assets=game/"game/assets"
    if not assets.is_dir() or not any(assets.iterdir()):
        render("The Sims 3",100,"Falha de validação","Assets ausentes ou vazios")
        return 74

    render("The Sims 3",97,"Validação final","Payload confirmado")
    time.sleep(.5)
    render("The Sims 3",100,"Pronto","Dados extraídos e validados")
    time.sleep(.5)
    return 0

if __name__=="__main__":
    raise SystemExit(main())
