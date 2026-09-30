#!/usr/bin/env python3
"""Trusted per-port backend: delegates data preparation to the existing NXExtract."""
import os, subprocess
from pathlib import Path

root=Path(__file__).resolve().parents[1]
env=os.environ.copy()
env["NXEXTRACT_GAME_DIR"]=str(root)
env["NXEXTRACT_RECIPE"]=str(root/"extractor.json")
cmd=["bash",str(root/"run-extractor.sh")]
raise SystemExit(subprocess.call(cmd,cwd=root,env=env))
