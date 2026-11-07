#!/usr/bin/env python3
"""Compare named-file sharing on edited public C sources; verify every restore."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def measure(command, cwd, report):
    started = time.perf_counter()
    with report.with_suffix('.log').open('wb') as log:
        subprocess.run(['/usr/bin/time', '-f', '%M', '-o', str(report),
                        *map(str, command)], cwd=cwd, stdout=log, stderr=log,
                       timeout=600, check=True)
    return round(time.perf_counter() - started, 3), int(report.read_text())


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path, required=True, help='libarchive source checkout')
    ap.add_argument('--uc2', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True, help='new output directory')
    ap.add_argument('--repeat', type=int, default=3)
    args = ap.parse_args()
    root, source, cli = args.output.resolve(), args.source.resolve(), args.uc2.resolve()
    root.mkdir(parents=True, exist_ok=False)
    inputs = root / 'input'
    inputs.mkdir()
    paths = sorted((source / 'libarchive').glob('archive_read_support_format_*.c'))
    paths = sorted(paths, key=lambda p: (-p.stat().st_size, p.name))[:12]
    if len(paths) != 12 or args.repeat < 1:
        raise SystemExit('Need 12 libarchive format sources and repeat >= 1')
    provenance = {}
    for path in paths:
        data = path.read_bytes()
        provenance[path.name] = digest(path)
        for version in (0, 1):
            item = inputs / f'{version}-{path.name}'
            position = len(data) // 2
            changed = data[:position] + b'\n/* benchmark inserted edit */\n' + data[position:]
            item.write_bytes(changed if version else data)
            os.utime(item, (946684800, 946684800))
    expected = {p.name: digest(p) for p in inputs.iterdir()}
    results = []
    for trial in range(args.repeat):
        for chunked in (False, True):
            name = f'{trial}-' + ('chunks' if chunked else 'classic')
            archive, restored = root / (name + '.uc2'), root / (name + '-restored')
            restored.mkdir()
            flags = ['--chunked'] if chunked else []
            enc, rss = measure([cli, *flags, '-w', '-L', '4', archive, *sorted(expected)],
                               inputs, root / (name + '-create.time'))
            dec, dec_rss = measure([cli, '-q', '-d', restored, archive], root,
                                   root / (name + '-restore.time'))
            actual = {p.name: digest(p) for p in restored.iterdir()}
            if actual != expected:
                raise AssertionError('Restored bytes differ')
            row = dict(trial=trial, chunked=chunked, archive_bytes=archive.stat().st_size,
                       create_seconds=enc, restore_seconds=dec, peak_kib=rss,
                       restore_peak_kib=dec_rss, verified=True)
            results.append(row)
            print(json.dumps(row), flush=True)
    report = dict(platform=platform.platform(), cli_sha256=digest(cli),
                  source_revision=subprocess.check_output(
                      ['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip(),
                  source_hashes=provenance, input_bytes=sum(p.stat().st_size for p in inputs.iterdir()),
                  files=len(expected), results=results)
    (root / 'results.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
