#!/usr/bin/env python3
"""Run the WCX DLL host on Windows or in an isolated Wine container."""
import argparse
import io
import os
from pathlib import Path
import random
import shutil
import subprocess
import tarfile
import time

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--uc2', type=Path, required=True, help='CLI executable for this host OS')
ap.add_argument('--plugin', type=Path, required=True)
ap.add_argument('--host', type=Path, required=True)
ap.add_argument('--work', type=Path, required=True, help='new fixture directory')
ap.add_argument('--wine-image', help='Docker image built from tests/wcx/Dockerfile')
ap.add_argument('--metadata-count', type=int, default=10000, help='empty-file listing regression size')
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


def run_host(arguments, *, capture=False):
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
    return subprocess.run(command, check=True, timeout=120, capture_output=capture)


def tar_archive(name, entries):
    archive = root / name
    stream = io.BytesIO()
    with tarfile.open(fileobj=stream, mode='w') as tar:
        for path, data in entries:
            info = tarfile.TarInfo(path)
            info.size = len(data)
            tar.addfile(info, io.BytesIO(data))
    subprocess.run([str(args.uc2.resolve()), '--ingest-tar', str(archive)],
                   input=stream.getvalue(), check=True, capture_output=True)
    return archive


for level, chunked in ((4, False), (6, False), (4, True), (6, True)):
    archive = root/f'архив-{level}-{chunked}.uc2'
    created = root/'fixture.uc2'
    subprocess.run([str(args.uc2.resolve()), *(['--chunked'] if chunked else []), '-qw', '-L', str(level), str(created),
                    'sub dir', 'empty.dat', 'random.bin'], cwd=source, check=True)
    created.rename(archive)
    out = root/f'out-{level}-{chunked}'
    out.mkdir()
    run_host([plugin, archive, source, out])

versions = [('version.bin', b'old'), ('nested/deep/notes.txt', b'nested old'),
            ('version.bin', b'middle'), ('literal;1', b'literal suffix'),
            ('version.bin', b'new'), ('nested/deep/notes.txt', b'nested new')]
version_archive = tar_archive('versions.uc2', versions)
expected = root / 'expected-versions'
(expected / 'nested/deep').mkdir(parents=True)
names = ['version.bin;2', 'nested/deep/notes.txt;1', 'version.bin;1',
         'literal;1', 'version.bin', 'nested/deep/notes.txt']
for name, (_, data) in zip(names, versions):
    (expected / name).write_bytes(data)
version_output = root / 'version-output'
version_output.mkdir()
run_host([plugin, version_archive, expected, version_output])
listed = run_host(['--list', plugin, version_archive], capture=True).stdout.decode('utf-8').splitlines()
assert listed == ['nested', 'nested\\deep', *[name.replace('/', '\\') for name in names]], listed

case_collision = tar_archive('case-collision.uc2', [('Mixed.txt', b'a'), ('mixed.txt', b'b')])
run_host(['--reject', plugin, case_collision])

if not 1 <= args.metadata_count <= 90000:
    raise ValueError('--metadata-count must be between 1 and 90000')
metadata = [(f'folder{i % 32:02d}/inner/file{i:05d}', b'') for i in range(args.metadata_count)]
large = tar_archive('metadata.uc2', metadata)
start = time.monotonic()
listed = run_host(['--list', plugin, large], capture=True).stdout.decode('utf-8').splitlines()
seconds = time.monotonic() - start
directories = [name for i in range(min(32, args.metadata_count))
               for name in (f'folder{i:02d}', f'folder{i:02d}\\inner')]
assert listed == directories + [path.replace('/', '\\') for path, _ in metadata]
print(f'WCX metadata: {len(listed)} entries in archive order, {seconds:.3f} seconds including host startup')

(source/'random.bin;1').write_bytes(b'literal version-looking name')
ambiguous = root/'ambiguous.uc2'
subprocess.run([str(args.uc2.resolve()), '-qw', str(ambiguous),
                'random.bin', 'random.bin', 'random.bin;1'], cwd=source, check=True)
run_host(['--reject', plugin, ambiguous])
