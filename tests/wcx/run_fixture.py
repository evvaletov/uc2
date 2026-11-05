#!/usr/bin/env python3
"""Run the WCX DLL host on Windows or in an isolated Wine container."""
import argparse
import os
from pathlib import Path
import random
import shutil
import subprocess

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--uc2', type=Path, required=True, help='CLI executable for this host OS')
ap.add_argument('--plugin', type=Path, required=True)
ap.add_argument('--host', type=Path, required=True)
ap.add_argument('--work', type=Path, required=True, help='new fixture directory')
ap.add_argument('--wine-image', help='Docker image built from tests/wcx/Dockerfile')
args = ap.parse_args()
root = args.work.resolve()
root.mkdir(parents=True, exist_ok=False)
source = root/'input'
(source/'sub dir').mkdir(parents=True)
(source/'sub dir/long filename.txt').write_bytes(b'WCX round trip\n' * 1000)
(source/'empty.dat').write_bytes(b'')
(source/'random.bin').write_bytes(random.Random(301).randbytes(150000))
plugin = root/args.plugin.name
host = root/'test_wcx.exe'
shutil.copyfile(args.plugin, plugin)
shutil.copyfile(args.host, host)


def run_host(arguments):
    paths = [host, *arguments]
    def path_arg(p):
        if isinstance(p, Path):
            return 'Z:\\work\\' + p.relative_to(root).as_posix().replace('/', '\\')
        return p
    if args.wine_image:
        command = ['docker', 'run', '--rm', '--network=none',
                   '--user', f'{os.getuid()}:{os.getgid()}',
                   '-e', 'WINEPREFIX=/work/wine-prefix', '-e', 'HOME=/work',
                   '-e', 'XDG_CACHE_HOME=/work/cache',
                   '-v', f'{root}:/work:rw', args.wine_image,
                   *map(path_arg, paths)]
    else:
        command = [str(p) for p in paths]
    subprocess.run(command, check=True, timeout=120)


for level in (4, 6):
    archive = root/f'архив-{level}.uc2'
    created = root/'fixture.uc2'
    subprocess.run([str(args.uc2.resolve()), '-qw', '-L', str(level), str(created),
                    'sub dir', 'empty.dat', 'random.bin'], cwd=source, check=True)
    created.rename(archive)
    out = root/f'out-{level}'
    out.mkdir()
    run_host([plugin, archive, source, out])

(source/'random.bin;1').write_bytes(b'literal version-looking name')
ambiguous = root/'ambiguous.uc2'
subprocess.run([str(args.uc2.resolve()), '-qw', str(ambiguous),
                'random.bin', 'random.bin', 'random.bin;1'], cwd=source, check=True)
run_host(['--reject', plugin, ambiguous])
