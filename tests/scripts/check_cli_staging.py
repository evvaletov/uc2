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


def directory_input(path):
    """Return a valid descriptor whose reads fail, including on Windows."""
    if os.name != 'nt':
        return os.open(path, os.O_RDONLY)
    import ctypes
    from ctypes import wintypes
    import msvcrt
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    create = kernel.CreateFileW
    create.argtypes = (wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                       ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD,
                       wintypes.HANDLE)
    create.restype = wintypes.HANDLE
    close = kernel.CloseHandle
    close.argtypes = (wintypes.HANDLE,)
    close.restype = wintypes.BOOL
    # GENERIC_READ, full sharing, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS.
    handle = create(str(path), 0x80000000, 7, None, 3, 0x02000000, None)
    if handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        return msvcrt.open_osfhandle(handle, os.O_RDONLY | os.O_BINARY)
    except BaseException:
        close(handle)
        raise


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
    # directory read fails after that point, so atexit must close then unlink.
    fd = directory_input(root)
    try:
        result = run('--ingest', protected, stdin=fd, success=False)
    finally:
        os.close(fd)
    assert b'read stdin' in result.stderr, result
    assert protected.read_bytes() == b'existing archive'
    assert not list(root.glob('protected.uc2.tmp-*'))

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
        assert not list(root.glob(archive.name + '.tmp-*'))
        destination = root / f'{mode}-directory.uc2'
        destination.mkdir()
        marker = destination / 'keep'
        marker.write_bytes(b'keep this directory')
        run(*arguments(destination), data=data, success=False)
        assert marker.read_bytes() == b'keep this directory'
        assert not list(root.glob(destination.name + '.tmp-*'))
    assert source.read_bytes() == PAYLOAD

print('CLI staged output checks passed')
