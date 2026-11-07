# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Launch the native executable shipped inside this wheel."""
import os
from pathlib import Path
import subprocess
import sys


def executable() -> Path:
    return Path(__file__).resolve().parent / "bin" / ("uc2.exe" if os.name == "nt" else "uc2")


def main() -> int:
    if sys.argv[1:2] == ["snapshot"]:
        from .snapshot import main as snapshot_main
        return snapshot_main(sys.argv[2:])
    binary = executable()
    if os.name == "posix":
        os.execv(str(binary), [str(binary), *sys.argv[1:]])
    return subprocess.call([str(binary), *sys.argv[1:]])
