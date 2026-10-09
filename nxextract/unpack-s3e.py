#!/usr/bin/env python3
"""The Sims 3 S3E unpack (migrated from hooks/ into the NxExtract flow).

Decodes the LZMA-alone S3E payload that NxExtract pulls out of the APK into
the XE3U payload the loader expects at ``game/game.s3e.unpacked``.

Two decode paths, tried in order:

1. Python stdlib ``lzma`` (``FORMAT_ALONE``, then a strict ``FORMAT_RAW``
   fallback that parses the 13-byte LZMA-alone header by hand). This is the
   validated, self-contained path and works on any Python with liblzma.
2. The port loader's bundled Marmalade LZMA SDK via
   ``sims3_s3e_loader --unpack-s3e``, used only when the Python ``lzma``
   module is unavailable (system without liblzma).

Both paths validate the ``XE3U`` magic before writing, and the final write is
atomic. Usage: ``unpack-s3e.py INPUT OUTPUT [--loader PATH]``.

Exit codes: 0 ok, 2 usage, 20 no decoder available, 21 decode/validation error.
"""
import os
import struct
import subprocess
import sys
import tempfile

XE3U_MAGIC = bytes.fromhex("58453355")


def _parse_alone_header(data):
    """Parse an LZMA-alone header into ``(dict_size, lc, lp, pb, payload_off)``."""
    if len(data) < 13:
        raise ValueError("LZMA-alone header truncated (%d bytes)" % len(data))
    props = data[0]
    if props >= 225:
        raise ValueError("invalid LZMA properties byte %d" % props)
    dict_size = struct.unpack("<I", data[1:5])[0]
    # bytes 5:13 hold the uncompressed size (uint64 LE); ignored while streaming.
    lc = props % 9
    lp = (props // 9) % 5
    pb = props // 45
    return dict_size, lc, lp, pb, 13


def _decode_stdlib(data):
    import lzma

    # Primary: the container is a plain LZMA-alone stream.
    try:
        return lzma.decompress(data, format=lzma.FORMAT_ALONE)
    except lzma.LZMAError:
        pass

    # Strict fallback (some Python builds reject the alone container): decode
    # the raw LZMA1 stream using the filters described by the alone header.
    dict_size, lc, lp, pb, offset = _parse_alone_header(data)
    filters = [{"id": lzma.FILTER_LZMA1, "dict_size": dict_size,
                "lc": lc, "lp": lp, "pb": pb}]
    dec = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=filters)
    return dec.decompress(data[offset:])


def _decode_loader(data, src, loader):
    """Fallback: the port loader bundles the Marmalade LZMA SDK."""
    directory = os.path.dirname(src) or "."
    fd, tmp = tempfile.mkstemp(prefix=".unpack-s3e.loader.", dir=directory)
    os.close(fd)
    os.unlink(tmp)  # the loader creates the destination itself
    try:
        proc = subprocess.run(
            [loader, "--unpack-s3e", src, tmp],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        if proc.returncode != 0:
            raise RuntimeError("loader --unpack-s3e exit %d: %s"
                               % (proc.returncode, (proc.stdout or "").strip()))
        with open(tmp, "rb") as handle:
            return handle.read()
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def _atomic_write(dst, payload):
    directory = os.path.dirname(dst) or "."
    if directory and not os.path.isdir(directory):
        os.makedirs(directory, exist_ok=True)
    fd, tmp = tempfile.mkstemp(prefix=".game.s3e.unpacked.", dir=directory)
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(tmp, dst)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def main(argv):
    args = list(argv[1:])
    loader = None
    if "--loader" in args:
        idx = args.index("--loader")
        if idx + 1 >= len(args):
            print("uso: --loader exige um caminho", file=sys.stderr)
            return 2
        loader = args[idx + 1]
        del args[idx:idx + 2]
    if len(args) != 2:
        print("uso: unpack-s3e.py INPUT OUTPUT [--loader PATH]", file=sys.stderr)
        return 2

    src, dst = args[0], args[1]
    if not os.path.isfile(src):
        print("unpack-s3e: entrada ausente: " + src, file=sys.stderr)
        return 21
    with open(src, "rb") as handle:
        data = handle.read()
    if not data:
        print("unpack-s3e: entrada vazia: " + src, file=sys.stderr)
        return 21

    print("unpack-s3e: entrada=%s bytes=%d header13=%s"
          % (src, len(data), data[:13].hex()), flush=True)

    payload = None
    decoder = None
    try:
        import lzma  # noqa: F401
        payload = _decode_stdlib(data)
        decoder = "python-stdlib-lzma"
    except ImportError:
        # No liblzma on this system: fall back to the port loader.
        if loader and os.path.isfile(loader):
            payload = _decode_loader(data, src, loader)
            decoder = "loader-bundled-lzma-sdk"
        else:
            print("unpack-s3e: sem decodificador (lzma indisponivel e "
                  "loader ausente)", file=sys.stderr)
            return 20
    except Exception as exc:  # decode or header parse failure
        print("unpack-s3e: ERRO ao decodificar: " + str(exc), file=sys.stderr)
        return 21

    if not payload:
        print("unpack-s3e: payload descompactado vazio", file=sys.stderr)
        return 21
    if payload[:4] != XE3U_MAGIC:
        print("unpack-s3e: header XE3U nao encontrado (obtido %s)"
              % payload[:4].hex(), file=sys.stderr)
        return 21

    _atomic_write(dst, payload)
    print("unpack-s3e: OK XE3U decoder=%s bytes_out=%d dst=%s"
          % (decoder, len(payload), dst), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
