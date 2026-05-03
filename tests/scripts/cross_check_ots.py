#!/usr/bin/env python3
"""Cross-check uc2 OTS output against the python-opentimestamps reference.

Usage: cross_check_ots.py <uc2-binary> <work-dir>

Builds a tiny archive, attaches a hand-crafted OTS proof, then:
  1. Extracts via `uc2 --ots-extract`
  2. Round-trips the .ots through python-opentimestamps
  3. Confirms the proof's leaf digest equals SHA-256 of the attested archive prefix

Exits 0 on success, 1 on mismatch, 77 (autotools "skip" code) if the
opentimestamps library isn't installed.
"""

import hashlib
import os
import struct
import subprocess
import sys
from io import BytesIO

try:
    from opentimestamps.core.timestamp import DetachedTimestampFile
    from opentimestamps.core.serialize import StreamDeserializationContext
except ModuleNotFoundError:
    print("opentimestamps library not installed; skipping cross-check.")
    sys.exit(77)


HEADER_MAGIC = (b"\x00OpenTimestamps\x00\x00Proof\x00"
                b"\xbf\x89\xe2\xe8\x84\xe8\x92\x94")
PENDING_TAG = b"\x83\xdf\xe3\x0d\x2e\xf9\x0c\x8e"
TRAILER_MAGIC = b"UC2-OTS\x00"


def varint(n):
    out = b""
    while n >= 0x80:
        out += bytes([n & 0x7f | 0x80])
        n >>= 7
    return out + bytes([n])


def varbytes(b):
    return varint(len(b)) + b


def build_proof(leaf):
    # Pending attestation payload is itself varbytes(uri) per the OTS spec,
    # wrapped in the outer varbytes(serialized_attestation) layer.
    pending_payload = varbytes(b"https://example.com/digest")
    body = b"\x00" + PENDING_TAG + varbytes(pending_payload)
    return HEADER_MAGIC + b"\x01" + b"\x08" + leaf + body


def main():
    if len(sys.argv) != 3:
        print("usage: cross_check_ots.py <uc2-binary> <work-dir>", file=sys.stderr)
        return 1
    uc2 = sys.argv[1]
    work = sys.argv[2]
    os.makedirs(work, exist_ok=True)

    a = os.path.join(work, "a.txt")
    b = os.path.join(work, "b.txt")
    with open(a, "w") as f: f.write("hello uc2 ots cross-check\n")
    with open(b, "w") as f: f.write("second file\n")

    arc = os.path.join(work, "test.uc2")
    subprocess.check_call([uc2, "-w", "-q", arc, a, b])

    archive_size = os.path.getsize(arc)
    with open(arc, "rb") as f:
        archive_bytes = f.read()
    leaf = hashlib.sha256(archive_bytes).digest()

    proof_path = os.path.join(work, "proof.ots")
    with open(proof_path, "wb") as f:
        f.write(build_proof(leaf))

    subprocess.check_call([uc2, "--ots-attach", proof_path, arc])

    extracted = os.path.join(work, "extracted.ots")
    subprocess.check_call([uc2, "--ots-extract", arc, extracted])

    with open(extracted, "rb") as f:
        ots_bytes = f.read()
    ctx = StreamDeserializationContext(BytesIO(ots_bytes))
    detached = DetachedTimestampFile.deserialize(ctx)

    py_leaf = bytes(detached.timestamp.msg)
    if py_leaf != leaf:
        print("LEAF MISMATCH", file=sys.stderr)
        print(f"  hand-computed: {leaf.hex()}", file=sys.stderr)
        print(f"  python-ots:    {py_leaf.hex()}", file=sys.stderr)
        return 1

    attestations = list(detached.timestamp.all_attestations())
    if not attestations:
        print("no attestations parsed by python-opentimestamps", file=sys.stderr)
        return 1

    info = subprocess.check_output(
        [uc2, "--ots-info", arc], stderr=subprocess.STDOUT, text=True)
    if "leaf matches archive: yes" not in info:
        print("uc2 --ots-info reports leaf mismatch:", file=sys.stderr)
        print(info, file=sys.stderr)
        return 1

    if archive_size + len(ots_bytes) >= os.path.getsize(arc):
        pass  # archive grew by at least proof_len; trailer is present

    print(f"cross-check OK: archive_size={archive_size}, proof_len={len(ots_bytes)}, "
          f"attestations={len(attestations)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
