#!/usr/bin/env python3
"""Check extraction destinations and reject an incomplete directory trailer."""
import base64
import datetime
import io
import os
from pathlib import Path
import stat
import struct
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


def named_archive(name, *, ancestor):
    """Generate names the writer rejects without opening a source pathname."""
    def checksum(data):
        value = 0xA55A
        for offset in range(0, len(data), 2):
            value ^= int.from_bytes(data[offset:offset + 2], 'little')
        return value

    def turbo(data):
        literals = b''.join(b'\0' + data[i:i + 8] for i in range(0, len(data), 8))
        return struct.pack('<H', len(literals)) + literals + b'\0\0'

    payload = turbo(PAYLOAD)

    def entry(component, parent, directory_id=0):
        encoded = component.encode('cp850') + b'\0'
        metadata = struct.pack('<IBI11sBB', parent, 0x10 if directory_id else 0x20,
                               0, b'SAFE    TXT', 0, 1)
        if directory_id:
            record = b'\x01' + metadata + struct.pack('<I', directory_id)
        else:
            record = b'\x02' + metadata + struct.pack(
                '<IHIHIII', len(PAYLOAD), checksum(PAYLOAD), len(payload), 80, 1, 1, 29)
        return (record + b'AIP:Win95 LongN\0' + struct.pack('<I', len(encoded))
                + b'\0' + encoded)

    if ancestor:
        directory = entry(name, 0, 1) + entry('payload', 1)
    else:
        directory = entry(name, 0)
    directory += b'\x04' + b'\0' * 6 + b' ' * 11 + b'\0' * 4
    compressed = turbo(directory)
    size = 29 + len(payload) + 10 + len(compressed)
    header = struct.pack('<IIIBIIHBHHB', 0x1A324355, size - 13,
                         size - 13 + 0x01B2C3D4, 0, 1, 29 + len(payload),
                         checksum(directory), 0, 200, 200, 0)
    return header + payload + struct.pack('<IHI', len(compressed), 80, 1) + compressed


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

    # Selecting a directory itself does not suppress a later recursive
    # selection, even after one of its descendants has already been marked.
    for index, patterns in enumerate((('a', 'a/'),
                                      ('a/sub/first', 'a', 'a/'),
                                      ('a', 'a/', 'a', 'a/'))):
        listed = run('-qlT', siblings, *patterns, cwd=source).stdout
        names = [line.rsplit(b'\t', 1)[-1] for line in listed.splitlines()]
        assert sorted(names) == [b'a', b'a/sub', b'a/sub/first'], names
        destination = root / f'expanded-selection-{index}'
        destination.mkdir()
        run('-qx', siblings, *patterns, cwd=destination)
        actual = {p.relative_to(destination).as_posix(): p.read_bytes()
                  for p in destination.rglob('*') if p.is_file()}
        assert actual == {'a/sub/first': b'first'}, actual

    # Negated bracket sets must select the same entries on POSIX, Windows
    # and DOS, including ranges, a literal first ']', and literal hyphens.
    pattern_source = root / 'patterns'
    pattern_source.mkdir()
    pattern_names = ('a.txt', 'b.txt', 'c.txt', 'z.txt', '!.txt', '].txt', '-.txt', '[.txt')
    for name in pattern_names:
        (pattern_source / name).write_bytes(name.encode())
    pattern_archive = root / 'patterns.uc2'
    run('-qw', '--', pattern_archive, *pattern_names, cwd=pattern_source)
    for pattern, selected in (
            ('[!a].txt', set(pattern_names) - {'a.txt'}),
            ('[!a-c].txt', set(pattern_names) - {'a.txt', 'b.txt', 'c.txt'}),
            ('[a-c].txt', {'a.txt', 'b.txt', 'c.txt'}),
            ('[]a].txt', {'].txt', 'a.txt'}),
            ('[!]].txt', set(pattern_names) - {'].txt'}),
            ('[-a].txt', {'-.txt', 'a.txt'}),
            ('[[]' + '.txt', {'[.txt'}),
            (r'[\]a].txt', {'].txt', 'a.txt'})):
        result = run('-qlT', pattern_archive, pattern, cwd=root)
        names = {line.rsplit(b'\t', 1)[-1].decode() for line in result.stdout.splitlines()}
        assert names == selected, (pattern, names, selected)

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

    # Win32 aliases devices even with extensions and recognises superscript
    # COM/LPT digits. Generate archive bytes so fixture creation never opens a
    # device; selected ancestors must be checked before mkdir as well.
    devices = ['NUL', 'con', 'PrN', 'aUx', 'CONIN$', 'conout$']
    devices += [prefix + digit for prefix in ('COM', 'lpt')
                for digit in '123456789\u00b9\u00b2\u00b3']
    unsafe_names = [name + suffix for name in devices for suffix in ('', '.txt')]
    unsafe_names += ['trailing.', 'trailing ', 'NUL .txt', '...']
    unsafe_names += ['bad' + c + 'name' for c in '<>:"|?*\x01\x1f\x7f']
    valid_names = ['CONSOLE', 'auxiliary', 'NULx', 'COM0', 'LPT0', 'COM10',
                   'LPT10', 'COM1x.txt', 'COM\u00b9x', '.NUL']
    for index, name in enumerate(unsafe_names + valid_names):
        for ancestor in (False, True):
            named = root / f'name-{index}-{ancestor}.uc2'
            named.write_bytes(named_archive(name, ancestor=ancestor))
            destination = root / f'name-{index}-{ancestor}'
            destination.mkdir()
            rejected_name = os.name == 'nt' and name in unsafe_names
            patterns = ('*/payload',) if ancestor else ()
            result = run('-qxf', named, *patterns, cwd=destination,
                         success=not rejected_name)
            if rejected_name:
                assert b'unsafe archive entry name' in result.stderr, result
                assert not list(destination.iterdir())
            else:
                target = destination / name
                if ancestor:
                    target /= 'payload'
                assert target.read_bytes() == PAYLOAD, (name, ancestor)

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
