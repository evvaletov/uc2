#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Measure two verified directory versions against two separate tar ingests."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import subprocess
import tarfile
import tempfile
import time

from uc2 import snapshot


def hashes(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(root.rglob('*')) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uc2', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    binary = str(args.uc2.resolve())
    with tempfile.TemporaryDirectory(prefix='uc2-snapshot-benchmark-') as temp:
        root = Path(temp); source = root/'source'; source.mkdir(); (source/'empty').mkdir()
        data = random.Random(20261107).randbytes(1024*1024)
        (source/'data.bin').write_bytes(data)
        (source/'duplicate.bin').write_bytes(data)
        (source/'text.txt').write_bytes(b'Incremental collection snapshot\n' * 32768)
        for i in range(40):
            (source/f'note-{i:02d}.txt').write_text(f'Note {i}: a small independent file.\n' * 3)
        repo = root/'repository'
        results=[]
        for version in (1,2):
            if version == 2:
                (source/'data.bin').write_bytes(data[:123456] + b'inserted content\n'*257 + data[123456:])
                (source/'note-00.txt').unlink()
                (source/'note-01.txt').rename(source/'renamed.txt')
                (source/'added.txt').write_bytes(b'New version of the collection.\n')
            expected = hashes(source)
            start = time.perf_counter(); result = snapshot.create(source, repo, label=f'version-{version}')
            result['create_seconds'] = time.perf_counter()-start
            start = time.perf_counter(); snapshot.restore(repo,result['snapshot'],root/f'restore-{version}')
            result['restore_seconds'] = time.perf_counter()-start
            assert hashes(root/f'restore-{version}') == expected
            tar = root/f'v{version}.tar'
            start = time.perf_counter()
            with tarfile.open(tar,'w',format=tarfile.PAX_FORMAT) as archive:
                archive.add(source,arcname='collection')
            ingested = root/f'v{version}.uc2'
            with tar.open('rb') as stream:
                subprocess.run([binary,'--ingest',str(ingested)],stdin=stream,check=True,capture_output=True)
            result['tar_ingest_create_seconds'] = time.perf_counter()-start
            result['tar_bytes'] = tar.stat().st_size
            result['tar_ingest_bytes'] = ingested.stat().st_size
            restored = root/f'v{version}-restored.tar'
            start=time.perf_counter()
            with restored.open('wb') as stream:
                subprocess.run([binary,'--ingest-restore',str(ingested)],stdout=stream,stderr=subprocess.PIPE,check=True)
            with tarfile.open(restored) as archive:
                observed={p.name.removeprefix('collection/'):hashlib.sha256(archive.extractfile(p).read()).hexdigest()
                          for p in archive if p.isfile()}
            result['tar_ingest_restore_and_verify_seconds']=time.perf_counter()-start
            assert observed == expected
            result['manifest_overhead_percent']=100*result['manifest_bytes']/result['total_size']
            result['version']=version
            results.append(result)
        # Verify damage is reported, then recover by restoring the exact chunk.
        chunk=next((repo/'chunks').rglob('*.uc2')); content=chunk.read_bytes(); chunk.write_bytes(b'damaged')
        try:
            snapshot.verify(repo,results[0]['snapshot'])
        except (OSError,ValueError):
            damage_detected=True
        else:
            raise AssertionError('corrupt chunk accepted')
        chunk.write_bytes(content)
        snapshot.verify(repo,results[0]['snapshot'])
        report={'corpus':'seed20261107: two identical1MiB random files,1MiB repeated text,40small files; then insert4369bytes,rename,delete,add',
                'versions':results,'corruption_detected':damage_detected,'recovered_and_verified':True,
                'notes':'Single local run. Snapshot restore includes file creation and SHA256 checks. Tar restore timing includes stream decode and member hashing; it does not write individual files. No automatic corruption repair or pruning.'}
        args.output.write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))


if __name__=='__main__':main()
