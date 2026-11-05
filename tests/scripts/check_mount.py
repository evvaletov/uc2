#!/usr/bin/env python3
"""Exercise a real Linux FUSE mount using the installed uc2 package."""
from concurrent.futures import ThreadPoolExecutor
import errno
import json
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys
import tempfile
import time

from uc2._cli import executable


def main():
    if sys.platform != "linux" or not Path("/dev/fuse").exists():
        raise SystemExit("A Linux host with /dev/fuse is required; test was not run")
    unmount = shutil.which("fusermount3") or shutil.which("fusermount")
    if not unmount:
        raise SystemExit("fusermount is required")
    with tempfile.TemporaryDirectory(prefix="uc2-fuse-check-") as temporary:
        root = Path(temporary)
        source, mounted = root / "source", root / "mounted"
        (source / "nested").mkdir(parents=True)
        mounted.mkdir()
        files = {"one.bin": random.Random(82).randbytes(700_000),
                 "nested/two.bin": random.Random(83).randbytes(700_000),
                 "one.bin;1": b"literal suffix", "empty": b""}
        for name, data in files.items():
            (source / name).write_bytes(data)
        archive = root / "archive.uc2"
        subprocess.run([str(executable()), "-qw", str(archive), "one.bin", "one.bin",
                        "one.bin;1", "nested", "empty"], cwd=source, check=True,
                       capture_output=True)
        with (root / "mount.log").open("w+") as log:
            process = subprocess.Popen([sys.executable, "-m", "uc2._mount", str(archive),
                                        str(mounted), "--cache-mib", "1", "--cache-entries", "1"],
                                       stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 15
                while not os.path.ismount(mounted):
                    if process.poll() is not None or time.monotonic() > deadline:
                        log.seek(0)
                        raise RuntimeError("mount failed: " + log.read())
                    time.sleep(0.05)
                assert sorted(p.name for p in mounted.iterdir()) == ["files", "versions"]
                for name, data in files.items():
                    assert (mounted / "files" / name).read_bytes() == data
                assert (mounted / "versions/1/one.bin").read_bytes() == files["one.bin"]
                def read(i):
                    name = "one.bin" if i % 2 else "nested/two.bin"
                    with (mounted / "files" / name).open("rb") as stream:
                        stream.seek(i * 997)
                        return stream.read(4000) == files[name][i*997:i*997+4000]
                with ThreadPoolExecutor(max_workers=4) as pool:
                    assert all(pool.map(read, range(16)))
                try:
                    (mounted / "files/new").write_bytes(b"must fail")
                except OSError as error:
                    assert error.errno in (errno.EROFS, errno.EACCES)
                else:
                    raise AssertionError("mount allowed a write")
                print(json.dumps(dict(real_fuse_mount=True, files=len(files),
                                      version_read=True, concurrent_reads=16,
                                      cache_mib=1, cache_entries=1, writes_rejected=True)))
            finally:
                if os.path.ismount(mounted):
                    subprocess.run([unmount, "-u", str(mounted)], check=True, timeout=10)
                if process.poll() is None:
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.terminate()
                        process.wait(timeout=10)


if __name__ == "__main__":
    main()
