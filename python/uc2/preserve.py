# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Verified inventory and ZIP/tar migration of UC2 collections."""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import sys
import tarfile
import tempfile
import zipfile

from . import UC2File, ResourceLimitError, __version__

SCHEMA = "uc2-preservation-1"
BLOCK = 1024 * 1024


class _OutputError(OSError):
    """A destination failure aborts the staged batch, not just one source."""


def _output_call(function, *args, **kwargs):
    try:
        return function(*args, **kwargs)
    except (OSError, ValueError, RuntimeError) as error:
        raise _OutputError(str(error)) from error


@contextmanager
def _spool(output):
    spool = _output_call(tempfile.TemporaryFile, 'w+b', dir=output)
    try:
        yield spool
    finally:
        _output_call(spool.close)


def _hash(source):
    digest = hashlib.sha256()
    for block in iter(lambda: source.read(BLOCK), b""):
        digest.update(block)
    return digest.hexdigest()


def _leaf(name):
    # A numbered parent makes versions and case-folding collisions independent.
    leaf = re.sub(r'[^A-Za-z0-9._-]', '_', name.rsplit('/', 1)[-1])[:100]
    leaf = leaf.strip('. ') or 'entry'
    return 'entry-' + leaf


class _HashedWriter:
    def __init__(self, output):
        self.output = output
        self.digest = hashlib.sha256()

    def write(self, data):
        count = _output_call(self.output.write, data)
        if count != len(data):
            raise _OutputError('short write to destination spool')
        self.digest.update(data)
        return len(data)


def _metadata(info, index):
    return {
        'index': index, 'path': info.filename, 'version': info.version,
        'kind': 'directory' if info.is_dir() else 'file',
        'size': info.file_size, 'compressed_size': info.compress_size,
        'method': info.method, 'dos_attributes': info.attributes,
        'dos_time': info.dos_time,
        'timestamp_local': info.date_time.isoformat() if info.date_time else None,
        'status': 'directory' if info.is_dir() else 'pending',
        'sha256': None, 'export_path': None,
    }


def _write_member(container, spool, entry, info, format):
    name = f"files/{entry['index']:06d}/{_leaf(info.filename)}"
    spool.seek(0)
    if format == 'zip':
        item = zipfile.ZipInfo(name)
        if info.date_time:
            item.date_time = info.date_time.timetuple()[:6]
        item.compress_type = zipfile.ZIP_DEFLATED
        item.create_system = 0
        item.external_attr = (info.attributes & 0x27) | 0x20
        with container.open(item, 'w', force_zip64=True) as target:
            shutil.copyfileobj(spool, target, BLOCK)
    else:
        item = tarfile.TarInfo(name)
        item.size = info.file_size
        item.mode = 0o444 if info.attributes & 1 else 0o644
        # UC2 has no timezone, so an epoch value would invent metadata.
        item.mtime = 0
        item.pax_headers = {
            'UC2.path': info.filename,
            'UC2.version': str(info.version),
            'UC2.dos_time': str(info.dos_time),
            'UC2.dos_attributes': str(info.attributes),
        }
        container.addfile(item, spool)
    entry['export_path'] = name


def _verify_export(path, entries, format):
    expected = {e['export_path']: e for e in entries if e['export_path']}
    if format == 'zip':
        with zipfile.ZipFile(path) as archive:
            if archive.namelist() != list(expected):
                raise OSError('exported ZIP member list differs from the manifest')
            for name, entry in expected.items():
                with archive.open(name) as source:
                    if _hash(source) != entry['sha256']:
                        raise OSError(f'exported ZIP hash mismatch: {name}')
    else:
        with tarfile.open(path, 'r:') as archive:
            if archive.getnames() != list(expected):
                raise OSError('exported tar member list differs from the manifest')
            for name, entry in expected.items():
                with archive.extractfile(name) as source:
                    if _hash(source) != entry['sha256']:
                        raise OSError(f'exported tar hash mismatch: {name}')


