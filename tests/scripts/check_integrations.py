#!/usr/bin/env python3
"""Exercise literal MC extraction, streamed ingest, and atomic replacement."""
import hashlib
import importlib.util
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import time

uc2, mc = map(lambda p: str(Path(p).resolve()), sys.argv[1:3])
env = dict(os.environ, UC2=uc2)


def run(args, **kw):
    return subprocess.run([str(x) for x in args], check=True, env=env,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kw)


with tempfile.TemporaryDirectory(prefix='uc2-integrations-') as temporary:
    root = Path(temporary)
    if os.name == 'posix':
        spec = importlib.util.spec_from_file_location('mc_install', Path(mc).with_name('install.py'))
        installer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(installer)
        config = root / 'config/mc/mc.ext.ini'
        config.parent.mkdir(parents=True)
        original = '# [Default] is below\n[mc.ext.ini]\nVersion=4.0\n[UserRule]\nRegex=custom\n[Default]\nOpen=old\n'
        config.write_text(original)
        installer.install(Path(mc).parent, data_home=root / 'data', config_home=root / 'config', cli=Path(uc2))
        installed = config.read_text()
        installer.install(Path(mc).parent, data_home=root / 'data', config_home=root / 'config', cli=Path(uc2))
        if config.read_text() != installed or installed.count('[UC2]') != 1:
            raise AssertionError('MC installation is not idempotent')
        if '[UserRule]\nRegex=custom' not in installed or installed.index('\n[UC2]\n') > installed.index('\n[Default]\n'):
            raise AssertionError('MC installer changed existing rules or section priority')
        backups = list(config.parent.glob('mc.ext.ini.before-uc2-*'))
        if len(backups) != 1 or backups[0].read_text() != original:
            raise AssertionError('MC configuration backup is missing or incorrect')
        # Exercise the installed helper without an inherited UC2 override.
        installed_helper = root / 'data/mc/extfs.d/uc2'
    source = root/'input'
    source.mkdir()
    (source/'sub dir').mkdir()
    files = {'plain.txt':b'plain', 'star*.txt':b'literal star',
             'starXYZ.txt':b'not the star', 'bracket[1].txt':b'brackets',
             'semicolon;1':b'literal version-looking name',
             'sub dir/spaced file.txt':b'nested', 'empty':b''}
    # Windows cannot represent '*'; exercise those names on POSIX.
    if os.name == 'nt':
        files = {k:v for k,v in files.items() if '*' not in k}
    for name, data in files.items(): (source/name).write_bytes(data)
    archive = root/'files.uc2'
    names = sorted({Path(n).parts[0] for n in files})
    run([uc2, '-qw', archive, *names], cwd=source)
    listing = run([sys.executable, mc, 'list', archive]).stdout.decode()
    if os.name == 'posix':
        installed_env = dict(os.environ)
        installed_env.pop('UC2', None)
        packaged_listing = subprocess.check_output([sys.executable, str(installed_helper), 'list', str(archive)], env=installed_env).decode()
        if packaged_listing != listing:
            raise AssertionError('installed MC helper did not use its configured CLI')
    for name, data in files.items():
        if name not in listing: raise AssertionError('missing listing: '+name)
        target = root/'copy'
        run([sys.executable, mc, 'copyout', archive, name, target])
        if target.read_bytes() != data: raise AssertionError('wrong literal extraction: '+name)
    failure = subprocess.run([sys.executable, mc, 'copyout', archive, 'missing', root/'missing'], env=env)
    if failure.returncode == 0 or (root/'missing').exists(): raise AssertionError('missing member accepted')
    failure = subprocess.run([sys.executable, mc, 'copyin', archive, 'x', root/'copy'], env=env)
    if failure.returncode == 0: raise AssertionError('read-only helper accepted a write')

    # Damage payload bytes while leaving the central directory listable.
    (source/'checksum.bin').write_bytes(bytes(range(256))*100)
    damaged = root/'damaged.uc2'
    run([uc2, '-qw', damaged, 'checksum.bin'], cwd=source)
    encoded = bytearray(damaged.read_bytes())
    cdir = int.from_bytes(encoded[17:21], 'little')
    encoded[cdir-16] ^= 0xff
    damaged.write_bytes(encoded)
    protected_copy = root/'protected-copy'
    protected_copy.write_bytes(b'keep existing contents')
    failure = subprocess.run([sys.executable, mc, 'copyout', damaged,
                              'checksum.bin', protected_copy], env=env)
    if failure.returncode == 0 or protected_copy.read_bytes() != b'keep existing contents':
        raise AssertionError('damaged extraction replaced existing destination')

    rng = random.Random(301)
    for length in (0, 1, 2047, 2048, 8191, 8192, 32767, 32768, 32769, 110000):
        data = rng.randbytes(length)
        ing = root/'ing.uc2'
        run([uc2, '--ingest', ing], input=data)
        if run([uc2, '--ingest-restore', ing]).stdout != data:
            raise AssertionError('stream boundary failure: '+str(length))

    if sys.platform == 'linux' and not os.environ.get('UC2_SKIP_MEMORY_LIMIT'):
        import resource
        # A 64 MiB stream must work below the old whole-input allocation size.
        datafile = root/'large.bin'
        unit = rng.randbytes(32768)
        expected = hashlib.sha256()
        with datafile.open('wb') as f:
            for _ in range(2048): f.write(unit); expected.update(unit)
        def limit(): resource.setrlimit(resource.RLIMIT_AS, (48*1024**2, 48*1024**2))
        with datafile.open('rb') as f:
            run([uc2, '--ingest', root/'large.uc2'], stdin=f, preexec_fn=limit)
        with (root/'restored.bin').open('wb') as f:
            subprocess.run([uc2, '--ingest-restore', str(root/'large.uc2')], stdout=f, check=True)
        with (root/'restored.bin').open('rb') as f:
            if hashlib.file_digest(f, 'sha256').digest() != expected.digest():
                raise AssertionError('large stream restore mismatch')
        run([uc2, '-qw', root/'large-files.uc2', datafile], preexec_fn=limit)
        with (root/'restored-file.bin').open('wb') as f:
            subprocess.run([uc2, '-qp', str(root/'large-files.uc2'), 'large.bin'],
                           stdout=f, check=True)
        with (root/'restored-file.bin').open('rb') as f:
            if hashlib.file_digest(f, 'sha256').digest() != expected.digest():
                raise AssertionError('memory-limited writer restore mismatch')

    if os.name == 'posix':
        protected = root/'protected.uc2'
        protected.write_bytes(b'existing destination')
        process = subprocess.Popen([uc2, '--ingest', str(protected)], stdin=subprocess.PIPE,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            process.stdin.write(b'partial input'); process.stdin.flush()
            deadline = time.monotonic() + 5
            while not list(root.glob('protected.uc2.tmp-*')) and time.monotonic() < deadline:
                time.sleep(.01)
            if not list(root.glob('protected.uc2.tmp-*')):
                raise AssertionError('writer did not start')
            if protected.read_bytes() != b'existing destination':
                raise AssertionError('destination changed before EOF')
            process.terminate(); process.wait(timeout=5)
            if protected.read_bytes() != b'existing destination':
                raise AssertionError('interrupted writer damaged destination')
        finally:
            if process.poll() is None: process.kill(); process.wait()
            process.stdin.close(); process.stderr.close()
print('Integration checks passed: MC literal paths, ingest boundaries, bounded payload RAM, interruption safety')
