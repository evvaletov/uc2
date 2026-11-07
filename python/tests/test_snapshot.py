"""Restorable versions, deduplication, corruption and failed publication."""
import hashlib
import json
import os
from pathlib import Path
import random
import subprocess
import sys

import pytest
from uc2 import snapshot as s


def tree(root):
    return {p.relative_to(root).as_posix(): p.read_bytes() for p in root.rglob('*') if p.is_file()}


def manifest(repo, address):
    return json.loads((repo / 'snapshots' / (address + '.json')).read_bytes())


def forged(repo, value):
    data = json.dumps(value).encode()
    digest = hashlib.sha256(data).hexdigest()
    (repo / 'snapshots' / (digest + '.json')).write_bytes(data)
    return digest


def test_incremental_versions_and_recovery(tmp_path):
    src, repo = tmp_path/'src', tmp_path/'repo'
    src.mkdir(); (src/'emptydir').mkdir(); (src/'empty').touch()
    data = random.Random(77).randbytes(512*1024)
    (src/'large.bin').write_bytes(data)
    (src/'duplicate.bin').write_bytes(data)
    (src/'old.txt').write_bytes(b'first version\n')
    os.utime(src/'old.txt', ns=(1600000000123456789,)*2)
    initial = tree(src)
    first = s.create(src, repo, label='first')
    assert first['new_chunks'] < len(manifest(repo, first['snapshot'])['entries'][0].get('chunks', [])) + 100
    same = s.create(src, repo)
    assert same['new_chunks'] == same['new_chunk_bytes'] == 0
    (src/'old.txt').unlink()
    (src/'new.txt').write_bytes(b'second version\n')
    (src/'large.bin').write_bytes(data[:100000] + b'insertion' * 127 + data[100000:])
    second_tree = tree(src)
    second = s.create(src, repo, label='second')
    assert second['new_chunk_bytes'] < first['new_chunk_bytes'] // 4
    for result, expected in [(first, initial), (second, second_tree)]:
        address = result['snapshot']
        assert s.verify(repo, address)['verified_files'] == len(expected)
        out = tmp_path/address
        s.restore(repo, address, out)
        assert tree(out) == expected
        assert (out/'emptydir').is_dir()
    assert (tmp_path/first['snapshot']/'old.txt').stat().st_mtime_ns // 10**6 == 1600000000123
    assert len(s.snapshots(repo)) == 3
    # Damage a shared chunk. A failed restore must expose no partial directory.
    chunk = next((repo/'chunks').rglob('*.uc2'))
    original = chunk.read_bytes()
    chunk.write_bytes(b'damaged')
    with pytest.raises((ValueError, OSError)):
        s.restore(repo, first['snapshot'], tmp_path/'failed')
    assert not (tmp_path/'failed').exists()
    assert not list(tmp_path.glob('.uc2-restore-*'))
    chunk.write_bytes(original)
    s.restore(repo, first['snapshot'], tmp_path/'recovered')
    assert tree(tmp_path/'recovered') == initial


def test_empty_limits_lock_and_cli(tmp_path):
    src, repo = tmp_path/'src', tmp_path/'repo'
    src.mkdir()
    result = s.create(src, repo, max_total_size=0)
    assert s.verify(repo, result['snapshot'], max_total_size=0)['verified_files'] == 0
    out = subprocess.check_output([sys.executable, '-m', 'uc2', 'snapshot', 'list', str(repo)])
    assert json.loads(out)[0]['snapshot'] == result['snapshot']
    (src/'a').write_bytes(b'a')
    with pytest.raises(ValueError, match='total size'):
        s.create(src, repo, max_total_size=0)
    assert len(s.snapshots(repo)) == 1
    (repo/'.writer-lock').write_text('active')
    with pytest.raises(FileExistsError):
        s.create(src, repo)
    assert (repo/'.writer-lock').read_text() == 'active'
    with pytest.raises(ValueError, match='outside'):
        s.create(src, src/'inside')
    with pytest.raises(FileExistsError):
        s.restore(repo, result['snapshot'], src)


@pytest.mark.parametrize('name', ['../escape', '/absolute', 'C:drive', 'con', 'a.', 'a\\b', 'a//b', 'CONIN$', 'ConOut$.log', 'CON .txt', 'aux  .foo'])
def test_unsafe_manifest_paths(tmp_path, name):
    src, repo = tmp_path/'src', tmp_path/'repo'
    src.mkdir(); (src/'a').write_bytes(b'payload')
    result = s.create(src, repo)
    m = manifest(repo, result['snapshot'])
    m['entries'][0]['path'] = name
    address = forged(repo, m)
    with pytest.raises(ValueError):
        s.restore(repo, address, tmp_path/'out')
    assert not (tmp_path/'out').exists()


def test_manifest_and_chunk_integrity(tmp_path):
    src, repo = tmp_path/'src', tmp_path/'repo'
    src.mkdir(); (src/'a').write_bytes(b'payload')
    result = s.create(src, repo)
    address = result['snapshot']
    m = manifest(repo, address)
    m['entries'][0]['sha256'] = '0'*64
    with pytest.raises(ValueError, match='file SHA'):
        s.restore(repo, forged(repo, m), tmp_path/'out')
    path = repo/'snapshots'/(address+'.json')
    path.write_bytes(path.read_bytes()+b' ')
    with pytest.raises(ValueError, match='manifest SHA'):
        s.verify(repo, address)