def _one(path, output, format, limits):
    manifest = {
        'schema': SCHEMA, 'uc2_version': __version__,
        'source': {'path': str(path), 'size': None, 'sha256': None},
        'status': 'error', 'entries': [], 'errors': [],
        'export': None,
        'metadata_notes': [
            'All stored versions are listed; version 0 is newest.',
            'Original paths, directory records, DOS attributes and raw timestamps are retained in this manifest.',
            'Exported files use numbered paths to avoid collisions and unsafe original names.',
            'DOS timestamps have no timezone. ZIP retains valid local timestamps; tar mtime is zero with UC2 metadata in PAX headers.',
            'UC2 master blocks, compressed streams and arbitrary tags are not reproduced in ZIP/tar. Keep the source archive.',
            'SHA-256 records decoded bytes; UC2 checksum validation is not cryptographic authentication.',
        ],
    }
    container = None
    export = output / ('content.' + format) if format != 'inventory' else None
    try:
        if path.is_symlink():
            raise OSError('source archive must not be a symlink')
        flags = os.O_RDONLY | getattr(os, 'O_BINARY', 0) | getattr(os, 'O_NONBLOCK', 0) | getattr(os, 'O_NOFOLLOW', 0)
        fd = os.open(path, flags)
        try:
            if not stat.S_ISREG(os.fstat(fd).st_mode):
                raise OSError('source archive must be a regular file')
            source_file = os.fdopen(fd, 'rb')
        except BaseException:
            os.close(fd)
            raise
        with source_file as source:
            st = os.fstat(source.fileno())
            if st.st_size > limits['max_archive_size']:
                raise ResourceLimitError('archive exceeds max_archive_size')
            manifest['source']['size'] = st.st_size
            manifest['source']['sha256'] = _hash(source)
            source.seek(0)
            reader_limits = {k: v for k, v in limits.items() if k != 'max_archive_size'}
            with UC2File(source, **reader_limits) as archive:
                infos = archive.infolist(all_versions=True)
                if format == 'zip':
                    container = _output_call(zipfile.ZipFile, export, 'w', allowZip64=True)
                elif format == 'tar':
                    container = _output_call(tarfile.open, export, 'w', format=tarfile.PAX_FORMAT)
                attempted_size = 0
                for index, info in enumerate(infos):
                    entry = _metadata(info, index)
                    manifest['entries'].append(entry)
                    if info.is_dir():
                        continue
                    try:
                        if info.file_size > limits['max_extract_size']:
                            raise ResourceLimitError('entry exceeds max_extract_size')
                        if info.file_size > limits['max_total_size'] - attempted_size:
                            raise ResourceLimitError('entry exceeds remaining max_total_size')
                        attempted_size += info.file_size
                        with _spool(output) as spool:
                            target = _HashedWriter(spool)
                            archive.copy_to(info, target)
                            entry['sha256'] = target.digest.hexdigest()
                            if container:
                                _output_call(_write_member, container, spool, entry, info, format)
                        entry['status'] = 'verified'
                    except _OutputError:
                        raise
                    except (OSError, ValueError, RuntimeError) as error:
                        entry['status'] = 'unsupported' if info.method == 80 and not isinstance(error, ResourceLimitError) else 'error'
                        entry['sha256'] = None
                        entry['error'] = str(error)
                if container:
                    _output_call(container.close)
                    container = None
                    _output_call(_verify_export, export, manifest['entries'], format)
                    with export.open('rb') as content:
                        manifest['export'] = {'path': export.name, 'format': format,
                                              'sha256': _hash(content), 'verified': True}
                source.seek(0)
                if _hash(source) != manifest['source']['sha256']:
                    raise OSError('source archive changed during the operation')
                manifest['status'] = ('partial' if any(e['status'] in ('error', 'unsupported')
                                                     for e in manifest['entries']) else 'ok')
    except _OutputError:
        raise
    except (OSError, ValueError, RuntimeError) as error:
        manifest['errors'].append(str(error))
        if export and export.exists():
            export.unlink()
        manifest['export'] = None
        for entry in manifest['entries']:
            entry['export_path'] = None
    finally:
        if container:
            _output_call(container.close)
    (output / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=True, indent=2) + '\n', encoding='utf-8')
    return manifest


def _sources(inputs):
    result = []
    seen = set()
    for item in inputs:
        path = Path(item)
        if path.is_dir() and not path.is_symlink():
            candidates = []
            for parent, directories, files in os.walk(path, followlinks=False):
                directories[:] = sorted(d for d in directories if not (Path(parent) / d).is_symlink())
                candidates.extend(Path(parent) / f for f in sorted(files)
                                  if f.lower().endswith('.uc2') and not (Path(parent) / f).is_symlink())
        else:
            candidates = [path]
        for candidate in candidates:
            key = candidate.parent.resolve() / candidate.name
            if key not in seen:
                seen.add(key)
                result.append(candidate)
    if not result:
        raise ValueError('no UC2 archives found')
    return result


