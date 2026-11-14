#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Exercise directory lookup, name collisions, ordering and version selection."""
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile


def checksum(data):
    value = 0xA55A
    for i in range(0, len(data), 2):
        value ^= int.from_bytes(data[i:i + 2], 'little')
    return value


def turbo(data):
    output = bytearray()
    for start in range(0, len(data), 24000):
        block = data[start:start + 24000]
        literals = b''.join(b'\0' + block[i:i + 8] for i in range(0, len(block), 8))
        output += struct.pack('<H', len(literals)) + literals
    return bytes(output) + b'\0\0'


def archive(records):
    directory = bytearray()
    for name, parent, directory_id in records:
        encoded = name.encode('cp850') + b'\0'
        metadata = struct.pack('<IBI11sBB', parent, 0x10 if directory_id else 0x20,
                               0, b'SAFE    TXT', 0, 1)
        if directory_id:
            directory += b'\x01' + metadata + struct.pack('<I', directory_id)
        else:
            directory += b'\x02' + metadata + struct.pack('<IHIHIII', 0, 0xA55A, 0, 80, 1, 1, 29)
        directory += b'AIP:Win95 LongN\0' + struct.pack('<I', len(encoded)) + b'\0' + encoded
    directory += b'\x04' + b'\0' * 6 + b' ' * 11 + b'\0' * 4
    compressed = turbo(directory)
    size = 29 + 10 + len(compressed)
    return (struct.pack('<IIIBIIHBHHB', 0x1A324355, size - 13,
                        size - 13 + 0x01B2C3D4, 0, 1, 29,
                        checksum(directory), 0, 200, 200, 0)
            + struct.pack('<IHI', len(compressed), 80, 1) + compressed)


def expected(records):
    """Small independent reference: explicit lists and incremented versions."""
    root = {'children': [], 'path': ''}
    directories = {}
    for name, parent_id, directory_id in records:
        parent = directories[parent_id] if parent_id else root
        children = parent['children']
        node = dict(name=name, path=parent['path'] + name, version=0,
                    directory=bool(directory_id), children=[])
        at = len(children)
        if not directory_id:
            for i, old in enumerate(children):
                if old['name'] == name:
                    old['version'] += 1
                    at = min(at, i)
        children.insert(at, node)
        if directory_id:
            node['path'] += '/'
            directories.setdefault(directory_id, node)

    def walk(parent):
        for node in parent['children']:
            if not node['directory']:
                yield node
        for node in parent['children']:
            if node['directory']:
                yield node
                yield from walk(node)

    return [(n['path'].rstrip('/'), n['version'], n['directory']) for n in walk(root)]


def listing(cli, path, *flags, success=True):
    result = subprocess.run([cli, *flags, str(path)], capture_output=True, timeout=30)
    assert (result.returncode == 0) == success, result
    if not success:
        return result
    rows = []
    for line in result.stdout.decode('utf-8').splitlines():
        fields = line.split('\t')
        assert len(fields) in (4, 5), fields
        assert fields[0] in ('a-----', '-d----') and fields[1] == '', fields
        directory = fields[0] == '-d----'
        assert fields[2] == ('' if directory else '0'), fields
        rows.append((fields[3], int(fields[4]) if len(fields) > 4 else 0, directory))
    return rows


def main():
    cli = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix='uc2-cli-metadata-') as temporary:
        path = Path(temporary) / 'metadata.uc2'
        # Deliberately include duplicate IDs and a file/directory sharing a name;
        # readers must retain the old first-parent and version-order behaviour.
        records = [('same', 0, 1), ('same', 0, 0), ('same', 0, 2),
                   ('same', 0, 0), ('old-parent', 0, 3), ('new-parent', 0, 3),
                   ('child', 3, 0), ('café', 0, 0), ('café', 0, 0)]
        records += [(f'dir-{i}', 0, i + 10) for i in range(600)]
        records += [(f'name-{i}', 10 + i % 600, 0) for i in range(1600)]
        rng = random.Random(12)
        records += [(f'file-{rng.randrange(200)}', 0, 0) for _ in range(1200)]
        path.write_bytes(archive(records))
        assert listing(cli, path, '-qalT') == expected(records)

        records = [('first', 0, 0), ('other', 0, 0), ('first', 0, 0),
                   ('directory', 0, 1), ('nested', 1, 0), ('nested', 1, 0)]
        path.write_bytes(archive(records))
        assert listing(cli, path, '-qlT') == [r for r in expected(records) if r[1] == 0]
        selected = subprocess.run([cli, '-qlT', str(path), 'first;1', 'directory/nested;1'],
                                  capture_output=True, timeout=30, check=True)
        assert selected.stdout.decode().splitlines() == [
            'a-----\t\t0\tfirst\t1', 'a-----\t\t0\tdirectory/nested\t1']
        path.write_bytes(archive([('missing', 999, 0)]))
        assert b'missing parent directory' in listing(cli, path, '-qlT', success=False).stderr

        # A practical upper-size regression, with an explicit wall bound. The
        # performance benchmark separately records times on writer-made inputs.
        records = [(f'file-{i}', 0, 0) for i in range(100000)]
        path.write_bytes(archive(records))
        rows = listing(cli, path, '-qalT')
        assert rows == [(f'file-{i}', 0, False) for i in range(100000)]
    print('CLI metadata checks passed')


if __name__ == '__main__':
    if not __debug__:
        raise RuntimeError('validation requires Python without -O or PYTHONOPTIMIZE')
    main()
