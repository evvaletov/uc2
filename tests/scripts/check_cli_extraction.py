#!/usr/bin/env python3
"""Check extraction destinations and reject an incomplete directory trailer."""
import base64
import datetime
import io
import os
from pathlib import Path
import stat
import subprocess
import sys
import tarfile
import tempfile

CLI = str(Path(sys.argv[1]).resolve())
PAYLOAD = b'archive contents\n'


def run(*args, cwd, success=True):
    result = subprocess.run([CLI, *map(str, args)], cwd=cwd, capture_output=True,
                            timeout=20)
    assert (result.returncode == 0) == success, (args, result)
    return result


with tempfile.TemporaryDirectory(prefix='uc2-cli-extraction-') as temporary:
    root = Path(temporary)
    source = root / 'source'
    (source / 'nested').mkdir(parents=True)
    (source / 'nested/payload').write_bytes(PAYLOAD)
    archive = root / 'files.uc2'
    run('-qw', archive, 'nested', cwd=source)

    # Normal nested extraction, existing directories, and overwrite semantics.
    normal = root / 'normal'
    normal.mkdir()
    run('-qx', archive, cwd=normal)
    target = normal / 'nested/payload'
    assert target.read_bytes() == PAYLOAD
    target.write_bytes(b'keep existing')
    run('-qx', archive, cwd=normal)
    assert target.read_bytes() == b'keep existing'
    run('-qxf', archive, cwd=normal)
    assert target.read_bytes() == PAYLOAD

    # Every entered directory must be left, even when it already exists or is
    # only an unselected ancestor of a selected file.
    expected = {'a/sub/first': b'first', 'b/sub/second': b'second'}
    for name, data in expected.items():
        path = source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    siblings = root / 'siblings.uc2'
    run('-qw', siblings, 'a', 'b', cwd=source)
    for preexisting, selected in ((True, False), (False, True), (True, True)):
        destination = root / f'siblings-{preexisting}-{selected}'
        destination.mkdir()
        if preexisting:
            (destination / 'a/sub').mkdir(parents=True)
            (destination / 'b/sub').mkdir(parents=True)
        patterns = tuple(expected) if selected else ()
        run('-qx', siblings, *patterns, cwd=destination)
        actual = {p.relative_to(destination).as_posix(): p.read_bytes()
                  for p in destination.rglob('*') if p.is_file()}
        assert actual == expected, actual

    # Recursive scans obtain UTF-8 names from readdir on Windows. Metadata
    # lookup must use the same path encoding and retain dates beyond 2038.
    unicode_tree = source / 'unicode-tree'
    (unicode_tree / 'caf\u00e9').mkdir(parents=True)
    dated = unicode_tree / 'caf\u00e9/r\u00e9sum\u00e9.txt'
    dated.write_bytes(PAYLOAD)
    stamp = datetime.datetime(2041, 6, 7, 12, 34, 56).timestamp()
    try:
        os.utime(dated, (stamp, stamp))
        stored_stamp = dated.stat().st_mtime
        if abs(stored_stamp - stamp) >= 2:
            print('Filesystem cannot retain the requested 2041 mtime; checking its stored value')
        stamp = stored_stamp
    except (OSError, OverflowError) as error:
        print('Skipping post-2038 mtime assertion:', error)
        stamp = None
    unicode_archive = root / 'unicode.uc2'
    run('-qw', unicode_archive, 'unicode-tree', cwd=source)
    unicode_out = root / 'unicode-out'
    unicode_out.mkdir()
    run('-qx', unicode_archive, cwd=unicode_out)
    restored = unicode_out / 'unicode-tree/caf\u00e9/r\u00e9sum\u00e9.txt'
    assert restored.read_bytes() == PAYLOAD
    if stamp is not None:
        assert abs(restored.stat().st_mtime - stamp) < 2

    # The DOS read-only bit must leave POSIX directories searchable; forcing a
    # second extraction must also replace read-only regular files on Windows.
    tar_bytes = io.BytesIO()
    with tarfile.open(fileobj=tar_bytes, mode='w') as tar:
        directory = tarfile.TarInfo('readonly-dir')
        directory.type = tarfile.DIRTYPE
        directory.mode = 0o555
        tar.addfile(directory)
        for name in ('readonly-dir/payload', 'readonly-file'):
            member = tarfile.TarInfo(name)
            member.mode = 0o444
            member.size = len(PAYLOAD)
            tar.addfile(member, io.BytesIO(PAYLOAD))
    readonly_archive = root / 'readonly.uc2'
    result = subprocess.run([CLI, '--ingest-tar', str(readonly_archive)],
                            input=tar_bytes.getvalue(), capture_output=True, timeout=20)
    assert result.returncode == 0, result
    readonly_out = root / 'readonly-out'
    readonly_out.mkdir()
    try:
        run('-qx', readonly_archive, cwd=readonly_out)
        if os.name != 'nt':
            assert stat.S_IMODE((readonly_out / 'readonly-dir').stat().st_mode) == 0o555
        assert (readonly_out / 'readonly-dir/payload').read_bytes() == PAYLOAD
        (readonly_out / 'readonly-dir').chmod(0o755)
        run('-qxf', readonly_archive, cwd=readonly_out)
        assert (readonly_out / 'readonly-file').read_bytes() == PAYLOAD
    finally:
        (readonly_out / 'readonly-dir').chmod(0o755)
        for name in ('readonly-dir/payload', 'readonly-file'):
            path = readonly_out / name
            if path.exists():
                path.chmod(0o644)

    wrong_directory = root / 'wrong-directory'
    wrong_directory.mkdir()
    (wrong_directory / 'nested').write_bytes(b'keep file')
    run('-qx', archive, cwd=wrong_directory, success=False)
    assert (wrong_directory / 'nested').read_bytes() == b'keep file'

    wrong_file = root / 'wrong-file'
    (wrong_file / 'nested/payload').mkdir(parents=True)
    for flags in ('-qx', '-qxf'):
        run(flags, archive, cwd=wrong_file, success=False)
        assert (wrong_file / 'nested/payload').is_dir()

    # Symlinks can require privileges on Windows. Other cases still run there.
    links = root / 'links'
    links.mkdir()
    outside = root / 'outside'
    outside.mkdir()
    try:
        (links / 'nested').symlink_to(outside, target_is_directory=True)
    except (OSError, NotImplementedError) as error:
        print('Skipping symlink cases:', error)
    else:
        for flags in ('-qx', '-qxf'):
            run(flags, archive, cwd=links, success=False)
            assert (links / 'nested').is_symlink()
            assert not list(outside.iterdir())
        (links / 'nested').unlink()
        (links / 'nested').mkdir()
        leaf = links / 'nested/payload'
        for existing in (False, True):
            external = outside / ('existing' if existing else 'missing')
            if existing:
                external.write_bytes(b'outside data')
            leaf.symlink_to(external)
            for flags in ('-qx', '-qxf'):
                run(flags, archive, cwd=links, success=False)
                assert leaf.is_symlink()
                if existing:
                    assert external.read_bytes() == b'outside data'
                else:
                    assert not external.exists()
            leaf.unlink()

    if hasattr(os, 'mkfifo'):
        special = root / 'special'
        (special / 'nested').mkdir(parents=True)
        fifo = special / 'nested/payload'
        os.mkfifo(fifo)
        for flags in ('-qx', '-qxf'):
            run(flags, archive, cwd=special, success=False)
            assert fifo.is_fifo()

    # Generated with fixture_archive.h: one empty TEST.DAT, level 4, with the
    # raw cdir cut immediately after EndOfCdir (XTAIL and serial omitted) before
    # compression. Outer lengths and the cdir checksum are correct.
    incomplete = root / 'missing-directory-tail.uc2'
    incomplete.write_bytes(base64.b64decode(
        'VUMyGkAAAAAUxLIBAAEAAAAhAAAA6pMAyADIAAB6vwABIgAAAAEAAQAAANixAcCDEA0r'
        'KikAiIKFxpVTAG58UECCxsAYTstkGHo/AAE='))
    rejected = root / 'rejected'
    rejected.mkdir()
    for flags in ('-qt', '-ql', '-qx'):
        result = run(flags, incomplete, cwd=rejected, success=False)
        assert b'Truncated' in result.stderr, result
        assert not list(rejected.iterdir())
    result = run('--ingest-restore', incomplete, cwd=rejected, success=False)
    assert result.stdout == b''

    # fixture_archive.h with TEST.DAT's parent changed from root to missing ID
    # 7 before compression; the payload is b'orphan' and all checksums match.
    orphan = root / 'missing-parent.uc2'
    orphan.write_bytes(base64.b64decode(
        'VUMyGlIAAAAmxLIBAAEAAAAnAAAAnsIAyADIAADsqyqsayh6vwABLgAAAAEAAQAAANmx'
        'ZjwYMzCISsZKw6JKIQClYJhxkcwM4sr9QwEIGgNjOC2QYQDGgEiHc6D3ABA='))
    for flags in ('-qt', '-ql', '-qx'):
        result = run(flags, orphan, cwd=rejected, success=False)
        assert b'missing parent directory' in result.stderr, result
        assert not list(rejected.iterdir())

print('CLI extraction safety checks passed')
