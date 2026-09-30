#!/usr/bin/env python3
import argparse, hashlib, json, os, shutil, subprocess, tempfile, zipfile
from pathlib import Path, PurePosixPath

VERSION='0.2.0'
class ExtractError(Exception): pass
def safe(p):
    x=PurePosixPath(p)
    if x.is_absolute() or '..' in x.parts: raise ExtractError('unsafe path')
    return Path(*x.parts)
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1048576),b''): h.update(b)
    return h.hexdigest()
def emit(n,msg): print('GATITO_STAGE|%d|%s'%(max(0,min(100,n)),msg),flush=True)
def zips(p):
    try:
        with zipfile.ZipFile(p) as z: return z
    except zipfile.BadZipFile: return None
def find(paths,patterns):
    for src in paths:
        z=zips(src)
        if z:
            names=[x for x in z.namelist() if not x.endswith('/') ]
            for pat in patterns:
                for name in names:
                    if PurePosixPath(name).match(pat): return src,name
        for pat in patterns:
            if src.name==pat: return src,''
    raise ExtractError('source not found: '+','.join(patterns))
def copyone(src,name,dst):
    dst.parent.mkdir(parents=True,exist_ok=True)
    if name:
        with zipfile.ZipFile(src) as z, z.open(name) as i, dst.open('wb') as o: shutil.copyfileobj(i,o)
    else: shutil.copy2(src,dst)
def validate(root,r):
    p=root/safe(r['path']); typ=r.get('type','file')
    if typ=='dir':
        if not p.is_dir() or not any(p.iterdir()): raise ExtractError('missing/empty: '+r['path'])
        return
    if not p.is_file() or p.stat().st_size==0: raise ExtractError('missing/empty: '+r['path'])
    if 'min_size' in r and p.stat().st_size<int(r['min_size']): raise ExtractError('size: '+r['path'])
    if 'size' in r and p.stat().st_size!=int(r['size']): raise ExtractError('size: '+r['path'])
    if 'magic_hex' in r:
        with p.open('rb') as f:
            if f.read(len(bytes.fromhex(r['magic_hex'])))!=bytes.fromhex(r['magic_hex']): raise ExtractError('magic: '+r['path'])
    if 'sha256' in r:
        allowed=r['sha256'] if isinstance(r['sha256'],list) else [r['sha256']]
        if sha(p) not in allowed: raise ExtractError('sha256: '+r['path'])
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('recipe'); ap.add_argument('--game-dir',required=True); ap.add_argument('--input',action='append',default=[]); ap.add_argument('--abi'); a=ap.parse_args()
    recipe=Path(a.recipe).resolve(); r=json.loads(recipe.read_text()); game=Path(a.game_dir).resolve()
    spec=r.get('input',{}); paths=[Path(x).resolve() for x in a.input] if a.input else []
    if not paths:
        for d in spec.get('search_dirs',['gamedata','.']):
            q=game/safe(d)
            if q.is_dir(): paths += [x for x in sorted(q.iterdir()) if x.is_file()]
    if not paths: raise ExtractError('no APK/OBB/input found')
    emit(5,'Entrada encontrada: '+', '.join(x.name for x in paths[:3]))
    abi=a.abi or (r.get('abi_order') or [''])[0]
    stage=Path(tempfile.mkdtemp(prefix='stage-',dir=game/'.gatito-extract'));
    try:
        rules=r.get('extract',[]); total=max(1,len(rules))
        for i,rule in enumerate(rules):
            base=10+int(i*55/total); emit(base,'Verificando: '+rule.get('id',rule.get('destination','item')))
            src,name=find(paths,rule['source']['patterns']); dst=stage/safe(rule['destination'].replace('{abi}',abi))
            if rule['source'].get('kind')=='entries':
                z=zips(src); count=0
                for info in z.infolist():
                    if info.is_dir() or not any(PurePosixPath(info.filename).match(p) for p in rule['source']['patterns']): continue
                    rel=info.filename; strip=rule['source'].get('strip_prefix','')
                    if strip and rel.startswith(strip.rstrip('/')+'/'): rel=rel[len(strip.rstrip('/')+'/'): ]
                    out=dst/safe(rel); out.parent.mkdir(parents=True,exist_ok=True)
                    with z.open(info) as i,out.open('wb') as o: shutil.copyfileobj(i,o)
                    count+=1
                if not count: raise ExtractError('no entries matched: '+rule.get('id',''))
            else: copyone(src,name,dst)
            emit(base+20,'Extração concluída: '+rule['destination']); validate(stage,{'path':rule['destination'],'type':'dir'} if rule.get('source',{}).get('kind')=='entries' else {'path':rule['destination'],'min_size':1})
            emit(base+25,'Validação OK: '+rule['destination'])
        for hook in r.get('hooks',[]):
            argv=[str(x).replace('{recipe_dir}',str(recipe.parent)).replace('{game_dir}',str(game)) for x in hook.get('argv',[])]
            env=os.environ.copy(); env['NXEXTRACT_STAGE']=str(stage); env['NXEXTRACT_GAME_DIR']=str(game)
            emit(72,'Executando: '+hook.get('id','hook')); rc=subprocess.call(argv,cwd=game,env=env)
            if rc: raise ExtractError('hook failed: '+hook.get('id','hook'))
            for cp in hook.get('checkpoint',[]): validate(stage,cp)
            emit(84,'Checkpoint OK: '+hook.get('id','hook'))
        for rule in r.get('validate',[]): validate(stage,rule)
        emit(92,'Todos os artefatos foram validados')
        target=game/safe((r.get('commit') or {}).get('root','game')); old=target.with_name(target.name+'.gatito-old')
        if old.exists(): shutil.rmtree(old)
        if target.exists(): target.rename(old)
        try: stage.rename(target)
        except Exception:
            if old.exists(): old.rename(target)
            raise
        if old.exists(): shutil.rmtree(old)
        emit(100,'Pronto: dados extraídos e validados')
        return 0
    except Exception:
        shutil.rmtree(stage,ignore_errors=True); raise
if __name__=='__main__':
    try: raise SystemExit(main())
    except Exception as e: print('GATITO EXTRACT ERROR: '+str(e),file=os.sys.stderr); raise SystemExit(1)