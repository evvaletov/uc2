#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Opt-in large snapshot create/restore/maintenance measurement.

Default corpus: 1 GiB unique seeded random bytes, a 2.25 GiB file repeating
256 KiB of seeded random bytes, an empty directory and a small text file.
This is a 3.25 GiB logical collection, not 3.25 GiB of unique stored data.
The work directory and restored data are retained for inspection.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import random
import shutil
import sqlite3
import subprocess
import sys
import time


def digest(path):
    sha = hashlib.sha256()
    with path.open('rb') as source:
        for data in iter(lambda: source.read(1 << 20), b''):
            sha.update(data)
    return sha.hexdigest()


def inventory(root):
    result = {}
    for path in [root, *sorted(root.rglob('*'))]:
        info = path.stat()
        row = {'type': 'directory' if path.is_dir() else 'file',
               'mtime_ns': info.st_mtime_ns, 'mode': info.st_mode & 0o777}
        if path.is_file():
            row.update(sha256=digest(path), size=info.st_size)
        result[path.relative_to(root).as_posix()] = row
    return result


def stored(root):
    size = allocated = files = 0
    for path in root.rglob('*'):
        if path.is_file():
            info = path.stat()
            size += info.st_size
            allocated += getattr(info, 'st_blocks', 0) * 512
            files += 1
    return {'bytes': size, 'allocated_file_bytes': allocated, 'files': files}


