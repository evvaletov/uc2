#!/usr/bin/env python3
"""Package the tested, Python-independent CLI carried by a platform wheel."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument("--wheels", type=Path, required=True)
ap.add_argument("--platform", required=True,
                choices=["linux-x86_64", "macos-x86_64", "macos-arm64", "windows-x86_64"])
ap.add_argument("--output", type=Path, required=True)
args = ap.parse_args()
repository = Path(__file__).resolve().parents[1]
wheel = sorted(args.wheels.glob("*.whl"))[0]
with zipfile.ZipFile(wheel) as archive:
    metadata_name = next(n for n in archive.namelist() if n.endswith(".dist-info/METADATA"))
    from email.parser import Parser
    metadata = Parser().parsestr(archive.read(metadata_name).decode())
    version = metadata["Version"]
    member = "uc2/bin/uc2.exe" if args.platform.startswith("windows-") else "uc2/bin/uc2"
    binary = archive.read(member)
name = f"uc2-{version}-{args.platform}"
args.output.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="uc2-cli-package-") as tmp:
    root = Path(tmp) / name
    (root / "bin").mkdir(parents=True)
    executable = root / "bin" / Path(member).name
    executable.write_bytes(binary)
    executable.chmod(0o755)
    # This is the exact binary tested inside the wheel, with no Python dependency.
    result = subprocess.run([str(executable), "-h"], check=True, capture_output=True)
    if not result.stdout.startswith(f"UC2 {version}".encode()):
        raise SystemExit("CLI and wheel versions disagree")
    for item in ("LICENSE", "CREDITS.md"):
        shutil.copyfile(repository/item, root/item)
    shutil.copytree(repository/"LICENSES", root/"LICENSES")
    shutil.copyfile(repository/"docs/license-audit.md", root/"license-audit.md")
    shutil.copyfile(repository/"cli/uc2.1", root/"uc2.1")
    (root/"README.txt").write_text(f"""UltraCompressor II {version}, {args.platform}

Run bin/{executable.name} -h for usage. This executable does not require Python.
Examples:
  bin/{executable.name} -l archive.uc2
  bin/{executable.name} -t archive.uc2
  bin/{executable.name} -d restored archive.uc2
  bin/{executable.name} -w backup.uc2 files...

Create the extraction destination before using -d. This is an alpha release.
Turbo/method 80 and multi-volume archives remain unsupported. Compression
levels 6-9 require the new UC2 reader; levels 2-5 use the legacy Huffman format.

The corresponding source distribution, uc2-{version}.tar.gz, accompanies the
release at https://github.com/evvaletov/uc2/releases . See the included license
texts and per-file audit for the GPL/LGPL components and integration licenses.
""")
    (root/"build-info.json").write_text(json.dumps({
        "version": version, "platform": args.platform, "source_wheel": wheel.name,
        "executable_sha256": hashlib.sha256(binary).hexdigest(),
    }, indent=2)+"\n")
    if args.platform.startswith("windows-"):
        output = args.output/(name+".zip")
        with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(root.rglob("*")):
                if path.is_file(): archive.write(path, path.relative_to(root.parent))
    else:
        output = args.output/(name+".tar.gz")
        with tarfile.open(output, "w:gz") as archive:
            archive.add(root, arcname=name)
print(output)
