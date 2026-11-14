#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Opt-in POSIX process-kill and real ENOSPC snapshot checks.

Run with the installed uc2 package. --disk-full requires --work to be an
otherwise empty, private 32 MiB filesystem (for example a tmpfs in a private
mount namespace). It deliberately fills that filesystem, never its parent.
Process-kill fixtures and receipts are retained. Disk-full fixtures are removed
inside the private mount after verification; redirect stdout to retain results.
"""
import argparse
import errno
import json
import os
from pathlib import Path
import random
import select
import shutil
import signal
import sqlite3
import subprocess
import sys

from uc2 import snapshot as s


def stop():
    print('READY', flush=True)
    os.kill(os.getpid(), signal.SIGSTOP)
    raise AssertionError('test worker must be killed')


def child(case, root):
    repo = root / 'repo'
    source = root / 'source'
    address = (root / 'first').read_text()
    if case in ('create-v1', 'create-v2'):
        original = s._save_manifest
        def save(*args):
            original(*args)
            if case == 'create-v2':
                stop()  # SQL insertion has not committed.
        if case == 'create-v2':
            s._save_manifest = save
        else:
            original_store = s._store
            def store(*args):
                result = original_store(*args)
                stop()  # A loose chunk is durable, no manifest published.
                return result
            s._store = store
        s.create(source, repo)
    elif case.startswith('compact-'):
        original = s._replace_file
        def replace(path, data):
            if path.name == 'config.json' and case == 'compact-before-config':
                stop()
            original(path, data)
            if path.name == 'config.json':
                stop()
        s._replace_file = replace
        s.compact(repo)
    elif case == 'prune-v1':
        original = Path.unlink
        def unlink(path, *args, **kwargs):
            result = original(path, *args, **kwargs)
            if path.parent.name == 'snapshots':
                stop()
            return result
        Path.unlink = unlink
        s.prune(repo, keep_last=1, apply=True)
    elif case in ('prune-v2', 'repair-v2'):
        original = sqlite3.connect
        class Connection(sqlite3.Connection):
            def commit(self):
                if self.target:
                    stop()
                return super().commit()
        def connect(path, *args, **kwargs):
            connection = original(path, *args, factory=Connection, **kwargs)
            filename = connection.execute('PRAGMA database_list').fetchone()[2]
            connection.target = Path(filename).parent == repo
            return connection
        sqlite3.connect = connect
        if case == 'prune-v2':
            s.prune(repo, keep_last=1, apply=True)
        else:
            s.repair(repo, root / 'replica')
    else:
        original = Path.rename
        def rename(path, target):
            if path.name.startswith('.uc2-restore-'):
                if case == 'restore-before-publish':
                    stop()
                result = original(path, target)
                stop()
                return result
            return original(path, target)
        Path.rename = rename
        s.restore(repo, address, root / 'restored')


def prepare(root, packed=False, second=False):
    root.mkdir()
    source = root / 'source'
    source.mkdir()
    (source / 'data').write_bytes(random.Random(12).randbytes(1 << 20))
    repo = root / 'repo'
    first = s.create(source, repo)['snapshot']
    (root / 'first').write_text(first)
    if packed:
        s.compact(repo)
    if second:
        (source / 'data').write_bytes(b'second version\n')
        s.create(source, repo)
    return source, repo, first


def corrupt(repo):
    with sqlite3.connect(repo / 'objects.sqlite3') as db:
        address = db.execute('SELECT digest FROM chunks LIMIT 1').fetchone()[0]
        db.execute('UPDATE chunks SET data=? WHERE digest=?', (b'damaged', address))


def kill_checks(work):
    results = []
    cases = ('create-v1', 'create-v2', 'compact-before-config', 'compact-after-config',
             'prune-v1', 'prune-v2', 'repair-v2', 'restore-before-publish',
             'restore-after-publish')
    for case in cases:
        root = work / case
        source, repo, first = prepare(root, packed=case.endswith('v2'), second=case.startswith('prune'))
        previous = [row['snapshot'] for row in s.snapshots(repo)]
        retained = (s.prune(repo, keep_last=1)['retained_snapshots']
                    if case.startswith('prune') else previous)
        if case.startswith('create'):
            (source / 'data').write_bytes(random.Random(13).randbytes(128 << 10))
        if case == 'repair-v2':
            shutil.copytree(repo, root / 'replica')
            corrupt(repo)
        process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()),
                                    '--child', case, '--work', str(root)], stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True)
        try:
            ready, _, _ = select.select([process.stdout], [], [], 60)
            if not ready or process.stdout.readline().strip() != 'READY':
                raise AssertionError(f'{case}: child did not reach the checkpoint')
            process.kill()
            assert process.wait(timeout=10) == -signal.SIGKILL
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            diagnostic = process.stderr.read()
            if diagnostic:
                print(diagnostic, file=sys.stderr, flush=True)
            process.stdout.close()
            process.stderr.close()
        # Only remove sentinels owned by our confirmed dead test child.
        locks = []
        for owner in (repo, root / 'replica'):
            lock = owner / '.writer-lock'
            if lock.exists():
                assert int(lock.read_text()) == process.pid
                lock.unlink()
                locks.append(owner.name)
        current = [row['snapshot'] for row in s.snapshots(repo)]
        expected = retained if case == 'prune-v1' else previous
        assert set(current) == set(expected), (case, current, expected)
        if case == 'repair-v2':
            try:
                s.verify(repo, first)
            except (ValueError, OSError):
                pass
            else:
                raise AssertionError('uncommitted repair survived')
            s.repair(repo, root / 'replica')
        for address in current:
            s.verify(repo, address)
        final_ids = set(retained)
        if case.startswith('create'):
            final_ids.add(s.create(source, repo)['snapshot'])
        elif case.startswith('compact'):
            s.compact(repo)
            assert not list((repo / 'chunks').rglob('*.uc2'))
        elif case.startswith('prune'):
            s.prune(repo, keep_last=1, apply=True)
        elif case.startswith('restore'):
            destination = root / 'restored'
            if case == 'restore-before-publish':
                # POSIX has reserved an empty name. Preserve the abandoned
                # stage as evidence and explicitly remove only this empty dir.
                assert destination.is_dir() and not list(destination.iterdir())
                destination.rmdir()
                s.restore(repo, first, destination)
            assert (destination / 'data').read_bytes() == (source / 'data').read_bytes()
        assert {row['snapshot'] for row in s.snapshots(repo)} == final_ids
        for address in final_ids:
            s.verify(repo, address)
        results.append(dict(case=case, killed=True, removed_owned_locks=locks, recovered=True))
        (work / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        print(json.dumps(results[-1]), flush=True)


def disk_checks(work):
    # Require a dedicated mount with a small capacity; never fill a shared disk.
    if not os.path.ismount(work) or list(work.iterdir()):
        raise ValueError('disk-full checks require an empty dedicated mount')
    stats = os.statvfs(work)
    if not 16 << 20 <= stats.f_blocks * stats.f_frsize <= 64 << 20:
        raise ValueError('disk-full checks require a 16–64 MiB private filesystem')
    results = []
    for case in ('create-v1', 'create-v2', 'compact', 'restore', 'repair-v2'):
        root = work / case
        source, repo, first = prepare(root, packed=case.endswith('v2'))
        if case.startswith('create'):
            (source / 'data').write_bytes(random.Random(14).randbytes(1 << 20))
        if case == 'repair-v2':
            shutil.copytree(repo, root / 'replica')
            corrupt(repo)
        filler = work / 'filler'
        try:
            with filler.open('wb') as output:
                stats = os.statvfs(work)
                os.posix_fallocate(output.fileno(), 0, stats.f_bavail * stats.f_frsize - 8192)
        except BaseException:
            filler.unlink(missing_ok=True)
            raise
        try:
            if case.startswith('create'):
                s.create(source, repo)
            elif case == 'compact':
                s.compact(repo)
            elif case == 'restore':
                s.restore(repo, first, root / 'restored')
            else:
                s.repair(repo, root / 'replica')
        except (OSError, sqlite3.Error) as error:
            assert (getattr(error, 'errno', None) == errno.ENOSPC
                    or getattr(error, 'sqlite_errorcode', None) == sqlite3.SQLITE_FULL), repr(error)
            caught = type(error).__name__
        else:
            raise AssertionError(f'{case}: expected real ENOSPC')
        finally:
            filler.unlink()
        assert not (repo / '.writer-lock').exists()
        assert len(s.snapshots(repo)) == 1
        if case == 'repair-v2':
            s.repair(repo, root / 'replica')
        s.verify(repo, first)
        final_ids = {first}
        if case.startswith('create'):
            final_ids.add(s.create(source, repo)['snapshot'])
        elif case == 'compact':
            s.compact(repo)
        elif case == 'restore':
            assert not (root / 'restored').exists()
            assert not list(root.glob('.uc2-restore-*'))
            s.restore(repo, first, root / 'restored')
            assert (root / 'restored/data').read_bytes() == (source / 'data').read_bytes()
        assert {row['snapshot'] for row in s.snapshots(repo)} == final_ids
        for address in final_ids:
            s.verify(repo, address)
        results.append(dict(case=case, error=caught, recovered=True))
        print(json.dumps(results[-1]), flush=True)
        shutil.rmtree(root)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', required=True, type=Path)
    parser.add_argument('--child')
    parser.add_argument('--disk-full', action='store_true')
    args = parser.parse_args()
    if args.child:
        child(args.child, args.work.resolve())
    elif args.disk_full:
        print(json.dumps({'disk_full_results': disk_checks(args.work.resolve())}))
    else:
        args.work.mkdir()
        kill_checks(args.work.resolve())


if __name__ == '__main__':
    if not __debug__:
        raise RuntimeError('validation requires Python without -O or PYTHONOPTIMIZE')
    main()
