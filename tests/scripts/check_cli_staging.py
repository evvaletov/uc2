#!/usr/bin/env python3
"""Check staged output cleanup on read errors and failed installation."""
import io
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile


CLI = str(Path(sys.argv[1]).resolve())
PAYLOAD = b'staged output regression\n'


def run(*args, data=PAYLOAD, stdin=None, success=True):
    input_args = {'stdin': stdin} if stdin is not None else {'input': data}
    result = subprocess.run([CLI, *map(str, args)], capture_output=True,
                            timeout=20, **input_args)
    assert (result.returncode == 0) == success, (args, result)
    return result


with tempfile.TemporaryDirectory(prefix='uc2-cli-staging-') as temporary:
    root = Path(temporary)
    source = root / 'input.txt'
    source.write_bytes(PAYLOAD)
    protected = root / 'protected.uc2'
    protected.write_bytes(b'existing archive')

    # Opaque ingestion opens the staged stream before it reads stdin. The
    # write-only descriptor rejects reads on every supported OS (NetBSD
    # permits directory reads), so atexit must close then unlink the stage.
    fd = os.open(source, os.O_WRONLY)
    try:
        result = run('--ingest', protected, stdin=fd, success=False)
    finally:
        os.close(fd)
    assert b'read stdin' in result.stderr, result
    assert protected.read_bytes() == b'existing archive'
    assert not list(root.glob('U???????.TMP' if os.name == 'nt' else '.uc2-tmp-*'))

    tar_bytes = io.BytesIO()
    with tarfile.open(fileobj=tar_bytes, mode='w') as tar:
        member = tarfile.TarInfo('input.txt')
        member.size = len(PAYLOAD)
        tar.addfile(member, io.BytesIO(PAYLOAD))

    # Exercise every staging caller, both successful commit and failure to
    # replace a directory. Closing the stream must transfer ownership once.
    for mode in ('create', 'ingest', 'chunked-ingest', 'tar'):
        def arguments(path):
            if mode == 'create':
                return ('-qw', path, source)
            if mode == 'chunked-ingest':
                return ('--chunked', '--ingest', path)
            return ('--ingest-tar' if mode == 'tar' else '--ingest', path)

        data = tar_bytes.getvalue() if mode == 'tar' else PAYLOAD
        archive = root / f'{mode}.uc2'
        run(*arguments(archive), data=data)
        run('-qt', archive)
        assert not list(root.glob('U???????.TMP' if os.name == 'nt' else '.uc2-tmp-*'))
        destination = root / f'{mode}-directory.uc2'
        destination.mkdir()
        marker = destination / 'keep'
        marker.write_bytes(b'keep this directory')
        run(*arguments(destination), data=data, success=False)
        assert marker.read_bytes() == b'keep this directory'
        assert not list(root.glob('U???????.TMP' if os.name == 'nt' else '.uc2-tmp-*'))
    assert source.read_bytes() == PAYLOAD

print('CLI staged output checks passed')
