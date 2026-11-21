#!/usr/bin/env python3
"""Build the optional FreeDOS ZIP from pinned source and a DJGPP executable."""

import argparse
import hashlib
import io
from pathlib import Path
import tarfile
import zipfile

SOURCE_SHA256 = "5cf9097224a0a6bb96728fde0b2313d6a64df8c6582afad86cd2c482d761e28d"
CWSDPMI_SHA256 = "c5cc5fb22d721d7552570ecf17229efa34d0af7158a85c281a62a54cfe4b3ff3"
REVISION = "c8f52a7b0543792d159545ab06563c37099d49b3"


def checked(path, expected):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError(f"Unexpected SHA-256: {path}")
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--cwsdpmi-package", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    source = checked(args.source, SOURCE_SHA256)
    cwsdpmi = checked(args.cwsdpmi_package, CWSDPMI_SHA256)
    binary = args.binary.read_bytes()
    if not binary.startswith(b"MZ") or b"UC2 3.0.0a13+dos1 (UltraCompressor II)" not in binary:
        raise ValueError("Expected a DOS alpha13 executable")
    files = {
        "APPINFO/UC2.LSM": (root / "packaging/freedos/UC2.LSM").read_bytes(),
        "LINKS/UC2.BAT": b"PROGS\\UC2\\UC2.EXE\r\n",
        "PROGS/UC2/UC2.EXE": binary,
        "PROGS/UC2/README.TXT": (root / "packaging/freedos/README.TXT").read_bytes(),
        "SOURCE/UC2/UC2SRC.TGZ": source,
    }
    with tarfile.open(fileobj=io.BytesIO(source)) as archive:
        prefix = "uc2-dos-src/"
        for src, dst in [("LICENSE", "COPYING"), ("LICENSES/LGPL-3.0-only.txt", "LGPL.TXT"),
                         ("LICENSES/BSD-2-Clause.txt", "BSD.TXT"), ("CREDITS.md", "CREDITS.TXT")]:
            files[f"PROGS/UC2/{dst}"] = archive.extractfile(prefix + src).read()
    with zipfile.ZipFile(io.BytesIO(cwsdpmi)) as archive:
        for src, dst in [("BIN/CWSDPMI.EXE", "PROGS/UC2/CWSDPMI.EXE"),
                         ("DOC/CWSDPMI/COPYING.CWS", "PROGS/UC2/COPYING.CWS"),
                         ("DOC/CWSDPMI/CWSDPMI.DOC", "PROGS/UC2/CWSDPMI.DOC"),
                         ("SOURCE/CWSDPMI/SOURCES.ZIP", "SOURCE/UC2/CWSSRC.ZIP")]:
            files[dst] = archive.read(src)
    files["PROGS/UC2/SOURCE.TXT"] = (
        "UC2 source: https://github.com/evvaletov/uc2\n"
        f"Revision: {REVISION}\n"
        f"Source archive SHA-256: {SOURCE_SHA256}\n"
        f"UC2.EXE SHA-256: {hashlib.sha256(binary).hexdigest()}\n"
        f"CWSDPMI package SHA-256: {CWSDPMI_SHA256}\n"
        "\nBuilt on Linux with DJGPP GCC 12.2.0 and CMake.\n"
        "Get DJGPP and updates at https://www.delorie.com/djgpp/\n"
        "Extract UC2SRC.TGZ on a host with long filename support, then:\n"
        "  cmake -S . -B build-dos -DCMAKE_TOOLCHAIN_FILE=cmake/djgpp.cmake\n"
        "    -DUC2_VERSION_LABEL=3.0.0a13+dos1 -DUC2_BUILD_TESTS=OFF\n"
        "    -DCMAKE_BUILD_TYPE=Release -DDJGPP_ROOT=/usr/local/djgpp\n"
        "Join the three lines above into one command. Then run:\n"
        "  cmake --build build-dos\n"
        "The executable is build-dos/cli/uc2.exe.\n"
    ).encode("ascii")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            if name.endswith((".LSM", ".TXT")):
                data = data.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
            info = zipfile.ZipInfo(name, (2026, 11, 21, 0, 0, 0))
            info.create_system = 0
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    print(f"{args.output}: {hashlib.sha256(args.output.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