def test_source_change_and_failed_commit(tmp_path, monkeypatch):
    src, repo = tmp_path/'src', tmp_path/'repo'
    src.mkdir(); file = src/'a'; file.write_bytes(b'a'*100)
    store = s._store
    def changing(*args):
        result = store(*args)
        file.write_bytes(b'b'*100)
        return result
    monkeypatch.setattr(s, '_store', changing)
    with pytest.raises(ValueError, match='source changed'):
        s.create(src, repo)
    assert not list((repo/'snapshots').iterdir())
    assert not (repo/'.writer-lock').exists()


def test_path_and_handle_ctime_can_differ(tmp_path, monkeypatch):
    # CPython on Windows can report creation time from lstat, but change
    # time from fstat. Reproduce the discrepancy independently of the OS.
    original = os.fstat
    class HandleStat:
        def __init__(self, fd):
            self.value = original(fd)
            self.st_ctime_ns = self.value.st_ctime_ns + 10**9
        def __getattr__(self, name):
            return getattr(self.value, name)
    monkeypatch.setattr(s.os, 'fstat', HandleStat)
    source, repo = tmp_path/'source', tmp_path/'repo'
    source.mkdir(); (source/'data').write_bytes(b'updated file')
    result = s.create(source, repo)
    s.restore(repo, result['snapshot'], tmp_path/'restored')
    assert tree(tmp_path/'restored') == tree(source)


@pytest.mark.skipif(os.name == 'nt', reason='requires POSIX symlink/FIFO privileges')
def test_symlinks_special_files_and_collisions(tmp_path):
    src, repo = tmp_path/'src', tmp_path/'repo'
    src.mkdir(); (src/'link').symlink_to(tmp_path, target_is_directory=True)
    with pytest.raises(ValueError, match='unsupported'):
        s.create(src, repo)
    (src/'link').unlink(); os.mkfifo(src/'fifo')
    with pytest.raises(ValueError, match='unsupported'):
        s.create(src, repo)
    (src/'fifo').unlink()
    (src/'A').write_bytes(b'a'); (src/'a').write_bytes(b'b')
    if (src/'A').stat().st_ino == (src/'a').stat().st_ino:
        pytest.skip('filesystem cannot represent a case collision')
    with pytest.raises(ValueError, match='collision'):
        s.create(src, repo)


def test_filesystem_without_hardlinks_and_repo_overlap(tmp_path, monkeypatch):
    import errno
    def unavailable(*args):
        raise OSError(errno.ENOTSUP, 'no hardlinks')
    monkeypatch.setattr(s.os, 'link', unavailable)
    src, repo = tmp_path/'src', tmp_path/'repo'
    src.mkdir(); (src/'a').write_bytes(b'portable storage')
    result = s.create(src, repo)
    s.restore(repo, result['snapshot'], tmp_path/'out')
    assert tree(tmp_path/'out') == tree(src)
    with pytest.raises(ValueError, match='outside'):
        s.create(repo/'chunks', repo)


def test_identical_manifest_is_idempotent(tmp_path, monkeypatch):
    from datetime import datetime, timezone
    class Clock:
        @staticmethod
        def now(tz):
            return datetime(2026, 11, 7, tzinfo=timezone.utc)
    monkeypatch.setattr(s, 'datetime', Clock)
    source, repo = tmp_path/'src', tmp_path/'repo'
    source.mkdir()
    first = s.create(source, repo)
    second = s.create(source, repo)
    assert first['snapshot'] == second['snapshot']
    assert second['growth_bytes'] == second['new_manifest_bytes'] == 0
    assert len(s.snapshots(repo)) == 1


@pytest.mark.skipif(os.name == 'nt', reason='POSIX directory permissions')
def test_failed_publish_cleans_restricted_directories(tmp_path, monkeypatch):
    source, repo = tmp_path/'src', tmp_path/'repo'
    (source/'d'/'nested').mkdir(parents=True)
    (source/'d'/'nested'/'data').write_bytes(b'payload')
    result = s.create(source, repo)
    m = manifest(repo, result['snapshot'])
    for entry in m['entries']:
        entry['mode'] = 0
    address = forged(repo, m)
    def fail(*args):
        raise OSError('injected publication failure')
    monkeypatch.setattr(s.Path, 'rename', fail)
    with pytest.raises(OSError, match='injected'):
        s.restore(repo, address, tmp_path/'out')
    assert not (tmp_path/'out').exists()
    assert not list(tmp_path.glob('.uc2-restore-*'))


@pytest.mark.parametrize('code', [1, 50])
def test_windows_no_hardlink_error_mapping(tmp_path, monkeypatch, code):
    import errno
    def unavailable(*args):
        error = OSError(errno.EINVAL, 'Win32 hardlinks unsupported')
        error.winerror = code
        raise error
    monkeypatch.setattr(s.os, 'link', unavailable)
    source = tmp_path/'source'; source.mkdir()
    result = s.create(source, tmp_path/'repo')
    assert s.verify(tmp_path/'repo', result['snapshot'])['verified_files'] == 0
