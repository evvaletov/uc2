from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import zipfile

import pytest

from uc2 import UC2File, UC2Writer
from uc2.preserve import preserve

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(params=[4, 8])
def collection(tmp_path, request):
    path = tmp_path / 'collection.UC2'
    contents = [('folder/notes.txt', b'first version\n'),
                ('folder/notes.txt', b'second version\n' * 5000),
                ('folder/NOTES.TXT', b'case collision'),
                ('CON', b'reserved DOS name'),
                ('empty', b''), ('long café.txt', bytes(range(256)) * 200)]
    with UC2Writer(path, level=request.param) as writer:
        writer.mkdir('empty-folder', date_time=datetime(1996, 7, 4, 12, 14, 16))
        for name, data in contents:
            writer.writestr(name, data, date_time=datetime(1993, 2, 3, 4, 5, 6), attributes=0x23)
    return path, contents


@pytest.mark.parametrize('format', ['inventory', 'zip', 'tar'])
def test_migration_retains_versions_metadata_and_bytes(collection, tmp_path, format):
    source, contents = collection
    before = source.read_bytes()
    output = tmp_path / format
    report = preserve([source], output, format=format)
    assert report['status'] == 'ok'
    manifest = json.loads((output / report['archives'][0]['manifest']).read_text())
    assert manifest['source']['sha256'] == hashlib.sha256(before).hexdigest()
    files = [entry for entry in manifest['entries'] if entry['kind'] == 'file']
    assert [entry['path'] for entry in files] == [name for name, data in contents]
    versions = [entry['version'] for entry in files if entry['path'] == 'folder/notes.txt']
    assert versions == [1, 0]
    for entry, (_, data) in zip(files, contents):
        assert entry['status'] == 'verified'
        assert entry['sha256'] == hashlib.sha256(data).hexdigest()
        assert entry['dos_attributes'] == 0x23
        assert entry['timestamp_local'] == '1993-02-03T04:05:06'
        assert entry['dos_time'] == ((1993 - 1980) << 25 | 2 << 21 | 3 << 16 | 4 << 11 | 5 << 5 | 3)
    if format != 'inventory':
        paths = [entry['export_path'] for entry in files]
        assert len({p.casefold() for p in paths}) == len(paths)
        assert all('..' not in p.split('/') and not p.startswith('/') for p in paths)
        export = (output / report['archives'][0]['manifest']).parent / manifest['export']['path']
        if format == 'zip':
            with zipfile.ZipFile(export) as archive:
                for entry, (_, data) in zip(files, contents):
                    assert archive.read(entry['export_path']) == data
                    assert archive.getinfo(entry['export_path']).date_time == (1993, 2, 3, 4, 5, 6)
        else:
            with tarfile.open(export) as archive:
                for entry, (_, data) in zip(files, contents):
                    member = archive.getmember(entry['export_path'])
                    assert archive.extractfile(member).read() == data
                    assert member.pax_headers['UC2.path'] == entry['path']
                    assert member.mtime == 0
        assert manifest['export']['verified']
    assert source.read_bytes() == before
    with pytest.raises(FileExistsError):
        preserve([source], output, format=format)
    assert source.read_bytes() == before


@pytest.mark.parametrize('format', ['zip', 'tar'])
def test_original_encoder_corpus(tmp_path, format):
    report = preserve([ROOT / 'tests/archives' / item['path'] for item in
                       json.loads((ROOT / 'tests/archives/manifest.json').read_text())['archives']], tmp_path / format, format=format)
    assert report['status'] == 'ok'
    assert len(report['archives']) == 5
    for record in report['archives']:
        manifest = json.loads((tmp_path / format / record['manifest']).read_text())
        for entry in manifest['entries']:
            if entry['kind'] == 'file':
                expected = (ROOT / 'tests/corpus' / entry['path'].lower()).read_bytes()
                assert entry['sha256'] == hashlib.sha256(expected).hexdigest()