def worker(request):
    if sys.platform == 'linux':
        import resource
        resource.setrlimit(resource.RLIMIT_AS, (2 << 30, 2 << 30))
    from uc2 import snapshot
    operation = request.pop('operation')
    started = time.monotonic()
    function = getattr(snapshot, operation)
    args = request.pop('args')
    try:
        result = function(*args, **request)
    except (ValueError, OSError, sqlite3.Error) as error:
        result = {'error': type(error).__name__, 'message': str(error)}
    peak = None
    if sys.platform == 'linux':
        # ru_maxrss can retain the launcher's pre-exec high-water mark.
        for line in Path('/proc/self/status').read_text().splitlines():
            if line.startswith('VmHWM:'):
                peak = int(line.split()[1]) * 1024
    print(json.dumps({'operation': operation, 'seconds': time.monotonic() - started,
                      'peak_rss_bytes': peak, 'result': result}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path)
    parser.add_argument('--unique-mib', type=int, default=1024)
    parser.add_argument('--repeat-mib', type=int, default=2304)
    parser.add_argument('--worker')
    args = parser.parse_args()
    if args.worker:
        worker(json.loads(args.worker))
        return
    if args.work is None or min(args.unique_mib, args.repeat_mib) <= 0:
        parser.error('supply a new --work directory and positive corpus sizes')
    root = args.work.resolve()
    root.mkdir()
    source = root / 'source'
    source.mkdir()
    (source / 'empty').mkdir()
    (source / 'note.txt').write_bytes(b'first version\n')
    rng = random.Random(20261114)
    with (source / 'unique.bin').open('wb') as output:
        for _ in range(args.unique_mib):
            output.write(rng.randbytes(1 << 20))
    block = random.Random(14112026).randbytes(256 << 10)
    with (source / 'repeated.bin').open('wb') as output:
        for _ in range(args.repeat_mib * 4):
            output.write(block)
    expected_first = inventory(source)
    report = {'corpus': {'unique_mib': args.unique_mib,
                         'repeated_file_mib': args.repeat_mib,
                         'repeated_block_bytes': len(block), 'seed': 20261114},
              'phases': [], 'restores': [], 'complete': False,
              'notes': 'One Linux run. Each operation uses a fresh process with a 2 GiB address-space cap. VmHWM measures resident memory in that process. Timings include verification performed by the API. Data is synthetic; repeats are disclosed.'}
    repo = root / 'repository'

    def save():
        (root / 'results.json').write_text(json.dumps(report, indent=2) + '\n')

    def run(phase_name, operation, *positional, fail=False, **kwargs):
        request = dict(operation=operation, args=list(map(str, positional)), **kwargs)
        result = subprocess.run([sys.executable, str(Path(__file__).resolve()),
                                 '--worker', json.dumps(request)],
                                capture_output=True, text=True, timeout=7200)
        if result.returncode:
            raise RuntimeError(f'{phase_name}: {result.returncode}: {result.stderr[-2000:]}')
        row = json.loads(result.stdout)
        row['label'] = phase_name
        report['phases'].append(row)
        save()
        print(json.dumps(row), flush=True)
        assert ('error' in row['result']) == fail, row
        return row['result']

    first = run('create loose v1', 'create', source, repo, label='large first')
    report['first'] = first
    report['loose_storage'] = stored(repo)
    run('verify loose v1', 'verify', repo, first['snapshot'])
    out = root / 'restore-v1'
    run('restore loose v1', 'restore', repo, first['snapshot'], out)
    assert inventory(out) == expected_first
    assert (out / 'empty').is_dir()
    report['restores'].append({'format': 1, 'bytes_paths_mtime_mode_equal': True})
    run('compact v1 to v2', 'compact', repo)
    report['packed_storage'] = stored(repo)

    with (source / 'unique.bin').open('r+b') as output:
        output.seek(min(123 << 20, (args.unique_mib << 20) // 2))
        output.write(random.Random(99).randbytes(1 << 20))
    with (source / 'repeated.bin').open('ab') as output:
        output.write(b'changed after the large repeated prefix\n')
    (source / 'note.txt').rename(source / 'renamed.txt')
    (source / 'added.txt').write_bytes(b'second version\n')
    expected_second = inventory(source)
    second = run('create edited packed v2', 'create', source, repo, label='large second')
    report['second'] = second
    out = root / 'restore-v2'
    run('restore edited packed v2', 'restore', repo, second['snapshot'], out)
    assert inventory(out) == expected_second
    assert (out / 'empty').is_dir()
    report['restores'].append({'format': 2, 'bytes_paths_mtime_mode_equal': True})
    replica = root / 'replica'
    shutil.copytree(repo, replica)
    with sqlite3.connect(repo / 'objects.sqlite3') as db:
        def first_repeated_chunk(snapshot_id):
            data = db.execute('SELECT data FROM manifests WHERE digest=?', (snapshot_id,)).fetchone()[0]
            manifest = json.loads(gzip.decompress(data))
            entry = next(e for e in manifest['entries'] if e['path'] == 'repeated.bin')
            return entry['chunks'][0][0]
        address = first_repeated_chunk(second['snapshot'])
        assert first_repeated_chunk(first['snapshot']) == address
        db.execute('UPDATE chunks SET data=? WHERE digest=?', (b'damaged', address))
    # The deliberately truncated archive belongs to both committed versions.
    for label, address in [('first', first['snapshot']), ('second', second['snapshot'])]:
        request = dict(operation='verify', args=[str(repo), address])
        result = subprocess.run([sys.executable, str(Path(__file__).resolve()),
                                 '--worker', json.dumps(request)],
                                capture_output=True, text=True, timeout=7200, check=True)
        row = json.loads(result.stdout)
        row['label'] = 'corruption detection ' + label
        report['phases'].append(row)
        assert row['result'] == {'error': 'BadArchiveError', 'message': 'Truncated'}, row
    report['corruption_detected'] = True
    repaired = run('repair from packed replica', 'repair', repo, replica)
    assert repaired['repaired_chunks'] == 1
    run('verify repaired first', 'verify', repo, first['snapshot'])
    run('verify repaired latest', 'verify', repo, second['snapshot'])
    run('prune old history', 'prune', repo, keep_last=1, keep=[second['snapshot']], apply=True)
    run('vacuum packed repository', 'compact', repo)
    out = root / 'restore-after-maintenance'
    run('restore after repair prune and vacuum', 'restore', repo, second['snapshot'], out)
    assert inventory(out) == expected_second
    report['restores'].append({'format': 2, 'after_maintenance': True,
                              'bytes_paths_mtime_mode_equal': True})
    report['retained'] = run('list retained history', 'snapshots', repo)
    assert [r['snapshot'] for r in report['retained']] == [second['snapshot']]
    report['final_storage'] = stored(repo)
    report['complete'] = True
    save()


if __name__ == '__main__':
    if not __debug__:
        raise RuntimeError('validation requires Python without -O or PYTHONOPTIMIZE')
    main()
