#!/usr/bin/env python3
"""Gatito-Extrator graphical test UI - NxExtract pattern.

This UI follows the NxExtract visual style with progress bars, phases, and
error reporting. The extractor remains a Python backend.
"""
import argparse
import ctypes
import ctypes.util
import errno
import fcntl
import glob
import mmap
import os
import platform
import select
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

FBIOGET_VSCREENINFO = 0x4600
INPUT_EVENT = struct.Struct("llHHI")
EV_KEY = 0x01
KEY_RELEASE = 0

# Small 5x7 font. Only characters needed by the diagnostic UI are included;
# unknown characters are rendered as spaces.
FONT = {
    "A":"01110 10001 10001 11111 10001 10001 10001","B":"11110 10001 10001 11110 10001 10001 11110",
    "C":"01111 10000 10000 10000 10000 10000 01111","D":"11110 10001 10001 10001 10001 10001 11110",
    "E":"11111 10000 10000 11110 10000 10000 11111","F":"11111 10000 10000 11110 10000 10000 10000",
    "G":"01111 10000 10000 10111 10001 10001 01111","H":"10001 10001 10001 11111 10001 10001 10001",
    "I":"11111 00100 00100 00100 00100 00100 11111","J":"00111 00010 00010 00010 10010 10010 01100",
    "K":"10001 10010 10100 11000 10100 10010 10001","L":"10000 10000 10000 10000 10000 10000 11111",
    "M":"10001 11011 10101 10101 10001 10001 10001","N":"10001 11001 10101 10011 10001 10001 10001",
    "O":"01110 10001 10001 10001 10001 10001 01110","P":"11110 10001 10001 11110 10000 10000 10000",
    "Q":"01110 10001 10001 10001 10101 10010 01101","R":"11110 10001 10001 11110 10100 10010 10001",
    "S":"01111 10000 10000 01110 00001 00001 11110","T":"11111 00100 00100 00100 00100 00100 00100",
    "U":"10001 10001 10001 10001 10001 10001 01110","V":"10001 10001 10001 10001 10001 01010 00100",
    "W":"10001 10001 10001 10101 10101 10101 01010","X":"10001 10001 01010 00100 01010 10001 10001",
    "Y":"10001 10001 01010 00100 00100 00100 00100","Z":"11111 00001 00010 00100 01000 10000 11111",
    "0":"01110 10001 10011 10101 11001 10001 01110","1":"00100 01100 00100 00100 00100 00100 01110",
    "2":"01110 10001 00001 00010 00100 01000 11111","3":"11110 00001 00001 01110 00001 00001 11110",
    "4":"00010 00110 01010 10010 11111 00010 00010","5":"11111 10000 10000 11110 00001 00001 11110",
    "6":"01110 10000 10000 11110 10001 10001 01110","7":"11111 00001 00010 00100 01000 01000 01000",
    "8":"01110 10001 10001 01110 10001 10001 01110","9":"01110 10001 10001 01111 00001 00001 01110",
    "%":"10001 00010 00100 01000 10001 00000 00000","-":"00000 00000 00000 11111 00000 00000 00000",
    ":":"00000 00100 00000 00000 00000 00100 00000",".":"00000 00000 00000 00000 00000 00110 00110",
    "/":"00001 00010 00100 01000 10000 00000 00000","_":"00000 00000 00000 00000 00000 00000 11111",
    " ":"00000 00000 00000 00000 00000 00000 00000",
}
for k, v in list(FONT.items()):
    FONT[k] = [int(row, 2) for row in v.split()]

