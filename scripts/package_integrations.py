#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Package MC source helpers or both tested Windows WCX binaries."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tarfile
import tempfile
import tomllib
import zipfile

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument("kind", choices=["mc", "wcx"])
ap.add_argument("--binaries", type=Path, help="directory containing uc2.wcx and uc2.wcx64")
ap.add_argument("--output", type=Path, required=True)
args = ap.parse_args()
repository = Path(__file__).resolve().parents[1]
version = tomllib.loads((repository / "pyproject.toml").read_text())["project"]["version"]
args.output.mkdir(parents=True, exist_ok=True)
name = f"uc2-{args.kind}-{version}"
with tempfile.TemporaryDirectory(prefix="uc2-integration-package-") as tmp:
    root = Path(tmp) / name
    root.mkdir()
    for item in ("LICENSE", "CREDITS.md"):
        shutil.copyfile(repository / item, root / item)
    shutil.copytree(repository / "LICENSES", root / "LICENSES")
    shutil.copyfile(repository / "docs/license-audit.md", root / "license-audit.md")
    if args.kind == "mc":
        for item in ("uc2", "install.py", "mc.ext.ini", "README.md"):
            shutil.copy2(repository / "contrib/mc" / item, root / item)
    else:
        if not args.binaries:
            ap.error("wcx packaging requires --binaries")
        for item, machine in (("uc2.wcx", 0x14C), ("uc2.wcx64", 0x8664)):
            source = args.binaries / item
            data = source.read_bytes()
            offset = int.from_bytes(data[0x3C:0x40], "little")
            if data[:2] != b"MZ" or data[offset:offset+4] != b"PE\0\0" or int.from_bytes(data[offset+4:offset+6], "little") != machine:
                raise SystemExit(f"wrong architecture or invalid DLL: {source}")
            shutil.copyfile(source, root / item)
        for item in ("pluginst.inf", "README.md"):
            shutil.copyfile(repository / "contrib/totalcommander" / item, root / item)
    (root / "SOURCE.txt").write_text(f"UC2 {version}\nCorresponding source: uc2-{version}.tar.gz\n"
                                    "https://github.com/evvaletov/uc2/releases\n")
    manifest = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in sorted(root.rglob("*")) if p.is_file()}
    (root / "SHA256SUMS").write_text("".join(f"{v}  {k}\n" for k, v in manifest.items()))
    if args.kind == "wcx":
        output = args.output / (name + ".zip")
        with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(root.rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(root))  # pluginst.inf must be at ZIP root
    else:
        output = args.output / (name + ".tar.gz")
        with tarfile.open(output, "w:gz") as archive:
            archive.add(root, arcname=name)
print(output)
