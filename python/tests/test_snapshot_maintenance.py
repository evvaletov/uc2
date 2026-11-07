"""Retention ordering, verified repair, v1 migration and packed transactions."""
import json
import shutil
import sqlite3
from pathlib import Path

import pytest
from uc2 import snapshot as s


def versions(tmp_path):
    src, repo = tmp_path / 'src', tmp_path / 'repo'
    src.mkdir()
    (src / 'a').write_bytes(b'old' * 10000)
    old = s.create(src, repo)['snapshot']
    (src / 'a').write_bytes(b'new' * 10000)
    new = s.create(src, repo)['snapshot']
    return src, repo, old, new


@pytest.mark.parametrize('packed', [False, True])
def test_retention_replica_repair_and_reuse(tmp_path, packed):
    src, repo, old, new = versions(tmp_path)
    replica = tmp_path / 'replica'
    shutil.copytree(repo, replica)
    if packed:
        s.compact(repo)
        assert not list((repo / 'chunks').glob('*/*.uc2'))
        assert not list((repo / 'snapshots').glob('*.json'))
        with sqlite3.connect(repo / 'objects.sqlite3') as db:
            db.execute('UPDATE chunks SET data=?', (b'broken',))
            db.execute('DELETE FROM manifests WHERE digest=?', (old,))
    else:
        for path in (repo / 'chunks').glob('*/*.uc2'):
            path.write_bytes(b'broken')
        (repo / 'snapshots' / (old + '.json')).unlink()
    with pytest.raises((ValueError, OSError)):
        s.verify(repo, new)
    repaired = s.repair(repo, replica)
    assert repaired['repaired_chunks'] > 0
    assert repaired['repaired_manifests'] == 1
    assert len(s.snapshots(repo)) == 2
    plan = s.prune(repo)
    assert plan['removed_snapshots'] == [old]
    assert plan['unreachable_chunks'] > 0
    assert len(s.snapshots(repo)) == 2
    result = s.prune(repo, apply=True)
    assert result == dict(plan, applied=True)
    assert s.snapshots(repo)[0]['snapshot'] == new
    assert s.verify(repo, new)['verified_files'] == 1
    s.restore(repo, new, tmp_path / 'restored')
    assert (tmp_path / 'restored' / 'a').read_bytes() == (src / 'a').read_bytes()
    assert s.create(src, repo)['new_chunks'] == 0
    if packed:
        s.compact(repo)
        assert json.loads((repo / 'config.json').read_text()) == s.PACKED_FORMAT


def test_corrupt_history_blocks_pruning(tmp_path):
    _, repo, old, new = versions(tmp_path)
    (repo / 'snapshots' / (old + '.json')).write_bytes(b'bad')
    paths = sorted(p.relative_to(repo) for p in repo.rglob('*'))
    with pytest.raises(ValueError):
        s.prune(repo, apply=True)
    assert paths == sorted(p.relative_to(repo) for p in repo.rglob('*'))
    assert s.verify(repo, new)['verified_files'] == 1


def test_bad_replica_changes_nothing(tmp_path):
    _, repo, old, _ = versions(tmp_path)
    replica = tmp_path / 'replica'
    shutil.copytree(repo, replica)
    (replica / 'snapshots' / (old + '.json')).write_bytes(b'broken')
    before = {p.relative_to(repo): p.read_bytes() for p in repo.rglob('*') if p.is_file()}
    with pytest.raises(ValueError):
        s.repair(repo, replica)
    assert before == {p.relative_to(repo): p.read_bytes() for p in repo.rglob('*') if p.is_file()}


def test_reader_lock_refuses_maintenance(tmp_path):
    _, repo, _, new = versions(tmp_path)
    with s._lock(repo):
        for operation in (lambda: s.prune(repo, apply=True), lambda: s.compact(repo),
                          lambda: s.verify(repo, new), lambda: s.snapshots(repo)):
            with pytest.raises(FileExistsError):
                operation()