class Framebuffer:
    def __init__(self):
        self.fd = os.open("/dev/fb0", os.O_RDWR)
        raw = bytearray(160)
        fcntl.ioctl(self.fd, FBIOGET_VSCREENINFO, raw, True)
        self.xres, self.yres = struct.unpack_from("II", raw, 0)
        self.bits = struct.unpack_from("I", raw, 24)[0]
        self.red = struct.unpack_from("BBBB", raw, 32)
        self.green = struct.unpack_from("BBBB", raw, 36)
        self.blue = struct.unpack_from("BBBB", raw, 40)
        self.line_length = self.xres * max(1, self.bits // 8)
        self.size = self.line_length * self.yres
        self.mm = mmap.mmap(self.fd, self.size, mmap.MAP_SHARED, mmap.PROT_WRITE | mmap.PROT_READ)
    def _pack(self, c):
        r,g,b = c
        if self.bits == 16:
            return struct.pack("<H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
        if self.bits == 32:
            return struct.pack("<I", (r << 16) | (g << 8) | b)
        return bytes((r,g,b))
    def fill(self, c):
        p=self._pack(c); self.mm[:] = p * (self.size // len(p))
    def rect(self,x,y,w,h,c):
        if w<=0 or h<=0: return
        x=max(0,x); y=max(0,y); w=min(w,self.xres-x); h=min(h,self.yres-y)
        p=self._pack(c)
        for yy in range(y,y+h):
            off=yy*self.line_length+x*len(p)
            self.mm[off:off+w*len(p)] = p*w
    def text(self,x,y,s,scale=2,c=(235,240,245)):
        for ch in str(s).upper():
            glyph=FONT.get(ch,FONT[" "])
            for row,bits in enumerate(glyph):
                for col in range(5):
                    if bits & (1 << (4-col)):
                        self.rect(x+col*scale,y+row*scale,scale,scale,c)
            x += 6*scale
    def close(self):
        try: self.mm.close()
        finally: os.close(self.fd)

class SDLUI:
    def __init__(self):
        self.lib=None; self.window=None; self.renderer=None; self.texture=None
        self.enabled=False
        self.start_down=False
        self.select_down=False
        names=["SDL2","libSDL2-2.0.so.0","libSDL2.so"]
        for name in names:
            path=ctypes.util.find_library(name) or name
            try:
                lib=ctypes.CDLL(path)
                if hasattr(lib,"SDL_Init"):
                    self.lib=lib; break
            except OSError: pass
        if not self.lib: return
        L=self.lib
        L.SDL_Init.argtypes=[ctypes.c_uint]; L.SDL_Init.restype=ctypes.c_int
        L.SDL_Quit.argtypes=[]; L.SDL_Quit.restype=None
        L.SDL_GetError.restype=ctypes.c_char_p
        L.SDL_CreateWindow.restype=ctypes.c_void_p
        L.SDL_CreateWindow.argtypes=[ctypes.c_char_p,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_uint]
        L.SDL_CreateRenderer.restype=ctypes.c_void_p
        L.SDL_CreateRenderer.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_uint]
        L.SDL_SetRenderDrawColor.argtypes=[ctypes.c_void_p,ctypes.c_uint8,ctypes.c_uint8,ctypes.c_uint8,ctypes.c_uint8]
        L.SDL_RenderClear.argtypes=[ctypes.c_void_p]; L.SDL_RenderPresent.argtypes=[ctypes.c_void_p]
        L.SDL_RenderFillRect.argtypes=[ctypes.c_void_p,ctypes.c_void_p]
        L.SDL_PollEvent.argtypes=[ctypes.c_void_p]; L.SDL_PollEvent.restype=ctypes.c_int
        # SDL_QUIT=0x100, SDL_KEYDOWN=0x300, SDL_CONTROLLERBUTTONDOWN=0x651, SDL_JOYBUTTONDOWN=0x603.
        for driver in ("fbcon","kmsdrm","wayland","x11"):
            if driver == "fbcon" and not os.path.exists("/dev/fb0"): continue
            os.environ["SDL_VIDEODRIVER"]=driver
            if L.SDL_Init(0x00000020 | 0x00002000) == 0:
                self.window=L.SDL_CreateWindow(b"Gatito-Extrator",0,0,640,480,0x00000005)
                if self.window:
                    self.renderer=L.SDL_CreateRenderer(self.window,-1,2)
                    if not self.renderer:
                        self.renderer=L.SDL_CreateRenderer(self.window,-1,1)
                    if self.renderer:
                        self.enabled=True; self.w=640; self.h=480; return
                L.SDL_Quit()
        self.lib=None
    def event(self, logf=None):
        if not self.enabled: return False
        buf=ctypes.create_string_buffer(56)
        while self.lib.SDL_PollEvent(buf):
            t=struct.unpack_from("I",buf.raw,0)[0]
            if t == 0x100:
                if logf: logf.write("[input] SDL_QUIT\n"); logf.flush()
                return True
            if t == 0x300:
                key=struct.unpack_from("I",buf.raw,20)[0]
                if logf: logf.write("[input] SDL_KEYDOWN key=%d\n" % key); logf.flush()
                continue
            if t in (0x603,0x604,0x651,0x652):
                button=struct.unpack_from("B",buf.raw,12)[0]
                down=t in (0x603,0x651)
                # Eventos de controlador SDL (0x651): BACK=4, START=6.
                # Eventos crus de joystick (0x603) variam por fabricante
                # (BACK=6, START=7) - aceitamos os dois esquemas.
                if t in (0x651,0x652):
                    sel, st = 4, 6
                else:
                    sel, st = 6, 7
                if button == sel:
                    self.select_down = down
                elif button == st:
                    self.start_down = down
                elif t in (0x651,0x652) and button == 7:
                    # alguns pads enviam START=7 mesmo em eventos de controlador
                    self.start_down = down
                if logf:
                    logf.write("[input] SDL button=%d %s select=%s start=%s\n"
                               % (button, "DOWN" if down else "UP",
                                  self.select_down, self.start_down))
                    logf.flush()
                if self.start_down and self.select_down: return True
        return False
    def draw(self,pct,stage,details):
        L=self.lib
        L.SDL_SetRenderDrawColor(self.renderer,10,14,20,255); L.SDL_RenderClear(self.renderer)
        class R(ctypes.Structure):
            _fields_=[("x",ctypes.c_int),("y",ctypes.c_int),("w",ctypes.c_int),("h",ctypes.c_int)]
        def rect(x,y,w,h,c):
            r=R(x,y,w,h); L.SDL_SetRenderDrawColor(self.renderer,*c,255)
            L.SDL_RenderFillRect(self.renderer,ctypes.byref(r))
        def text(x,y,value,scale=2,c=(235,240,245),limit=42):
            value=str(value).upper()[:limit]
            for ch in value:
                glyph=FONT.get(ch,FONT[" "])
                for row,bits in enumerate(glyph):
                    for col in range(5):
                        if bits & (1 << (4-col)):
                            rect(x+col*scale,y+row*scale,scale,scale,c)
                x += 6*scale
        rect(24,24,592,432,(22,29,40))
        text(48,44,"GATITO EXTRATOR",3)
        text(48,78,"THE SIMS 3",2,(120,200,140))
        rect(48,116,544,30,(45,52,64))
        rect(48,116,544*max(0,min(100,pct))//100,30,(65,190,110))
        text(48,158,"%d%%" % pct,2)
        text(48,194,stage,2,(235,240,245),54)
        text(48,236,details,1,(180,190,200),76)
        text(48,382,"START + SELECT = SAIR",2,(120,200,140))
        self._last=(pct,stage,details)
        L.SDL_RenderPresent(self.renderer)
    def close(self):
        if self.lib:
            if self.renderer and hasattr(self.lib,"SDL_DestroyRenderer"): self.lib.SDL_DestroyRenderer(self.renderer)
            if self.window and hasattr(self.lib,"SDL_DestroyWindow"): self.lib.SDL_DestroyWindow(self.window)
            self.lib.SDL_Quit()

def input_devices():
    return sorted(glob.glob("/dev/input/event*"))

EVIOCGNAME = 0x80004506
BTN_MODE = 316
BTN_SELECT = 314  # BTN_SELECT — mantido para compatibilidade
BTN_START = 315   # BTN_START — usado como START em muitos pads

class RawInputMonitor:
    def __init__(self, logf=None):
        self.logf=logf; self.fds=[]; self.paths=[]; self.start_down=False; self.select_down=False
        for path in input_devices():
            try:
                fd=os.open(path,os.O_RDONLY|os.O_NONBLOCK); self.fds.append(fd); self.paths.append(path)
                name=ctypes.create_string_buffer(256)
                try: fcntl.ioctl(fd,EVIOCGNAME,name); device=name.value.decode("utf-8","replace") or "<sem-nome>"
                except OSError: device="<nome indisponivel>"
                self.log("[input-device] %s name=%s" % (path,device))
            except OSError as exc: self.log("[input-device] %s open_error=%s" % (path,exc))
    def log(self,msg):
        if self.logf: self.logf.write(msg+"\n"); self.logf.flush()
    def poll(self):
        if not self.fds: return False
        ready,_,_=select.select(self.fds,[],[],0)
        for fd in ready:
            path=self.paths[self.fds.index(fd)]
            try:
                while True:
                    data=os.read(fd,INPUT_EVENT.size)
                    if len(data)!=INPUT_EVENT.size: break
                    _,_,typ,code,val=INPUT_EVENT.unpack(data)
                    if typ==EV_KEY:
                        label={BTN_SELECT:"SELECT",BTN_START:"START",BTN_MODE:"START"}.get(code,"")
                        state="DOWN" if val==1 else ("UP" if val==0 else "REPEAT")
                        self.log("[input] %s type=EV_KEY code=%d%s value=%d state=%s" % (path,code,(" label="+label) if label else "",val,state))
                        if code == BTN_SELECT:
                            self.select_down = (val != 0)
                        elif code in (BTN_START, BTN_MODE):
                            self.start_down = (val != 0)
                    elif typ: self.log("[input] %s type=%d code=%d value=%d" % (path,typ,code,val))
            except OSError as exc:
                if exc.errno!=errno.EAGAIN: self.log("[input] %s read_error=%s" % (path,exc))
        return self.start_down and self.select_down
    def close(self):
        for fd in self.fds:
            try: os.close(fd)
            except OSError: pass
        self.fds=[]

def command_output(cmd):
    try:
        p=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=3)
        return p.stdout.strip().replace("\n"," ")[:500]
    except Exception as exc: return "<erro: %s>" % exc

def collect_data(game_dir,recipe,engine,log):
    lines=["Host: "+platform.platform(),"Kernel: "+command_output(["uname","-a"]),
           "Python: "+sys.version.split()[0],"GAMEDIR: "+str(game_dir),
           "Recipe: "+str(recipe),"Engine: "+str(engine),
           "CFW: "+os.environ.get("CFW_NAME","unknown"),
           "DEVICE: "+os.environ.get("DEVICE_NAME","unknown"),
           "ARCH: "+os.environ.get("DEVICE_ARCH",platform.machine()),
           "DISPLAY: "+os.environ.get("DISPLAY","<none>"),
           "WAYLAND_DISPLAY: "+os.environ.get("WAYLAND_DISPLAY","<none>"),
           "TERM: "+os.environ.get("TERM","<none>"),"PATH: "+os.environ.get("PATH","<none>"),
           "Disk: "+command_output(["df","-h",str(game_dir)])]
    for path in ("/dev/fb0","/dev/dri","/dev/mali0","/dev/snd"):
        lines.append("%s: %s" % (path,"present" if os.path.exists(path) else "missing"))
    lines.append("SDL2: %s" % ("available" if ctypes.util.find_library("SDL2") else "not found"))
    lines.append("INPUT_EVENT_DEVICES: %d" % len(input_devices()))
    for dev in input_devices(): lines.append("input: %s" % dev)
    for tool in ("python3","bash","sh","unzip","xz","7z"):
        lines.append("tool %s: %s" % (tool,"yes" if shutil.which(tool) else "no"))
    Path(log).parent.mkdir(parents=True,exist_ok=True)
    with open(log,"a",encoding="utf-8") as f: f.write("\n=== Gatito-Extrator diagnostics ===\n"+"\n".join(lines)+"\n")
    return lines

def fb_draw(fb,pct,stage,details):
    fb.fill((10,14,20))
    w,h=fb.xres,fb.yres; scale=max(1,min(4,w//320))
    fb.rect(10,10,w-20,h-20,(22,29,40))
    fb.text(24,24,"GATITO EXTRATOR",scale)
    fb.text(24,52,"ETAPA",scale,(120,200,140)); fb.text(24,72,stage[:max(1,w//(6*scale)-4)],scale)
    y=100 if h>=240 else 70
    fb.rect(24,y,w-48,24,(45,52,64)); fb.rect(24,y,(w-48)*max(0,min(100,pct))//100,24,(65,190,110))
    fb.text(24,y+34,"%d%%" % pct,scale)
    fb.text(24,y+60,"INTERFACE GRAFICA ATIVA",scale,(120,200,140))
    fb.text(24,y+84,"BOTAO: SAIR APOS O TESTE",scale)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--game-dir",required=True); ap.add_argument("--recipe",required=True)
    ap.add_argument("--engine",required=True); ap.add_argument("--log",required=True)
    ap.add_argument("--input"); ap.add_argument("--abi"); ap.add_argument("--demo",action="store_true")
    ap.add_argument("--no-pause",action="store_true")
    ap.add_argument("--exit-timeout",type=int,default=int(os.environ.get("SIMS3_UI_EXIT_TIMEOUT","20") or 20))
    a=ap.parse_args()
    game_dir=Path(a.game_dir).resolve(); recipe=Path(a.recipe).resolve(); engine=Path(a.engine).resolve()
    details=collect_data(game_dir,recipe,engine,a.log)
    ui=SDLUI(); fb=None
    if not ui.enabled:
        try: fb=Framebuffer() if os.path.exists("/dev/fb0") else None
        except Exception: fb=None
    if not ui.enabled and not fb:
        print("GATITO UI ERROR: SDL2 e /dev/fb0 indisponiveis",file=sys.stderr); return 2
    raw_monitor=None
    def draw(p,s,d):
        if ui.enabled: ui.draw(p,s,d)
        else: fb_draw(fb,p,s,d)
    try:
        draw(5,"AMBIENTE DETECTADO","Interface grafica iniciada")
        with open(a.log,"a",encoding="utf-8") as prep_log:
            raw_monitor=RawInputMonitor(prep_log)
            mapping_end=time.time()+3.0
            while time.time()<mapping_end:
                combo=ui.event(prep_log) if ui.enabled else raw_monitor.poll()
                draw(12,"MAPEANDO CONTROLES","Pressione os botoes para registrar no log")
                if combo:
                    draw(12,"CANCELADO","START + SELECT"); return 0
                time.sleep(.03)
            for remaining in range(5,0,-1):
                draw(18,"INICIO EM %d" % remaining,"Validacao e extracao comecam em seguida")
                combo=ui.event(prep_log) if ui.enabled else raw_monitor.poll()
                if combo:
                    draw(18,"CANCELADO","START + SELECT"); return 0
                time.sleep(1.0)
        if a.demo:
            for p,s in [(0,"INICIALIZANDO"),(15,"COLETANDO AMBIENTE"),(35,"DETECTANDO FERRAMENTAS"),
                        (55,"SIMULANDO STAGING"),(75,"VALIDANDO ARTEFATOS"),(92,"PUBLICANDO"),(100,"TESTE CONCLUIDO")]:
                draw(p,s,"Interface grafica ativa"); time.sleep(.45)
            rc=0; final="TESTE CONCLUIDO"
        elif not engine.is_file():
            draw(100,"ERRO ENGINE NAO ENCONTRADA",str(engine)); rc=1; final="ERRO"
        else:
            cmd=[sys.executable,str(engine),str(recipe),"--game-dir",str(game_dir),"--validation-delay","0.15"]
            if a.input: cmd += ["--input",a.input]
            if a.abi: cmd += ["--abi",a.abi]
            draw(8,"INICIANDO EXTRATOR","Interface grafica permanece na tela")
            with open(a.log,"a",encoding="utf-8") as logf:
                logf.write("\n=== Gatito-Extrator run ===\nCMD: "+" ".join(cmd)+"\n")
                p=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
                pct=8; last="EXECUTANDO"; rc=None
                while True:
                    ready,_,_=select.select([p.stdout],[],[],0.05)
                    if ready:
                        line=p.stdout.readline()
                        if line:
                            line=line.rstrip(); logf.write(line+"\n"); logf.flush()
                            if line.startswith("GATITO_STAGE|"):
                                try: _,v,msg=line.split("|",2); pct=int(v); last=msg
                                except ValueError: pass
                            elif line: last=line[:80]
                    combo=ui.event(logf) if ui.enabled else (raw_monitor.poll() if raw_monitor else False)
                    draw(pct,last,"PROCESSO ATIVO | START+SELECT = SAIR")
                    if combo:
                        logf.write("[input] START+SELECT durante extracao\n"); logf.flush()
                        p.terminate()
                        try: p.wait(timeout=2)
                        except Exception: p.kill(); p.wait()
                        rc=130; break
                    if p.poll() is not None:
                        for line in p.stdout:
                            line=line.rstrip(); logf.write(line+"\n")
                            if line.startswith("GATITO_STAGE|"):
                                try: _,v,msg=line.split("|",2); pct=int(v); last=msg
                                except ValueError: pass
                        rc=p.returncode; break
                final="CONCLUIDO" if rc==0 else ("CANCELADO" if rc==130 else "ERRO - PROCESSO TERMINOU")
                draw(100 if rc==0 else pct,final,"ARQUIVOS E LOG PRESERVADOS")
        if not a.no_pause:
            # Sai automaticamente apos timeout (padrao 20s, ou SIMS3_UI_EXIT_TIMEOUT)
            deadline = time.time() + a.exit_timeout
            while time.time() < deadline:
                remaining = int(max(0, deadline - time.time()))
                draw(100 if rc == 0 else 0,
                     final,
                     "SAINDO EM %d S  |  START+SELECT = AGORA" % remaining)
                combo = ui.event() if ui.enabled else (raw_monitor.poll() if raw_monitor else False)
                if combo:
                    break
                time.sleep(.03)
            # timeout atingido -> sai automaticamente (proxima etapa: iniciar jogo)
        return rc
    finally:
        if raw_monitor: raw_monitor.close()
        if fb: fb.close()
        ui.close()

if __name__=="__main__":
    raise SystemExit(main())
