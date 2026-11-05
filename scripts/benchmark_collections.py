#!/usr/bin/env python3
"""Reproducible collection benchmark; every restore is hash-checked."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import shutil
import subprocess
import tarfile
import time


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def manifest(root):
    return {p.relative_to(root).as_posix(): digest(p)
            for p in sorted(root.rglob('*')) if p.is_file()}


def fixtures(root, size):
    rng = random.Random(20261105)
    base = rng.randbytes(size)
    names = ['binary_versions', 'game_assets', 'source_versions', 'simulation', 'independent']
    for kind in names:
        folder = root / kind
        folder.mkdir(parents=True, exist_ok=False)
        for version in range(8):
            if kind == 'binary_versions':
                data = bytearray(base)
                for _ in range(8):
                    pos = rng.randrange(size - 32)
                    data[pos:pos+32] = rng.randbytes(32)
            elif kind == 'game_assets':
                # Asset packs reorder shared high-entropy objects and add one new object.
                assets = [base[i:i+32768] for i in range(0, size, 32768)]
                rng.shuffle(assets)
                assets[0] = rng.randbytes(len(assets[0]))
                data = b''.join(assets)
            elif kind == 'source_versions':
                lines = [f'int function_{i}(int value) {{ return value + {i % 97}; }}\n'
                         for i in range(max(1, size // 60))]
                lines[version] = f'/* revision {version} */\n'
                data = ''.join(lines).encode()
            elif kind == 'simulation':
                # Synthetic serialized checkpoints; repeated blocks with local updates.
                blocks = [base[i:i+4096] for i in range(0, size, 4096)]
                blocks[version % len(blocks)] = rng.randbytes(len(blocks[version % len(blocks)]))
                data = b''.join(blocks)
            else:
                data = rng.randbytes(size)
            item = folder / f'revision-{version:02}.dat'
            item.write_bytes(data)
            item.chmod(0o644)
            os.utime(item, (946684800, 946684800))
        yield kind, folder


def run(command, cwd, logs, tag, stdin=None, stdout=None):
    timing = logs / (tag + '.time')
    started = time.perf_counter()
    with (logs / (tag + '.stderr')).open('wb') as err:
        result = subprocess.run(['/usr/bin/time', '-f', '%e %M', '-o', str(timing),
                                 'timeout', '--kill-after=5s', '600s',
                                 *map(str, command)], cwd=cwd, stdin=stdin,
                                stdout=stdout or subprocess.DEVNULL, stderr=err)
    if result.returncode:
        raise RuntimeError(f'{tag} failed: see {logs / (tag + ".stderr")}')
    elapsed = time.perf_counter() - started
    _, rss = timing.read_text().split()
    return round(elapsed, 6), int(rss)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--uc2', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--size-kib', type=int, default=512)
    ap.add_argument('--repeat', type=int, default=3)
    ap.add_argument('--corpus', type=Path,
                    help='benchmark this existing flat directory instead of synthetic collections')
    args = ap.parse_args()
    uc2 = args.uc2.resolve()
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=False)
    seven = next((shutil.which(x) for x in ('7zz', '7z', '7za') if shutil.which(x)), None)
    if not seven or not shutil.which('zstd'):
        raise SystemExit('7zz/7z/7za and zstd are required; missing comparisons are not silently skipped')
    if args.size_kib < 32 or args.repeat < 1:
        raise SystemExit('size-kib must be >=32 and repeat must be >=1')
    metadata = {'date_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                'platform': platform.platform(), 'cpu': platform.processor(),
                'uc2_sha256': digest(uc2), 'seed': 20261105,
                'size_kib': args.size_kib, 'repeat': args.repeat,
                'corpus': (str(args.corpus.resolve()) if args.corpus else
                           'synthetic, eight revisions per collection; not real user data'),
                'rss_scope': 'per process peak RSS in KiB; tar materialization excluded equally',
                'versions': {}}
    for tool, cmd in [('7zip', [seven]), ('zstd', ['zstd', '--version']), ('uc2', [str(uc2), '-h'])]:
        metadata['versions'][tool] = subprocess.run(cmd, capture_output=True, text=True).stdout.splitlines()[:5]
    rows = []
    if args.corpus:
        folder = args.corpus.resolve()
        if not folder.is_dir() or not any(folder.iterdir()) or any(not p.is_file() for p in folder.iterdir()):
            raise SystemExit('--corpus must be a nonempty flat directory of regular files')
        collections = [(folder.name, folder)]
    else:
        collections = fixtures(root / 'inputs', args.size_kib * 1024)
    for kind, folder in collections:
        expected = manifest(folder)
        total = sum(p.stat().st_size for p in folder.iterdir())
        tar_path = root / f'{kind}.tar'
        with tarfile.open(tar_path, 'w', format=tarfile.USTAR_FORMAT) as tar:
            for p in sorted(folder.iterdir()):
                info = tar.gettarinfo(str(p), arcname=p.name)
                info.mtime = info.uid = info.gid = 0
                info.uname = info.gname = ''
                with p.open('rb') as f:
                    tar.addfile(info, f)
        (root / f'{kind}.manifest.json').write_text(json.dumps(expected, indent=2)+'\n')
        for trial in range(args.repeat):
            for method in ['uc2-files', 'tar-uc2', '7z', 'tar-zstd']:
                work = root / f'{kind}-{method}-{trial}'
                work.mkdir()
                archive = work / ('archive.7z' if method == '7z' else 'archive')
                restored = work / 'restored'
                restored.mkdir()
                with tar_path.open('rb') as stream:
                    if method == 'uc2-files':
                        cmd = [uc2, '-q', '-w', '-L', '4', archive, *sorted(expected)]
                        inp = None
                    elif method == 'tar-uc2':
                        cmd, inp = [uc2, '--ingest', archive], stream
                    elif method == '7z':
                        cmd, inp = [seven, 'a', '-t7z', '-mx=5', '-mmt=1', archive, *sorted(expected)], None
                    else:
                        cmd, inp = ['zstd', '-q', '-3', '-T1', '-o', archive], stream
                    enc, enc_rss = run(cmd, folder, work, 'create', stdin=inp)
                if method == 'uc2-files':
                    dec, dec_rss = run([uc2, '-q', '-d', restored, archive], root, work, 'restore')
                elif method == '7z':
                    dec, dec_rss = run([seven, 'x', '-y', '-o'+str(restored), archive], root, work, 'restore')
                else:
                    restored_tar = work / 'restored.tar'
                    with restored_tar.open('wb') as out:
                        cmd = ([uc2, '--ingest-restore', archive] if method == 'tar-uc2'
                               else ['zstd', '-q', '-d', '-c', archive])
                        dec, dec_rss = run(cmd, root, work, 'restore', stdout=out)
                    if digest(restored_tar) != digest(tar_path):
                        raise RuntimeError(f'{kind}/{method}: tar bytes changed')
                    with tarfile.open(restored_tar) as tar:
                        tar.extractall(restored, filter='data')
                if manifest(restored) != expected:
                    raise RuntimeError(f'{kind}/{method}: file manifest mismatch')
                row = dict(collection=kind, method=method, trial=trial,
                           input_bytes=total, archive_bytes=archive.stat().st_size,
                           compress_seconds=enc, restore_seconds=dec,
                           compress_peak_kib=enc_rss, restore_peak_kib=dec_rss,
                           verified=True)
                rows.append(row)
                print(json.dumps(row), flush=True)
                (root/'results.json').write_text(json.dumps({'metadata':metadata, 'results':rows}, indent=2)+'\n')
    with (root/'results.csv').open('w') as f:
        writer=csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)


if __name__ == '__main__':
    main()