def test_prune_interruption_only_leaves_orphans(tmp_path, monkeypatch):
    _, repo, old, new = versions(tmp_path)
    unlink = Path.unlink
    def fail(self, *args, **kwargs):
        if self.suffix == '.uc2':
            raise OSError('interrupted')
        return unlink(self, *args, **kwargs)
    with monkeypatch.context() as m:
        m.setattr(Path, 'unlink', fail)
        with pytest.raises(OSError, match='interrupted'):
            s.prune(repo, apply=True)
    assert [x['snapshot'] for x in s.snapshots(repo)] == [new]
    s.verify(repo, new)
    assert s.prune(repo, apply=True)['unreachable_chunks'] > 0


def test_failed_migration_preserves_v1(tmp_path, monkeypatch):
    _, repo, old, new = versions(tmp_path)
    replace = s._replace_file
    def fail(path, data):
        if path.name == 'config.json':
            raise OSError('interrupt config switch')
        return replace(path, data)
    with monkeypatch.context() as m:
        m.setattr(s, '_replace_file', fail)
        with pytest.raises(OSError):
            s.compact(repo)
    assert json.loads((repo / 'config.json').read_text()) == s.FORMAT
    s.verify(repo, old)
    s.compact(repo)
    s.verify(repo, new)


def test_v2_failed_create_rolls_back(tmp_path, monkeypatch):
    src, repo, _, new = versions(tmp_path)
    s.compact(repo)
    (src / 'a').write_bytes(b'not committed' * 100)
    before = s.snapshots(repo)
    def fail(*args):
        raise OSError('manifest failure')
    monkeypatch.setattr(s, '_save_manifest', fail)
    with pytest.raises(OSError):
        s.create(src, repo)
    assert s.snapshots(repo) == before
    s.verify(repo, new)


@pytest.mark.parametrize('corruption', ['deflate', 'chunk-type', 'manifest-type'])
def test_packed_logical_corruption_repair(tmp_path, corruption):
    _, repo, old, new = versions(tmp_path)
    replica = tmp_path / 'replica'
    shutil.copytree(repo, replica)
    s.compact(repo)
    with sqlite3.connect(repo / 'objects.sqlite3') as db:
        if corruption == 'deflate':
            # gzip header followed by reserved deflate block type.
            db.execute('UPDATE manifests SET data=? WHERE digest=?', (b'\x1f\x8b\x08\x00'+b'\0'*6+b'\x07'+b'\0'*8,new))
        elif corruption == 'chunk-type':
            db.execute('UPDATE chunks SET data=123')
        else:
            db.execute('UPDATE manifests SET data=123')
    s.repair(repo, replica)
    s.verify(repo, old)
    s.verify(repo, new)


def test_cleanup_after_interrupted_compaction_and_prune(tmp_path, monkeypatch):
    _, repo, old, new = versions(tmp_path)
    unlink=Path.unlink
    def fail(self, *args, **kwargs):
        if self.suffix == '.uc2':
            raise OSError('cleanup interrupted')
        return unlink(self, *args, **kwargs)
    with monkeypatch.context() as m:
        m.setattr(Path, 'unlink', fail)
        with pytest.raises(OSError):
            s.compact(repo)
    s.prune(repo, apply=True)
    s.compact(repo)
    assert not list((repo/'chunks').glob('*/*.uc2'))
    s.verify(repo, new)


def test_stray_files_are_ignored_and_retained(tmp_path):
    _, repo, _, new = versions(tmp_path)
    stray=repo/'snapshots'/'notes.json';stray.write_text('unrelated')
    assert len(s.snapshots(repo)) == 2
    s.compact(repo)
    assert stray.read_text() == 'unrelated'
    s.verify(repo,new)


@pytest.mark.parametrize('packed', [False, True])
def test_inventory_limits_include_orphans(tmp_path, monkeypatch, packed):
    _, repo, _, _ = versions(tmp_path)
    if packed:
        s.compact(repo)
    with monkeypatch.context() as m:
        m.setattr(s, 'MAX_OBJECTS', 1)
        with pytest.raises(ValueError, match='chunk-count limit'):
            s.prune(repo)
    with monkeypatch.context() as m:
        m.setattr(s, 'MAX_SNAPSHOTS', 1)
        with pytest.raises(ValueError, match='snapshot-count limit'):
            s.snapshots(repo)
    assert len(s.snapshots(repo)) == 2