def _publish(stage, destination):
    if os.name == 'nt':
        # Windows rename refuses an existing destination, including directories.
        os.rename(stage, destination)
        return
    # Reserve our own empty directory exclusively before POSIX rename replaces it.
    # This closes the check/rename race with another creator in the trusted parent.
    destination.mkdir()
    reservation = destination.stat()
    try:
        os.rename(stage, destination)
    except BaseException:
        try:
            current = destination.stat()
            if (current.st_dev, current.st_ino) == (reservation.st_dev, reservation.st_ino):
                destination.rmdir()
        except OSError:
            pass
        raise


def preserve(inputs, output, *, format='inventory', max_archive_size=1024**3,
             max_entries=100_000, max_decoder_memory=64 * 1024**2,
             max_extract_size=1024**3, max_total_size=4 * 1024**3):
    """Publish a new batch directory containing verified manifests and exports.

    Source archives are opened read-only. Existing output directories are refused.
    Source failures are recorded and do not stop the rest of a batch. Destination
    failures abort the staged batch. The output parent must be under your control.
    The returned report has status ``ok`` or ``partial``. Resource limits apply
    per source archive, including all stored file versions.
    """
    if format not in ('inventory', 'zip', 'tar'):
        raise ValueError('format must be inventory, zip or tar')
    limits = dict(max_archive_size=max_archive_size, max_entries=max_entries,
                  max_decoder_memory=max_decoder_memory, max_extract_size=max_extract_size,
                  max_total_size=max_total_size)
    from . import _positive
    for name, value in limits.items():
        _positive(name, value)
    sources = _sources(inputs)
    destination = Path(output).absolute()
    if destination.exists() or destination.is_symlink():
        raise FileExistsError(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.uc2-preserve-', dir=destination.parent))
    try:
        report = {'schema': SCHEMA, 'uc2_version': __version__,
                  'created_utc': datetime.now(timezone.utc).isoformat(),
                  'format': format, 'limits': limits, 'archives': [], 'status': 'ok'}
        for index, source in enumerate(sources):
            folder = f'{index:06d}-{_leaf(source.name)}'
            target = stage / folder
            target.mkdir()
            manifest = _one(source, target, format, limits)
            report['archives'].append({'source': str(source), 'status': manifest['status'],
                                       'manifest': folder + '/manifest.json',
                                       'sha256': manifest['source']['sha256']})
            if manifest['status'] != 'ok':
                report['status'] = 'partial'
        (stage / 'inventory.json').write_text(json.dumps(report, ensure_ascii=True, indent=2) + '\n', encoding='utf-8')
        _publish(stage, destination)
        return report
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def main(argv=None):
    parser = argparse.ArgumentParser(description='Inventory and migrate UC2 archives with verified file hashes.')
    parser.add_argument('inputs', nargs='+', help='archives or directories searched recursively for .uc2 files')
    parser.add_argument('-o', '--output', required=True, help='new output directory (must not already exist)')
    parser.add_argument('--format', choices=('inventory', 'zip', 'tar'), default='inventory')
    parser.add_argument('--max-archive-size', type=int, default=1024**3, metavar='BYTES')
    parser.add_argument('--max-entries', type=int, default=100_000)
    parser.add_argument('--max-decoder-memory', type=int, default=64 * 1024**2, metavar='BYTES')
    parser.add_argument('--max-extract-size', type=int, default=1024**3, metavar='BYTES')
    parser.add_argument('--max-total-size', type=int, default=4 * 1024**3, metavar='BYTES')
    args = vars(parser.parse_args(argv))
    try:
        report = preserve(**args)
    except (OSError, ValueError) as error:
        print(f'uc2-preserve: {error}', file=sys.stderr)
        return 2
    print(f"{len(report['archives'])} archives; {report['status']}; {args['output']}/inventory.json")
    return 0 if report['status'] == 'ok' else 1


if __name__ == '__main__':
    raise SystemExit(main())
