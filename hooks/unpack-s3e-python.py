#!/usr/bin/env python3
"""Fallback LZMA-alone decoder for The Sims 3 S3E payload."""
import os
import sys
import tempfile

def main():
    if len(sys.argv) != 3:
        print("uso: unpack-s3e-python.py INPUT OUTPUT", file=sys.stderr)
        return 2

    src, dst = sys.argv[1], sys.argv[2]
    try:
        import lzma
    except Exception as exc:
        print("python lzma indisponível: " + str(exc), file=sys.stderr)
        return 20

    try:
        with open(src, "rb") as f:
            data = f.read()

        # Marmalade S3E here uses the legacy LZMA-alone container.
        out = lzma.decompress(data, format=lzma.FORMAT_ALONE)

        if not out:
            raise RuntimeError("saída vazia")
        if out[:4] != bytes.fromhex("58453355"):
            raise RuntimeError("header XE3U não encontrado")

        fd, tmp = tempfile.mkstemp(prefix=".game.s3e.unpacked.", dir=os.path.dirname(dst))
        try:
            with os.fdopen(fd, "wb") as f:
                f.write(out)
                f.flush()
                os.fsync(f.fileno())
            os.replace(tmp, dst)
        finally:
            if os.path.exists(tmp):
                os.unlink(tmp)

        print("python-lzma: OK XE3U", flush=True)
        return 0
    except Exception as exc:
        print("python-lzma: ERRO: " + str(exc), file=sys.stderr)
        try:
            os.unlink(dst)
        except FileNotFoundError:
            pass
        return 21

if __name__ == "__main__":
    raise SystemExit(main())
