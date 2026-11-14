#!/usr/bin/env python3
"""CLI regressions for OTS suffix framing, wide offsets and benchmark input."""

import argparse
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


MAGIC = b"UC2-OTS\0"
PROOF_LIMIT = 1 << 20


def trailer(prefix_len, proof):
    return (MAGIC + struct.pack("<III", 1, prefix_len, len(proof)) + proof
            + struct.pack("<I", len(proof)) + MAGIC)


def proof_for(data, uri=b"https://example.com/digest"):
    magic = b"\0OpenTimestamps\0\0Proof\0\xbf\x89\xe2\xe8\x84\xe8\x92\x94"
    pending = b"\x83\xdf\xe3\x0d\x2e\xf9\x0c\x8e"
    # Both lengths fit a one-byte canonical OTS varint.
    payload = bytes([len(uri)]) + uri
    return (magic + b"\x01\x08" + hashlib.sha256(data).digest()
            + b"\0" + pending + bytes([len(payload)]) + payload)


def sparse_file(path):
    f = path.open("w+b")
    if os.name == "nt":
        # NTFS needs an explicit sparse-file flag before writing beyond EOF.
        import ctypes
        from ctypes import wintypes
        import msvcrt
        control = ctypes.WinDLL("kernel32", use_last_error=True).DeviceIoControl
        control.argtypes = [wintypes.HANDLE, wintypes.DWORD, ctypes.c_void_p,
                            wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD,
                            ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
        control.restype = wintypes.BOOL
        returned = wintypes.DWORD()
        if not control(msvcrt.get_osfhandle(f.fileno()), 0x900C4, None, 0,
                       None, 0, ctypes.byref(returned), None):
            f.close()
            raise OSError(ctypes.get_last_error(), "FSCTL_SET_SPARSE failed")
    return f


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("uc2")
    parser.add_argument("--no-memory-limit", action="store_true",
                        help="disable the Linux address-space cap for sanitizers")
    args = parser.parse_args()
    uc2 = str(Path(args.uc2).resolve())
    if args.no_memory_limit or os.environ.get("ASAN_OPTIONS"):
        print("Address-space cap disabled for sanitizers; sparse-offset checks remain enabled")

    def run(*argv, ok=True, bounded=False, input=None, cwd=None):
        preexec = None
        if (bounded and sys.platform == "linux" and not args.no_memory_limit
                and not os.environ.get("ASAN_OPTIONS")):
            import resource

            def preexec():
                resource.setrlimit(resource.RLIMIT_AS, (192 << 20, 192 << 20))

        result = subprocess.run([uc2, *map(str, argv)], input=input,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                timeout=30, preexec_fn=preexec, cwd=cwd)
        assert (result.returncode == 0) == ok, (argv, result.returncode, result.stderr)
        assert b": : " not in result.stderr, result.stderr
        return result

    with tempfile.TemporaryDirectory(prefix="uc2-cli-io-") as tmp:
        work = Path(tmp)
        source = work / "data.txt"
        source.write_bytes(b"CLI I/O regression\n" * 8)
        empty = work / "empty"
        empty.write_bytes(b"")
        empty_result = run("-B", empty)
        assert b"0 bytes input" in empty_result.stdout
        assert b"ERROR" not in empty_result.stdout
        expected_size = f"{source.stat().st_size} bytes input".encode()
        assert expected_size in run("-B", empty, source).stdout
        run("-B", work, ok=False)

        # Long-option spellings after -- are input filenames.
        for index, name in enumerate(("--ots-info", "--ots-extract", "--ots-attach=x")):
            literal = work / name
            literal.write_bytes(name.encode())
            literal_archive = work / f"literal-{index}.uc2"
            run("-qw", literal_archive, "--", name, cwd=work)
            assert run("--ingest-restore", literal_archive).stdout == literal.read_bytes()

        archive = work / "small.uc2"
        run("-w", "-q", archive, source)
        original = archive.read_bytes()
        assert run("--ingest-restore", archive).stdout == source.read_bytes()
        run("-t", "-q", archive)
        run("--ots-extract", archive, work / "absent.ots", ok=False)

        long_proof = proof_for(original)
        short_proof = proof_for(original, b"x")
        proof_path = work / "proof.ots"
        proof_path.write_bytes(long_proof)
        run("--ots-attach", proof_path, archive)
        assert archive.read_bytes() == original + trailer(len(original), long_proof)
        run("--ots-attach", proof_path, archive, ok=False)
        assert archive.read_bytes() == original + trailer(len(original), long_proof)
        proof_path.write_bytes(short_proof)
        run("--ots-attach", proof_path, "-f", archive)
        assert archive.read_bytes() == original + trailer(len(original), short_proof)
        run("-t", "-q", archive)
        assert b"leaf matches archive: yes" in run("--ots-info", archive).stdout
        extracted = work / "extracted.ots"
        run("--ots-extract", archive, extracted)
        assert extracted.read_bytes() == short_proof
        pending_tag = b"\x83\xdf\xe3\x0d\x2e\xf9\x0c\x8e"
        for payload in (b"\xff" * 9 + b"\x01", b"", b"\x80", b"\x02x"):
            bad_proof = (short_proof[:65] + b"\0" + pending_tag
                         + bytes([len(payload)]) + payload)
            malformed_uri = work / "malformed-uri.uc2"
            malformed_uri.write_bytes(original + trailer(len(original), bad_proof))
            result = run("--ots-info", malformed_uri, ok=False)
            assert b"walker error" in result.stderr, result.stderr
            assert len(result.stdout) < 4096

        if Path("/dev/full").exists():
            for argv in (("-p", archive), ("--ingest-restore", archive),
                         ("-l", archive), ("-B", source), ("--ots-info", archive)):
                with open("/dev/full", "wb") as full:
                    result = subprocess.run([uc2, *map(str, argv)], stdout=full,
                                            stderr=subprocess.PIPE, timeout=30)
                assert result.returncode != 0, (argv, result.stderr)
            run("--ots-extract", archive, "/dev/full", ok=False)

        proof_path.write_bytes(b"x" * (PROOF_LIMIT + 1))
        before = archive.read_bytes()
        run("--ots-attach", proof_path, "-f", archive, ok=False, bounded=True)
        assert archive.read_bytes() == before

        # These exercise the trailer API's framing error classes independently
        # of the .ots envelope, which extraction must preserve verbatim.
        framed = trailer(3, b"proof")
        cases = [
            ("short", MAGIC, "no OTS trailer"),
            ("truncated", struct.pack("<I", 5) + MAGIC, "(-1)"),
            ("large", struct.pack("<I", PROOF_LIMIT + 1) + MAGIC, "(-8)"),
            ("front-magic", b"abc" + b"X" + framed[1:], "(-4)"),
            ("version", b"abc" + framed[:8] + struct.pack("<I", 2) + framed[12:], "(-5)"),
            ("length-copy", b"abc" + framed[:16] + struct.pack("<I", 4) + framed[20:], "(-2)"),
            ("prefix-length", b"abcd" + framed, "(-3)"),
        ]
        for name, data, message in cases:
            bad = work / (name + ".uc2")
            bad.write_bytes(data)
            result = run("--ots-extract", bad, extracted, ok=False)
            assert message.encode() in result.stderr, (name, result.stderr)
        for proof in (b"", b"opaque proof bytes", b"x" * PROOF_LIMIT):
            framed_path = work / "framed.uc2"
            framed_path.write_bytes(b"abc" + trailer(3, proof))
            run("--ots-extract", framed_path, extracted, bounded=True)
            assert extracted.read_bytes() == proof

        # Relocating the central directory beyond signed-long range keeps the
        # decoded payload tiny while exercising real archive callback seeks.
        cdir = struct.unpack_from("<I", original, 17)[0]
        high_offset = (1 << 31) + 4096
        new_size = high_offset + len(original) - cdir
        head = bytearray(original[:cdir])
        component_len = new_size - 13
        struct.pack_into("<II", head, 4, component_len,
                         (component_len + 0x01B2C3D4) & 0xFFFFFFFF)
        struct.pack_into("<I", head, 17, high_offset)
        large = work / "wide.uc2"
        try:
            f = sparse_file(large)
        except OSError as error:
            print(f"SKIP wide-offset checks: sparse files unavailable: {error}")
        else:
            with f:
                f.write(head)
                f.seek(high_offset)
                f.write(original[cdir:])
            run("-l", "-q", large, bounded=True)
            run("-t", "-q", large, bounded=True)
            assert run("--ingest-restore", large, bounded=True).stdout == source.read_bytes()
            with large.open("ab") as f:
                f.write(trailer(new_size, b"large-offset proof"))
            run("--ots-extract", large, extracted, bounded=True)
            assert extracted.read_bytes() == b"large-offset proof"

        # Oversized benchmark input is rejected before allocating its contents.
        huge = work / "oversized"
        try:
            f = sparse_file(huge)
        except OSError:
            pass  # Same sparse-file limitation reported above.
        else:
            with f:
                f.seek((1 << 32) - 1)
                f.write(trailer((1 << 32) - 1, b"proof beyond 4 GiB"))
            run("--ots-extract", huge, extracted, bounded=True)
            assert extracted.read_bytes() == b"proof beyond 4 GiB"
            result = run("-B", huge, ok=False, bounded=True)
            assert b"benchmark input exceeds" in result.stderr, result.stderr

    print("CLI I/O checks passed")


if __name__ == "__main__":
    main()
