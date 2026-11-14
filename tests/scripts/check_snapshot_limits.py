#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Opt-in checks at the actual default snapshot limits (no lowered constants).

Creates 100,001 empty files and a roughly 1–2 GiB repetitive file. Retains all
fixtures. The manifest-size cases use explicitly synthetic padded manifests.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import random
import sqlite3
import time

from uc2 import snapshot as s


def rejected(function, phrase):
    try:
        function()
    except ValueError as error:
        assert phrase in str(error), str(error)
        return str(error)
    raise AssertionError('operation unexpectedly succeeded')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    args = parser.parse_args()
    root = args.work.resolve()
    root.mkdir()
    report = {'limits': {'entries': s.MAX_ENTRIES, 'references': s.MAX_REFS,
                         'manifest_bytes': s.MAX_MANIFEST}, 'complete': False}

    def save():
        (root / 'results.json').write_text(json.dumps(report, indent=2) + '\n')

    source = root / 'reference-source'
    source.mkdir()
    repo = root / 'reference-repository'
    s.create(source, repo)
    s.compact(repo)
    rng = random.Random(12)
    while True:
        candidate = rng.randbytes(s.MAX_CHUNK)
        length = s._native.chunk_length(candidate)
        if 2048 <= length <= 4096:
            block = candidate[:length]
            break
    assert s._native.chunk_length(block * 2) == len(block)
    data = source / 'repeated'
    with data.open('wb') as stream:
        for _ in range(s.MAX_REFS // 1000):
            stream.write(block * 1000)
        stream.write(block * (s.MAX_REFS % 1000))
    started = time.monotonic()
    accepted = s.create(source, repo)
    s.verify(repo, accepted['snapshot'])
    with data.open('ab') as stream:
        stream.write(block)
    before = s.snapshots(repo)
    message = rejected(lambda: s.create(source, repo), 'chunk-reference limit')
    assert s.snapshots(repo) == before
    s.verify(repo, accepted['snapshot'])
    report['references'] = dict(at_limit_accepted=True, over_limit_rejected=message,
                                block_bytes=len(block), bytes_at_limit=len(block) * s.MAX_REFS,
                                seconds=time.monotonic() - started)
    save()
    print(json.dumps(report['references']), flush=True)

    source = root / 'entry-source'
    source.mkdir()
    repo = root / 'entry-repository'
    started = time.monotonic()
    for i in range(s.MAX_ENTRIES):
        (source / f'file-{i:06d}').touch()
    accepted = s.create(source, repo)
    assert accepted['files'] == s.MAX_ENTRIES
    s.verify(repo, accepted['snapshot'])
    (source / 'one-more').touch()
    before = s.snapshots(repo)
    message = rejected(lambda: s.create(source, repo), 'entry limit')
    assert s.snapshots(repo) == before
    s.verify(repo, accepted['snapshot'])
    report['entries'] = dict(at_limit_accepted=True, over_limit_rejected=message,
                             seconds=time.monotonic() - started)
    save()
    print(json.dumps(report['entries']), flush=True)

    source = root / 'manifest-source'
    source.mkdir()
    repo = root / 'manifest-repository'
    first = s.create(source, repo)['snapshot']
    manifest = json.loads((repo / 'snapshots' / (first + '.json')).read_bytes())
    manifest['padding'] = ''  # Unknown descriptive fields are permitted.
    data = s._json(manifest)
    manifest['padding'] = 'x' * (s.MAX_MANIFEST - len(data))
    data = s._json(manifest)
    assert len(data) == s.MAX_MANIFEST
    address = hashlib.sha256(data).hexdigest()
    (repo / 'snapshots' / (address + '.json')).write_bytes(data)
    s.verify(repo, address)
    data += b'\n'
    over = hashlib.sha256(data).hexdigest()
    (repo / 'snapshots' / (over + '.json')).write_bytes(data)
    message = rejected(lambda: s.verify(repo, over), 'limit')
    report['manifest'] = dict(synthetic_padding=True, at_limit_accepted=True,
                              over_limit_rejected=message)

    packed = root / 'packed-manifest-repository'
    s.create(source, packed)
    s.compact(packed)
    with sqlite3.connect(packed / 'objects.sqlite3') as db:
        db.execute('INSERT INTO manifests VALUES (?, ?)', (address, gzip.compress(data[:-1], mtime=0)))
        db.execute('INSERT INTO manifests VALUES (?, ?)', (over, gzip.compress(data, mtime=0)))
    s.verify(packed, address)
    message = rejected(lambda: s.verify(packed, over), 'manifest exceeds 64 MiB')
    report['packed_manifest'] = dict(synthetic_padding=True, at_limit_accepted=True,
                                     over_limit_rejected=message)
    report['complete'] = True
    save()
    print(json.dumps(report['manifest']), flush=True)


if __name__ == '__main__':
    if not __debug__:
        raise RuntimeError('validation requires Python without -O or PYTHONOPTIMIZE')
    main()
