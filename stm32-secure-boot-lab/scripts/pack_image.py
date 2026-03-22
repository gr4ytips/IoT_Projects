#!/usr/bin/env python3
"""Sign and pack an STM32 application image for the secure bootloader.

This script:
1. Reads an application binary linked at APP_VECTOR_ADDR.
2. Uses OpenSSL to create an ECDSA P-256 signature over the application bytes.
3. Converts the DER signature into raw 64-byte r||s form.
4. Prepends a 512-byte image header.
5. Writes app_packed.bin for flashing at APP_HEADER_ADDR.

Requirements:
- Python 3
- OpenSSL in PATH
- Private key on prime256v1 / secp256r1

Example:
  python3 pack_image.py app.bin app_packed.bin --key private.pem --version 1
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

APP_HEADER_ADDR = 0x08008000
APP_VECTOR_ADDR = 0x08008200
IMAGE_MAGIC = 0x474D4942  # "BIMG"
IMAGE_HDR_VER = 0x00000002
IMAGE_HDR_SIZE = 0x200


def die(msg: str) -> None:
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(1)


def run_checked(cmd: list[str]) -> bytes:
    try:
        cp = subprocess.run(cmd, check=True, capture_output=True)
    except FileNotFoundError:
        die(f"missing executable: {cmd[0]}")
    except subprocess.CalledProcessError as exc:
        stderr = exc.stderr.decode(errors="replace") if exc.stderr else ""
        die(f"command failed: {' '.join(cmd)}\n{stderr}")
    return cp.stdout


def read_der_len(buf: bytes, off: int) -> tuple[int, int]:
    if off >= len(buf):
        raise ValueError("DER length out of range")
    first = buf[off]
    off += 1
    if first < 0x80:
        return first, off
    nbytes = first & 0x7F
    if nbytes == 0 or nbytes > 4:
        raise ValueError("unsupported DER length")
    if off + nbytes > len(buf):
        raise ValueError("truncated DER length")
    value = 0
    for _ in range(nbytes):
        value = (value << 8) | buf[off]
        off += 1
    return value, off


def der_ecdsa_to_raw64(sig_der: bytes) -> bytes:
    off = 0
    if len(sig_der) < 8 or sig_der[off] != 0x30:
        raise ValueError("signature is not a DER SEQUENCE")
    off += 1
    seq_len, off = read_der_len(sig_der, off)
    if off + seq_len != len(sig_der):
        raise ValueError("invalid DER SEQUENCE length")

    if sig_der[off] != 0x02:
        raise ValueError("missing INTEGER tag for r")
    off += 1
    r_len, off = read_der_len(sig_der, off)
    r = sig_der[off:off + r_len]
    off += r_len

    if sig_der[off] != 0x02:
        raise ValueError("missing INTEGER tag for s")
    off += 1
    s_len, off = read_der_len(sig_der, off)
    s = sig_der[off:off + s_len]
    off += s_len

    if off != len(sig_der):
        raise ValueError("trailing bytes after DER signature")

    r = r[1:] if len(r) > 32 and r[0] == 0x00 else r
    s = s[1:] if len(s) > 32 and s[0] == 0x00 else s

    if len(r) > 32 or len(s) > 32:
        raise ValueError("ECDSA component too large for P-256")

    return r.rjust(32, b"\x00") + s.rjust(32, b"\x00")


def sign_app(app_bytes: bytes, key_path: Path) -> bytes:
    with tempfile.TemporaryDirectory() as td:
        td_path = Path(td)
        app_path = td_path / "app.bin"
        sig_der_path = td_path / "sig.der"
        app_path.write_bytes(app_bytes)

        run_checked([
            "openssl", "dgst", "-sha256", "-sign", str(key_path),
            "-out", str(sig_der_path), str(app_path)
        ])

        sig_der = sig_der_path.read_bytes()
        return der_ecdsa_to_raw64(sig_der)


def build_header(image_size: int, app_version: int, flags: int, signature: bytes) -> bytes:
    if len(signature) != 64:
        raise ValueError("signature must be 64 bytes")

    header = struct.pack(
        "<6I64s424s",
        IMAGE_MAGIC,
        IMAGE_HDR_VER,
        image_size,
        APP_VECTOR_ADDR,
        app_version,
        flags,
        signature,
        b"\x00" * 424,
    )

    if len(header) != IMAGE_HDR_SIZE:
        raise ValueError("header size mismatch")

    return header


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("app_bin", help="Raw application binary linked at APP_VECTOR_ADDR")
    ap.add_argument("out_bin", help="Packed image to flash at APP_HEADER_ADDR")
    ap.add_argument("--key", required=True, help="ECDSA private key PEM (prime256v1)")
    ap.add_argument("--version", type=int, default=1, help="Application version")
    ap.add_argument("--flags", type=int, default=0, help="Header flags")
    args = ap.parse_args()

    app_path = Path(args.app_bin)
    out_path = Path(args.out_bin)
    key_path = Path(args.key)

    if not app_path.is_file():
        die(f"application binary not found: {app_path}")
    if not key_path.is_file():
        die(f"private key not found: {key_path}")

    app_bytes = app_path.read_bytes()
    if not app_bytes:
        die("application binary is empty")

    signature = sign_app(app_bytes, key_path)
    header = build_header(len(app_bytes), args.version, args.flags, signature)
    out_path.write_bytes(header + app_bytes)

    print(f"app bytes : {len(app_bytes)}")
    print(f"sig raw   : {signature.hex()}")
    print(f"out file  : {out_path}")
    print(f"flash @   : 0x{APP_HEADER_ADDR:08X}")


if __name__ == "__main__":
    main()