def test_bad_source_does_not_stop_batch(tmp_path):
    bad = tmp_path / 'bad.uc2'
    bad.write_bytes(b'not UC2')
    good = ROOT / 'tests/archives/single.uc2'
    result = preserve([bad, good], tmp_path / 'out', format='zip')
    assert result['status'] == 'partial'
    assert [r['status'] for r in result['archives']] == ['error', 'ok']
    broken = json.loads((tmp_path / 'out' / result['archives'][0]['manifest']).read_text())
    assert broken['errors'] and broken['export'] is None


def test_damaged_turbo_is_an_error(tmp_path):
    source = ROOT / 'tests/fuzz/regressions/turbo-empty-checksum.uc2'
    output = tmp_path / 'out'
    report = preserve([source], output)
    assert report['status'] == 'partial'
    manifest = json.loads((output / report['archives'][0]['manifest']).read_text())
    entry, = manifest['entries']
    assert entry['method'] == 80
    assert entry['status'] == 'error'
    assert entry['sha256'] is None
    assert 'Damaged archive' in entry['error']


def test_corruption_is_recorded_without_partial_export(tmp_path):
    path = tmp_path / 'bad-payload.uc2'
    with UC2Writer(path) as writer:
        writer.writestr('payload', bytes(range(256)) * 100)
    data = bytearray(path.read_bytes())
    directory = int.from_bytes(data[17:21], 'little')
    data[directory - 16] ^= 255
    path.write_bytes(data)
    report = preserve([path], tmp_path / 'out', format='zip')
    assert report['status'] == 'partial'
    manifest_path = tmp_path / 'out' / report['archives'][0]['manifest']
    manifest = json.loads(manifest_path.read_text())
    assert manifest['entries'][0]['status'] == 'error'
    assert manifest['entries'][0]['sha256'] is None
    with zipfile.ZipFile(manifest_path.parent / 'content.zip') as archive:
        assert archive.namelist() == []


@pytest.mark.parametrize('limit', ['max_archive_size', 'max_entries', 'max_decoder_memory', 'max_extract_size', 'max_total_size'])
def test_limits_record_failure(collection, tmp_path, limit):
    source, _ = collection
    report = preserve([source], tmp_path / limit, **{limit: 1})
    assert report['status'] == 'partial'
    manifest = json.loads((tmp_path / limit / report['archives'][0]['manifest']).read_text())
    if limit in ('max_extract_size', 'max_total_size'):
        assert manifest['status'] == 'partial'
        assert any(e['status'] == 'error' for e in manifest['entries'])
        assert next(e for e in manifest['entries'] if e['path'] == 'empty')['status'] == 'verified'
    else:
        assert manifest['status'] == 'error' and manifest['errors']


