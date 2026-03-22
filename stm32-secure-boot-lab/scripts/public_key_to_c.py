#!/usr/bin/env python3
"""Extract a raw uncompressed SEC1 public key from a DER public key file.

Usage:
  python3 public_key_to_c.py public.der

This prints a 65-byte C array suitable for boot_pubkey[] in verify.c.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def die(msg: str) -> None:
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(1)


def main() -> None:
    if len(sys.argv) != 2:
        die("usage: python3 public_key_to_c.py public.der")

    der_path = Path(sys.argv[1])
    if not der_path.is_file():
        die(f"file not found: {der_path}")

    try:
        cp = subprocess.run(
            ["openssl", "ec", "-pubin", "-inform", "DER", "-in", str(der_path), "-text", "-noout"],
            check=True,
            capture_output=True,
            text=True,
        )
    except FileNotFoundError:
        die("missing executable: openssl")
    except subprocess.CalledProcessError as exc:
        die(exc.stderr or "openssl command failed")

    text = cp.stdout.splitlines()
    collecting = False
    hex_bytes: list[str] = []
    for line in text:
        if line.strip().startswith("pub:"):
            collecting = True
            continue
        if collecting:
            s = line.strip()
            if not s or s.startswith("ASN1 OID") or s.startswith("NIST CURVE"):
                break
            parts = [p for p in s.split(':') if p]
            hex_bytes.extend(parts)

    if len(hex_bytes) != 65 or hex_bytes[0].lower() != '04':
        die(f"expected 65-byte uncompressed public key, found {len(hex_bytes)} bytes")

    print("static const uint8_t boot_pubkey[65] = {")
    for i in range(0, 65, 8):
        row = ', '.join(f"0x{b.lower()}" for b in hex_bytes[i:i+8])
        suffix = ',' if i + 8 < 65 else ''
        print(f"    {row}{suffix}")
    print("};")


if __name__ == "__main__":
    main()
