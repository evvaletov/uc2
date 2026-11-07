#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fetch pinned historical archives and check migration against recorded hashes."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
from urllib.request import urlopen

from uc2.preserve import preserve

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, help='optional directory for downloaded archives')
    args = parser.parse_args()
    corpus = json.loads((ROOT / 'tests/archives/historical.json').read_text())
    with tempfile.TemporaryDirectory(prefix='uc2-historical-') as temporary:
        scratch = Path(temporary)
        cache = args.cache or scratch / 'sources'
        cache.mkdir(parents=True, exist_ok=True)
        inputs = []
        for item in corpus['archives']:
            source = cache / item['name']
            data = source.read_bytes() if source.exists() else b''
            if hashlib.sha256(data).hexdigest() != item['sha256']:
                with urlopen(item['url'], timeout=30) as response:
                    data = response.read(item['size'] + 1)
                if len(data) != item['size'] or hashlib.sha256(data).hexdigest() != item['sha256']:
                    raise ValueError(f"Source hash/size mismatch: {item['name']}")
                source.write_bytes(data)
            inputs.append(source)
        for format in ('zip', 'tar'):
            output = scratch / format
            report = preserve(inputs, output, format=format)
            if report['status'] != 'ok':
                raise ValueError(f'{format} migration failed: {report}')
            for item, record in zip(corpus['archives'], report['archives']):
                manifest = json.loads((output / record['manifest']).read_text())
                observed = [{key: entry[key] for key in item['expected_entries'][0]}
                            for entry in manifest['entries']]
                if observed != item['expected_entries']:
                    raise ValueError(f"Decoded metadata/hash mismatch: {item['name']}")
            print(f"{format}: {len(inputs)} historical archives, "
                  f"{sum(len(x['expected_entries']) for x in corpus['archives'])} entries verified")


if __name__ == '__main__':
    main()
