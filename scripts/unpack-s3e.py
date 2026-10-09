#!/usr/bin/env python3
import lzma, os, sys, tempfile
from pathlib import Path
MAGIC = b'XE3U'
MIN_OUTPUT = 1024 * 1024
def main():
    if len(sys.argv) != 3:
        print('usage: unpack-s3e.py INPUT OUTPUT', file=sys.stderr); return 2
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    try:
        if not src.is_file() or src.is_symlink(): raise ValueError(f'unsafe input: {src}')
        payload = lzma.decompress(src.read_bytes(), format=lzma.FORMAT_ALONE)
        if len(payload) < MIN_OUTPUT: raise ValueError(f'payload too small: {len(payload)}')
        if payload[:4] != MAGIC: raise ValueError(f'XE3U magic missing: {payload[:4].hex()}')
        dst.parent.mkdir(parents=True, exist_ok=True)
        fd, tmp = tempfile.mkstemp(prefix='.unpack-s3e-', dir=str(dst.parent))
        try:
            with os.fdopen(fd, 'wb') as f: f.write(payload); f.flush(); os.fsync(f.fileno())
            os.replace(tmp, dst)
        finally:
            if os.path.exists(tmp): os.unlink(tmp)
        print(f'[S3E-UNPACK][OK] {dst} ({len(payload)} bytes)'); return 0
    except Exception as exc:
        print(f'[S3E-UNPACK][ERROR] {exc}', file=sys.stderr); return 1
if __name__ == '__main__': raise SystemExit(main())