def test_cli_exit_status_and_recursive_discovery(tmp_path):
    directory = tmp_path / 'input' / 'nested'
    directory.mkdir(parents=True)
    (directory / 'old.UC2').write_bytes((ROOT / 'tests/archives/single.uc2').read_bytes())
    out = tmp_path / 'out'
    command = [sys.executable, '-m', 'uc2.preserve', str(directory.parent), '-o', str(out), '--format', 'tar']
    result = subprocess.run(command, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert json.loads((out / 'inventory.json').read_text())['status'] == 'ok'
    assert subprocess.run(command, capture_output=True).returncode == 2
    bad = tmp_path / 'bad.uc2'
    bad.write_bytes(b'invalid')
    result = subprocess.run([sys.executable, '-m', 'uc2.preserve', str(bad), '-o', str(tmp_path / 'bad-output')], capture_output=True)
    assert result.returncode == 1


def test_raw_time_api(collection):
    source, _ = collection
    with UC2File(source) as reader:
        assert all(info.dos_time for info in reader.infolist(all_versions=True) if not info.is_dir())


def test_corpus_manifest_hashes():
    manifest = json.loads((ROOT / 'tests/archives/manifest.json').read_text())
    for group, directory in [('archives', 'archives'), ('payloads', 'corpus')]:
        for item in manifest[group]:
            data = (ROOT / 'tests' / directory / item['path']).read_bytes()
            assert len(data) == item['size']
            assert hashlib.sha256(data).hexdigest() == item['sha256']


def test_symlink_does_not_hide_real_input(tmp_path):
    source = ROOT / 'tests/archives/single.uc2'
    link = tmp_path / 'link.uc2'
    try:
        link.symlink_to(source)
    except OSError:
        pytest.skip('symlink creation is unavailable')
    report = preserve([link, source], tmp_path / 'out')
    assert [r['status'] for r in report['archives']] == ['error', 'ok']


@pytest.mark.skipif(not hasattr(os, 'mkfifo'), reason='POSIX named pipe')
def test_special_file_is_rejected_without_waiting(tmp_path):
    fifo = tmp_path / 'fifo.uc2'
    os.mkfifo(fifo)
    result = subprocess.run([sys.executable, '-m', 'uc2.preserve', str(fifo), '-o', str(tmp_path / 'out')], timeout=10, capture_output=True)
    assert result.returncode == 1
    manifest = next((tmp_path / 'out').glob('*/manifest.json'))
    assert 'regular file' in manifest.read_text()


@pytest.mark.parametrize('failure', ['spool', 'member', 'manifest', 'export-hash', 'bad-zip', 'bad-tar'])
def test_output_failure_aborts_without_publishing(tmp_path, monkeypatch, failure):
    import importlib
    module = importlib.import_module('uc2.preserve')
    def fail(*args, **kwargs):
        raise OSError('simulated destination write failure')
    if failure == 'spool':
        monkeypatch.setattr(module._HashedWriter, 'write', lambda self, data: module._output_call(fail))
    elif failure == 'member':
        monkeypatch.setattr(module, '_write_member', fail)
    elif failure == 'manifest':
        monkeypatch.setattr(Path, 'write_text', fail)
    elif failure == 'export-hash':
        monkeypatch.setattr(module, '_hash_path', fail)
    else:
        def bad(*args, **kwargs):
            error = zipfile.BadZipFile if failure == 'bad-zip' else tarfile.ReadError
            raise error('simulated destination corruption')
        monkeypatch.setattr(module, '_verify_export', bad)
    with pytest.raises(OSError, match='simulated destination'):
        preserve([ROOT / 'tests/archives/single.uc2'], tmp_path / 'out', format='zip')
    assert list(tmp_path.iterdir()) == []


def test_publish_does_not_replace_concurrently_created_directory(tmp_path, monkeypatch):
    import importlib
    module = importlib.import_module('uc2.preserve')
    original = module._publish
    def raced(stage, destination):
        destination.mkdir()
        original(stage, destination)
    monkeypatch.setattr(module, '_publish', raced)
    with pytest.raises(OSError):
        preserve([ROOT / 'tests/archives/single.uc2'], tmp_path / 'out')
    assert (tmp_path / 'out').is_dir()
    assert list((tmp_path / 'out').iterdir()) == []
    assert list(tmp_path.iterdir()) == [tmp_path / 'out']


def test_walk_error_is_not_silently_skipped(tmp_path, monkeypatch):
    def broken_walk(path, *, followlinks, onerror):
        onerror(PermissionError('unreadable archive directory'))
        return iter(())
    monkeypatch.setattr(os, 'walk', broken_walk)
    with pytest.raises(PermissionError, match='unreadable'):
        preserve([tmp_path], tmp_path / 'out')
    assert not (tmp_path / 'out').exists()


def test_source_change_invalidates_manifest_hashes(tmp_path, monkeypatch):
    path = tmp_path / 'changing.uc2'
    path.write_bytes((ROOT / 'tests/archives/single.uc2').read_bytes())
    copy = UC2File.copy_to
    def changing(self, *args, **kwargs):
        result = copy(self, *args, **kwargs)
        with path.open('ab') as source:
            source.write(b'changed')
        return result
    monkeypatch.setattr(UC2File, 'copy_to', changing)
    report = preserve([path], tmp_path / 'out', format='zip')
    manifest = json.loads((tmp_path / 'out' / report['archives'][0]['manifest']).read_text())
    assert report['status'] == 'partial'
    assert manifest['status'] == 'error' and manifest['export'] is None
    assert all(e['sha256'] is None and e['status'] == 'error' for e in manifest['entries'])
