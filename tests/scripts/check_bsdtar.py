#!/usr/bin/env python3
"""Exercise the real bsdtar executable, including pipes and large entries."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

uc2, tar = (str(Path(p).resolve()) for p in sys.argv[1:3])
seekable_only = '--seekable-only' in sys.argv[3:]


def sha(path):
    with path.open('rb') as f: return hashlib.file_digest(f, 'sha256').hexdigest()


with tempfile.TemporaryDirectory(prefix='uc2-bsdtar-') as temp:
    root=Path(temp)
    src=root/'src'; src.mkdir(); (src/'folder').mkdir()
    (src/'folder'/'long filename.txt').write_bytes(b'libarchive integration\n' * 10000)
    (src/'empty').write_bytes(b'')
    (src/'root.bin').write_bytes(bytes(range(256))*400)
    for level, chunked in (((4, False),) if '--legacy-only' in sys.argv[3:] else
                           ((4, False), (6, False), (4, True), (6, True))):
        archive=root/f'level{level}-{chunked}.uc2'
        subprocess.run([uc2,*(['--chunked'] if chunked else []),'-qw','-L',str(level),str(archive),'folder','empty','root.bin'],cwd=src,check=True)
        listed=subprocess.check_output([tar,'-tf',str(archive)]).decode().splitlines()
        for name in ('folder/long filename.txt','empty','root.bin'):
            if name not in listed: raise AssertionError('missing '+name)
        for mode in ('path', 'stdin-file', 'pipe'):
            out=root/f'out-{level}-{chunked}-{mode}';out.mkdir()
            with archive.open('rb') as inp:
                result = subprocess.run(
                    [tar,'-xf',str(archive) if mode == 'path' else '-', '-C',str(out)],
                    stdin=inp if mode == 'stdin-file' else None,
                    input=archive.read_bytes() if mode == 'pipe' else None,
                    capture_output=True)
            if seekable_only and mode == 'pipe':
                if result.returncode == 0 or b'seekable input required' not in result.stderr:
                    raise AssertionError('non-seekable input was not explicitly rejected')
                if list(out.iterdir()): raise AssertionError('failed pipe input published output')
                continue
            result.check_returncode()
            for path in src.rglob('*'):
                if path.is_file() and sha(path)!=sha(out/path.relative_to(src)):
                    raise AssertionError('restore mismatch')
        # Skip earlier entries and extract just one member through libarchive.
        single=root/f'single-{level}-{chunked}';single.mkdir()
        subprocess.run([tar,'-xf',str(archive),'-C',str(single),'folder/long filename.txt'],check=True)
        if sha(src/'folder/long filename.txt') != sha(single/'folder/long filename.txt'):
            raise AssertionError('selective extraction mismatch')
        # Listing a seekable source must not require an input temporary file.
        env = dict(os.environ, TMPDIR=str(root/'missing-temp-directory'))
        subprocess.run([tar,'-tf',str(archive)],check=True,env=env,stdout=subprocess.DEVNULL)
    if sys.platform == 'linux' and not os.environ.get('UC2_SKIP_MEMORY_LIMIT'):
        import resource
        large=src/'large.bin'
        with large.open('wb') as f:
            for _ in range(128): f.write(b'payload-' * 131072)
        archive=root/'large.uc2'
        subprocess.run([uc2,'-qw',str(archive),'large.bin'],cwd=src,check=True,stdout=subprocess.DEVNULL)
        def limit(): resource.setrlimit(resource.RLIMIT_AS,(96*1024**2,96*1024**2))
        out=root/'large-out';out.mkdir()
        subprocess.run([tar,'-xf',str(archive),'-C',str(out)],check=True,preexec_fn=limit)
        if sha(large)!=sha(out/'large.bin'): raise AssertionError('large restore mismatch')
        print('128 MiB entry restored under a 96 MiB address-space limit')
print('Actual bsdtar: listing, supported methods, file/stdin/pipe policy, directories, empty entries, selective extraction passed')
