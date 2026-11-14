#!/usr/bin/env python3
"""Exercise the native tar parser against Python-generated tar streams."""
import io
import os
from datetime import datetime
from pathlib import Path
import random
import subprocess
import sys
import tarfile
import tempfile
from uc2 import UC2File

cli = sys.argv[1]


def directory_metadata(root):
    """An explicit directory has the same metadata before or after its child."""
    for fmt, kind in ((tarfile.USTAR_FORMAT, tarfile.DIRTYPE),
                      (tarfile.GNU_FORMAT, tarfile.DIRTYPE),
                      (tarfile.PAX_FORMAT, tarfile.DIRTYPE),
                      (tarfile.USTAR_FORMAT, tarfile.AREGTYPE)):
        snapshots = []
        for children_first in (False, True):
            stream = io.BytesIO()
            with tarfile.open(fileobj=stream, mode='w', format=fmt) as tar:
                def directory(path, mode, mtime):
                    h = tarfile.TarInfo(path + '/')
                    h.type, h.mode, h.mtime = kind, mode, mtime
                    if fmt == tarfile.PAX_FORMAT:
                        h.pax_headers = {'mtime': f'{mtime}.75'}
                    tar.addfile(h)

                def directories():
                    # The late case also declares the nested directory first.
                    paths = [('outer', 0o555, 1600000000),
                             ('outer/nested', 0o700, 1600000080)]
                    for args in reversed(paths) if children_first else paths:
                        directory(*args)

                if not children_first:
                    directories()
                for path, data in (('outer/nested/payload', b'old version'),
                                   ('outer/nested/payload', b'new version'),
                                   ('implicit/payload', b'no directory record')):
                    h = tarfile.TarInfo(path)
                    h.size, h.mode, h.mtime = len(data), 0o644, 1600000160
                    tar.addfile(h, io.BytesIO(data))
                if children_first:
                    directories()
                directory('untimed', 0o755, 0)
                # Keep the existing first-explicit-record rule, including an
                # explicit zero timestamp and writable attributes.
                for path in ('outer', 'outer/nested', 'untimed'):
                    directory(path, 0o444, 1700000000)
            destination = root / f'dir-metadata-{fmt}-{kind.hex()}-{children_first}.uc2'
            subprocess.run([cli, '--ingest-tar', str(destination)],
                           input=stream.getvalue(), check=True, capture_output=True)
            with UC2File(destination) as archive:
                assert archive.getinfo('outer').attributes == 0x11, (fmt, children_first)
                assert archive.getinfo('outer/nested').attributes == 0x10
                for path, stamp in (('outer', 1600000000), ('outer/nested', 1600000080)):
                    assert archive.getinfo(path).date_time == datetime.fromtimestamp(stamp)
                for path in ('implicit', 'untimed'):
                    assert archive.getinfo(path).attributes == 0x10
                    assert archive.getinfo(path).date_time is None
                entries = archive.infolist(all_versions=True)
                assert [i.filename for i in entries] == [
                    'outer', 'outer/nested', 'implicit', 'untimed',
                    'outer/nested/payload', 'outer/nested/payload', 'implicit/payload']
                snapshots.append([(i.filename, i.version, i.attributes, i.dos_time,
                                   None if i.is_dir() else archive.read(i)) for i in entries])
        assert snapshots[0] == snapshots[1]
        print('directory metadata independent of tar member order:', fmt, kind)


with tempfile.TemporaryDirectory() as name:
    root = Path(name)
    directory_metadata(root)
    payload = random.Random(15).randbytes(150000)
    for fmt in (tarfile.USTAR_FORMAT, tarfile.GNU_FORMAT, tarfile.PAX_FORMAT):
        stream = io.BytesIO()
        expected = {'tree/a.bin': payload, 'tree/b.bin': payload, 'empty': b'', 'caf\u00e9.txt': b'hi'}
        expected['long/' + 'component-'*(8 if fmt == tarfile.USTAR_FORMAT else 15) + '.txt'] = b'long name'
        if fmt == tarfile.USTAR_FORMAT: expected['prefix/'*12+'file.txt'] = b'ustar prefix'
        with tarfile.open(fileobj=stream, mode='w', format=fmt) as tar:
            d = tarfile.TarInfo('emptydir/'); d.type=tarfile.DIRTYPE; tar.addfile(d)
            for path, data in expected.items():
                h = tarfile.TarInfo('./'+path); h.size=len(data); h.mtime=1600000000; h.mode=0o644
                tar.addfile(h, io.BytesIO(data))
        destination = root/f'test-{fmt}.uc2'
        subprocess.run([cli, '--ingest-tar', str(destination)], input=stream.getvalue(), check=True, capture_output=True)
        with UC2File(destination) as archive:
            assert {i.filename:archive.read(i) for i in archive.infolist() if not i.is_dir()} == expected
            assert archive.getinfo('emptydir').is_dir()
            assert archive.getinfo('tree/a.bin').date_time.year == 2020
        assert destination.stat().st_size < len(payload)*1.5, (destination.stat().st_size, len(payload))
        print('tar format',fmt,'exact bytes and identical-file reuse passed')
    stream=io.BytesIO()
    with tarfile.open(fileobj=stream, mode='w', format=tarfile.USTAR_FORMAT) as tar:
        h=tarfile.TarInfo('old-directory/');h.type=tarfile.AREGTYPE;tar.addfile(h)
    destination=root/'v7-directory.uc2'
    subprocess.run([cli, '--ingest-tar', str(destination)], input=stream.getvalue(), check=True, capture_output=True)
    with UC2File(destination) as archive: assert archive.getinfo('old-directory').is_dir()
    for kind in ('truncated', 'checksum', 'symlink', 'hardlink', 'escape', 'absolute', 'rootdir', 'emptydir', 'fifo', 'sparse', 'trailing'):
        stream=io.BytesIO()
        with tarfile.open(fileobj=stream, mode='w', format=tarfile.PAX_FORMAT) as tar:
            h=tarfile.TarInfo('../escape' if kind=='escape' else '/absolute' if kind=='absolute' else 'file')
            if kind in ('rootdir', 'emptydir'): h.name='/' if kind=='rootdir' else '';h.type=tarfile.DIRTYPE
            elif kind=='symlink': h.type=tarfile.SYMTYPE;h.linkname='/etc/passwd'
            elif kind=='hardlink': h.type=tarfile.LNKTYPE;h.linkname='outside'
            elif kind=='fifo': h.type=tarfile.FIFOTYPE
            elif kind=='sparse': h.pax_headers={'GNU.sparse.size':'99'}
            tar.addfile(h)
        data=bytearray(stream.getvalue())
        if kind=='truncated': data=data[:600]
        elif kind=='checksum': data[0]^=1
        elif kind=='trailing': data[-1]=1
        destination=root/'protected.uc2';destination.write_bytes(b'existing')
        result=subprocess.run([cli,'--ingest-tar',str(destination)], input=data,capture_output=True)
        assert result.returncode, kind
        assert destination.read_bytes()==b'existing',kind
        assert not list(root.glob('protected.uc2.tmp-*'))
        print(kind,'refused atomically')
print('tar ingestion checks passed')
