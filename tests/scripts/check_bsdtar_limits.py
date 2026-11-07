#!/usr/bin/env python3
"""Real libarchive format-option regressions, including stdin and overrides."""
from pathlib import Path
import subprocess
import sys
import tempfile

uc2, tar = (str(Path(p).resolve()) for p in sys.argv[1:3])
with tempfile.TemporaryDirectory(prefix='uc2-la-limits-') as temp:
    root = Path(temp)
    (root/'a').write_bytes(b'one'*10000)
    (root/'b').write_bytes(b'two'*10000)
    archive = root/'files.uc2'
    subprocess.run([uc2, '-qw', str(archive), 'a', 'b'], cwd=root, check=True)
    def run(option, listing=False, pipe=False):
        return subprocess.run([tar, '--options', 'uc2:'+option,
            '-tf' if listing else '-xOf', '-' if pipe else str(archive)],
            input=archive.read_bytes() if pipe else None, capture_output=True)
    for option, listing in [('memory-limit=1',True), ('memory-limit=65536',False),
                            ('entries-limit=1',True), ('input-limit=20',True),
                            ('entry-limit=29999',False), ('total-limit=59999',False)]:
        for pipe in (False,True):
            result = run(option, listing, pipe)
            assert result.returncode != 0, (option, result)
            assert b'limit' in result.stderr or b'uc2_open failed' in result.stderr, (option, result.stderr)
    for option in ['entries-limit=2', 'entry-limit=30000', 'total-limit=60000',
                   'memory-limit=67108864', 'input-limit='+str(archive.stat().st_size)]:
        result = run(option)
        assert result.returncode == 0, (option, result.stderr)
        assert result.stdout == b'one'*10000 + b'two'*10000
    for value in ['0', '-1', '+1', '1k', '18446744073709551616', '']:
        result = run('memory-limit='+value)
        assert result.returncode != 0, value
    corrupt = Path(__file__).resolve().parents[1] / 'fuzz/regressions/turbo-empty-checksum.uc2'
    result = subprocess.run([tar, '-xOf', str(corrupt)], capture_output=True)
    assert result.returncode != 0 and not result.stdout
    print('libarchive: five quotas, exact boundaries, overrides, stdin and invalid options passed')
