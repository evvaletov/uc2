"""Pinned Turbo outputs cross-checked with original DOS and independent Go."""
from pathlib import Path
import hashlib
import json
import pytest
from uc2 import UC2File

CORPUS = Path(__file__).resolve().parents[2] / 'tests/archives'

@pytest.mark.parametrize('case', json.loads((CORPUS/'turbo.json').read_text())['archives'])
def test_turbo_original_and_independent_hashes(case):
    path = CORPUS/case['path']
    assert hashlib.sha256(path.read_bytes()).hexdigest() == case['sha256']
    with UC2File(path) as archive:
        info = archive.getinfo(case['payload_path'])
        assert info.method == 80 and info.file_size == case['payload_size']
        assert hashlib.sha256(archive.read(info)).hexdigest() == case['payload_sha256']


def test_original_sas_fixture_hashes():
    from uc2 import BadArchiveError
    manifest = json.loads((CORPUS/'sas/manifest.json').read_text())
    for item in manifest['parts']:
        path = CORPUS/'sas'/item['path']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == item['sha256']
        with UC2File(path) as archive:
            archive.verify()
