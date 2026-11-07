"""Original SAS splitter fixtures; siblings are data, never executable code."""
import io
import shutil
from pathlib import Path
import pytest
from uc2 import UC2File, BadArchiveError

FIXTURES = Path(__file__).resolve().parents[2] / 'tests' / 'archives'


@pytest.mark.parametrize('part', [1, 2, 3])
def test_original_sas_any_part(part):
    with UC2File(FIXTURES/'turbo0.uc2') as archive:
        expected = {i.filename: archive.read(i) for i in archive.infolist()}
    with UC2File(FIXTURES/'sas'/f'SPLIT.P{part:02}') as archive:
        assert {i.filename: archive.read(i) for i in archive.infolist()} == expected


@pytest.mark.parametrize('fault', ['missing', 'truncated', 'order', 'total', 'size', 'extra'])
def test_invalid_sas_sets(tmp_path, fault):
    shutil.copytree(FIXTURES/'sas', tmp_path/'sas')
    part = tmp_path/'sas'/'SPLIT.P02'
    data = bytearray(part.read_bytes())
    if fault == 'missing':
        part.unlink()
    else:
        if fault == 'truncated': data = data[:-1]
        elif fault == 'order': data[12] = 3
        elif fault == 'total': data[4] ^= 1
        elif fault == 'size': data[8:12] = (14).to_bytes(4, 'little')
        elif fault == 'extra': data += b'bad'
        part.write_bytes(data)
    with pytest.raises(BadArchiveError, match='SAS'):
        UC2File(tmp_path/'sas'/'SPLIT.P01')


def test_stream_has_no_sibling_resolver():
    with pytest.raises(BadArchiveError, match='SAS'):
        UC2File(io.BytesIO((FIXTURES/'sas'/'SPLIT.P01').read_bytes()))


@pytest.mark.parametrize('suffix', ['P1', 'P001'])
def test_noncanonical_sas_suffix_is_rejected(tmp_path, suffix):
    shutil.copytree(FIXTURES/'sas', tmp_path/'sas')
    wrong = tmp_path/'sas'/('SPLIT.'+suffix)
    shutil.copyfile(tmp_path/'sas'/'SPLIT.P01', wrong)
    with pytest.raises(BadArchiveError, match='SAS'):
        UC2File(wrong)


@pytest.mark.parametrize('name', ['NORMAL.EXE', 'NORMAL.001', 'NORMAL.003', 'NORMAL.005', 'MORE.EXE'])
def test_original_sea_payloads(name, tmp_path, monkeypatch):
    import hashlib
    import json
    monkeypatch.chdir(tmp_path)
    manifest = json.loads((FIXTURES/'sea'/'manifest.json').read_text())
    entry = next(s for s in manifest['sets'] if name.startswith(s['name']+'.'))
    expected = {p['path']: (p['size'], p['sha256']) for p in entry['payloads']}
    with UC2File(FIXTURES/'sea'/name) as archive:
        actual = {i.filename: (i.file_size, hashlib.sha256(archive.read(i)).hexdigest())
                  for i in archive.infolist() if not i.is_dir()}
    assert actual == expected
    assert not list(tmp_path.iterdir())  # no banner/startup execution or extraction


@pytest.mark.parametrize('fault', ['missing', 'truncated', 'mixed', 'count', 'offset', 'banner', 'trailing'])
def test_damaged_sea(tmp_path, fault):
    shutil.copytree(FIXTURES/'sea', tmp_path/'sea')
    first=tmp_path/'sea'/'NORMAL.EXE'
    part=tmp_path/'sea'/'NORMAL.003'
    if fault=='missing': part.unlink()
    elif fault=='truncated': part.write_bytes(part.read_bytes()[:-2])
    elif fault=='mixed': part.write_bytes(b'X'*part.stat().st_size)
    elif fault=='trailing':
        tail=tmp_path/'sea'/'NORMAL.005';tail.write_bytes(tail.read_bytes()+b'XX')
    else:
        data=bytearray(first.read_bytes())
        data[{'count':85, 'offset':79, 'banner':99}[fault]] ^= 1
        first.write_bytes(data)
    with pytest.raises(BadArchiveError, match='UC2SEA'):
        UC2File(first)


def test_short_final_sea_part(tmp_path):
    data=bytearray((FIXTURES/'sea'/'MORE.EXE').read_bytes())
    config=data.index(b'UC2SFX Header')
    data[config+19:config+21]=(1).to_bytes(2,'little')
    (tmp_path/'MORE.EXE').write_bytes(data[:-2])
    part=tmp_path/'MORE.001';part.write_bytes(data[-2:])
    with UC2File(part) as archive:
        assert archive.read('SUB/NOTE.TXT') == b'UC2SEA nested file\r\n'*17
    wrong=tmp_path/'MORE.000';wrong.write_bytes(part.read_bytes())
    with pytest.raises(BadArchiveError, match='start at .001'):
        UC2File(wrong)



def test_native_path_preserves_python_file_handles(tmp_path):
    import subprocess
    import sys
    code = r"""
import tempfile
from pathlib import Path
from uc2 import UC2File
import sys
handles=[tempfile.TemporaryFile() for _ in range(12)]
for i,h in enumerate(handles):
    h.write(bytes([i])*100);h.flush()
for _ in range(8):
    with UC2File(sys.argv[1]) as archive:
        assert archive.read('SUB/NOTE.TXT') == b'UC2SEA nested file\r\n'*17
for i,h in enumerate(handles):
    h.seek(0);assert h.read()==bytes([i])*100;h.close()
print('handles preserved')
"""
    result=subprocess.run([sys.executable,'-c',code,str(FIXTURES/'sea'/'MORE.EXE')],
                          cwd=tmp_path,capture_output=True,text=True)
    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == 'handles preserved'


@pytest.mark.skipif(__import__('os').name == 'nt', reason='POSIX byte filenames')
def test_non_utf8_volume_path(tmp_path):
    import errno
    import os
    directory=tmp_path/os.fsdecode(b'volume-\xff')
    try:
        directory.mkdir()
    except OSError as exc:
        if exc.errno == errno.EILSEQ:
            pytest.skip('filesystem requires valid UTF-8 filenames')
        raise
    first=directory/'MORE.EXE'
    shutil.copyfile(FIXTURES/'sea'/'MORE.EXE', first)
    with UC2File(os.fsencode(first)) as archive:
        assert archive.read('SUB/NOTE.TXT') == b'UC2SEA nested file\r\n'*17
