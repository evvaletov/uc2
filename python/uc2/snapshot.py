# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Incremental directory snapshots using shared, verified UC2 chunks.

The repository is a directory, not a legacy monolithic UC2 archive. Source,
repository and destination parents must be trusted and remain stable while an
operation runs. Symlinks and special files are rejected. No pruning is implicit.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import datetime as _datetime
import errno
import hashlib
import gzip
import io
import sqlite3
import zlib
from contextvars import ContextVar
from functools import wraps
import json
import os
from pathlib import Path
import re
import shutil
import stat
import tempfile
import unicodedata

from . import UC2File, UC2Writer, _native

FORMAT = {"format": "uc2-snapshot", "version": 1, "chunker": "gear-13-2048-32768",
          "address": "sha256", "storage": "uc2-level-4"}
PACKED_FORMAT = dict(FORMAT, version=2, storage="sqlite-uc2-level-4")
_STATE = ContextVar("snapshot_operation", default={})
MAX_CHUNK = 32768
MAX_MANIFEST = 64 * 1024 * 1024
MAX_ENTRIES = 100_000
MAX_REFS = 500_000
MAX_OBJECTS = 2_000_000
MAX_SNAPSHOTS = 100_000
DIGEST = re.compile(r"[0-9a-f]{64}\Z")
RESERVED = re.compile(r"(?:con|conin\$|conout\$|prn|aux|nul|com[1-9¹²³]|lpt[1-9¹²³])\Z", re.I)


def _json(value):
    return (json.dumps(value, ensure_ascii=True, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("ascii")


def _hash(data):
    return hashlib.sha256(data).hexdigest()


def _digest(value):
    if not isinstance(value, str) or not DIGEST.fullmatch(value):
        raise ValueError("invalid SHA-256 address")
    return value


def _reparse(info):
    return bool(getattr(info, "st_file_attributes", 0) &
                getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))


def _directory(path):
    info = path.lstat()
    if _reparse(info) or not stat.S_ISDIR(info.st_mode):
        raise ValueError(f"expected a real directory: {path}")


@contextmanager
def _regular(path):
    before = path.lstat()
    if _reparse(before) or not stat.S_ISREG(before.st_mode):
        raise ValueError(f"expected a regular file: {path}")
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
    flags |= getattr(os, "O_NONBLOCK", 0)
    fd = os.open(path, flags)
    with os.fdopen(fd, "rb") as source:
        opened = os.fstat(source.fileno())
        if _reparse(opened) or not stat.S_ISREG(opened.st_mode) or (before.st_dev, before.st_ino) != (opened.st_dev, opened.st_ino):
            raise ValueError(f"source changed while opening: {path}")
        yield source


def _read(path, limit):
    with _regular(path) as source:
        if os.fstat(source.fileno()).st_size > limit:
            raise ValueError(f"file exceeds size limit: {path}")
        data = source.read(limit + 1)
    if len(data) > limit:
        raise ValueError(f"file exceeds size limit: {path}")
    return data


def _sync_directory(path):
    # Windows has no portable directory fsync. File data is flushed on both.
    if os.name == "posix":
        fd = os.open(path, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
        try:
            os.fsync(fd)
        finally:
            os.close(fd)


def _publish_file(temporary, destination):
    with temporary.open("r+b") as source:
        os.fsync(source.fileno())
    # Atomic, exclusive publication, including on NTFS; never overwrite a chunk.
    try:
        os.link(temporary, destination)
    except OSError as error:
        if (error.errno not in (errno.EPERM, errno.ENOTSUP, errno.EOPNOTSUPP, errno.ENOSYS)
                and getattr(error, "winerror", None) not in (1, 50)):
            raise
        # FAT/exFAT and some network mounts have no hardlinks. All callers
        # own the writer lock (or a new repository during initialization),
        # and parents are trusted. Readers see only the completed rename.
        if destination.exists() or destination.is_symlink():
            raise FileExistsError(destination) from error
        temporary.rename(destination)
    else:
        temporary.unlink()
    _sync_directory(destination.parent)


def _save(destination, data):
    fd, name = tempfile.mkstemp(prefix=".pending-", dir=destination.parent)
    temporary = Path(name)
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        try:
            _publish_file(temporary, destination)
        except FileExistsError:
            if _read(destination, len(data)) != data:
                raise ValueError("existing content-addressed file differs")
            return False
        return True
    finally:
        temporary.unlink(missing_ok=True)


def _repository(repository, *, create=False):
    root = Path(repository).absolute()
    if create:
        root.mkdir(exist_ok=True)
    _directory(root)
    config = root / "config.json"
    if create and not config.exists():
        # Refuse to adopt an unrelated or incomplete directory.
        if any(root.iterdir()):
            raise ValueError("repository has no config and is not empty")
        for name in ("chunks", "snapshots"):
            (root / name).mkdir()
        _save(config, _json(FORMAT))
    if json.loads(_read(config, 4096)) not in (FORMAT, PACKED_FORMAT):
        raise ValueError("unsupported snapshot repository format")
    for name in ("chunks", "snapshots"):
        _directory(root / name)
    return root


def _connect(path, *, create=False):
    if not create:
        with _regular(path):
            pass
    connection = sqlite3.connect(path.as_uri() + ("?mode=rwc" if create else "?mode=rw"), uri=True)
    try:
        connection.execute("PRAGMA trusted_schema=OFF")
        connection.execute("PRAGMA synchronous=FULL")
        if hasattr(connection, "setlimit"):
            connection.setlimit(sqlite3.SQLITE_LIMIT_LENGTH, MAX_MANIFEST + 1024 * 1024)
        return connection
    except BaseException:
        connection.close()
        raise


def _db(root):
    return _STATE.get().get(str(root), (None, {}))[0]


@contextmanager
def _lock(root):
    # This exclusive sentinel is also understood by v1 writers. All v2
    # readers participate, so maintenance cannot remove an in-use object.
    path = root / ".writer-lock"
    lock = path.open("x", encoding="ascii")
    connection = None
    token = None
    try:
        with lock:
            lock.write(str(os.getpid()) + "\n")
        if json.loads(_read(root / "config.json", 4096)) == PACKED_FORMAT:
            connection = _connect(root / "objects.sqlite3")
        token = _STATE.set(dict(_STATE.get(), **{str(root): (connection, {}, set())}))
        yield
        if connection is not None:
            connection.commit()
    finally:
        if token is not None:
            _STATE.reset(token)
        try:
            if connection is not None:
                connection.close()
        finally:
            path.unlink()


def _locked(function):
    @wraps(function)
    def call(repository, *args, **kwargs):
        root = _repository(repository)
        with _lock(root):
            return function(root, *args, **kwargs)
    return call


def _addresses(root):
    connection = _db(root)
    result = []
    def add(digest):
        if len(result) >= MAX_SNAPSHOTS:
            raise ValueError("repository snapshot-count limit exceeded")
        result.append(_digest(digest))
    if connection is not None:
        for row in connection.execute("SELECT digest FROM manifests"):
            add(row[0])
    else:
        with os.scandir(root / "snapshots") as entries:
            for entry in entries:
                if entry.name.endswith(".json") and DIGEST.fullmatch(entry.name[:-5]):
                    add(entry.name[:-5])
    return sorted(result)


def _manifest_data(root, address):
    connection = _db(root)
    if connection is None:
        return _read(root / "snapshots" / (address + ".json"), MAX_MANIFEST)
    row = connection.execute("SELECT data FROM manifests WHERE digest=?", (address,)).fetchone()
    if row is None:
        raise FileNotFoundError("missing snapshot " + address)
    if not isinstance(row[0], bytes):
        raise ValueError("manifest is not a blob")
    try:
        with gzip.GzipFile(fileobj=io.BytesIO(row[0])) as source:
            data = source.read(MAX_MANIFEST + 1)
    except (OSError, EOFError, zlib.error) as error:
        raise ValueError("invalid compressed manifest") from error
    if len(data) > MAX_MANIFEST:
        raise ValueError("manifest exceeds 64 MiB")
    return data


def _save_manifest(root, address, data):
    connection = _db(root)
    if connection is None:
        return len(data) if _save(root / "snapshots" / (address + ".json"), data) else 0
    if connection.execute("SELECT 1 FROM manifests WHERE digest=?", (address,)).fetchone():
        if _manifest_data(root, address) != data:
            raise ValueError("existing manifest differs")
        return 0
    compressed = gzip.compress(data, mtime=0)
    connection.execute("INSERT INTO manifests VALUES (?, ?)", (address, compressed))
    return len(compressed)


def _archive_data(root, digest):
    connection = _db(root)
    if connection is None:
        return _read(_chunk_path(root, digest), 1024 * 1024)
    row = connection.execute("SELECT data FROM chunks WHERE digest=?", (digest,)).fetchone()
    if row is None:
        raise FileNotFoundError("missing chunk " + digest)
    if not isinstance(row[0], bytes):
        raise ValueError("chunk is not a blob")
    if len(row[0]) > 1024 * 1024:
        raise ValueError("chunk archive exceeds 1 MiB")
    return row[0]


def _path(value):
    if not isinstance(value, str) or not value or len(value.encode("utf-8")) > 2048:
        raise ValueError("invalid snapshot path")
    parts = value.split("/")
    if len(parts) > 64:
        raise ValueError("snapshot path exceeds 64 components")
    for part in parts:
        if (part in ("", ".", "..") or part[-1:] in (".", " ") or
                RESERVED.fullmatch(part.partition(".")[0].rstrip(" ")) or len(part.encode("utf-8")) > 240 or
                any(ord(c) < 32 or ord(c) == 127 or c in '\\:< >"|?*'.replace(" ", "") for c in part)):
            raise ValueError(f"path cannot be restored portably: {value!r}")
    return tuple(parts)


def _key(value):
    return unicodedata.normalize("NFC", value).casefold()


def _integer(value, low, high):
    if type(value) is not int or not low <= value <= high:
        raise ValueError("invalid snapshot integer or resource limit")
    return value


def _metadata(path, kind, info=None):
    s = path.lstat() if info is None else info
    return {"type": kind, "mode": stat.S_IMODE(s.st_mode) & 0o777, "mtime_ns": s.st_mtime_ns}


def _chunk_path(root, digest, *, create=False):
    _digest(digest)
    directory = root / "chunks" / digest[:2]
    if create:
        try:
            directory.mkdir()
        except FileExistsError:
            pass
        else:
            _sync_directory(root / "chunks")
    _directory(directory)
    return directory / (digest + ".uc2")


def _chunk(root, digest, size):
    cache = _STATE.get().get(str(root), (None, {}))[1]
    if (digest, size) in cache:
        return cache[digest, size]
    with UC2File(io.BytesIO(_archive_data(root, digest)), max_entries=1,
                 max_decoder_memory=8 * 1024 * 1024, max_extract_size=MAX_CHUNK,
                             max_read_size=MAX_CHUNK) as archive:
        entries = archive.infolist(all_versions=True)
        if len(entries) != 1 or entries[0].is_dir() or entries[0].file_size != size:
            raise ValueError(f"invalid chunk archive: {digest}")
        data = archive.read(entries[0])
    if _hash(data) != digest:
        raise ValueError(f"chunk SHA-256 mismatch: {digest}")
    # Bounded by 256 * 32 KiB, independent of collection size.
    if len(cache) >= 256:
        cache.pop(next(iter(cache)))
    cache[digest, size] = data
    return data


def _store(root, data):
    digest = _hash(data)
    connection = _db(root)
    path = _chunk_path(root, digest, create=True) if connection is None else root / "objects.sqlite3"
    exists = (path.exists() or path.is_symlink()) if connection is None else connection.execute(
        "SELECT 1 FROM chunks WHERE digest=?", (digest,)).fetchone()
    if exists:
        if _chunk(root, digest, len(data)) != data:
            raise ValueError("chunk collision")
        return digest, 0
    if connection is not None:
        output = io.BytesIO()
        writer = _native.Writer(4, 1, MAX_CHUNK)
        try:
            writer.add_file(0, "chunk", 0, 0, len(data), io.BytesIO(data))
            writer.finish(output)
        finally:
            writer.abort()
        encoded = output.getvalue()
        with UC2File(io.BytesIO(encoded), max_entries=1, max_decoder_memory=8 * 1024 * 1024, max_extract_size=MAX_CHUNK,
                             max_read_size=MAX_CHUNK) as archive:
            if archive.read("chunk") != data:
                raise ValueError("new chunk did not round trip")
        connection.execute("INSERT INTO chunks VALUES (?, ?)", (digest, encoded))
        return digest, len(encoded)
    fd, name = tempfile.mkstemp(prefix=".pending-", dir=path.parent)
    os.close(fd)
    temporary = Path(name)
    try:
        with UC2Writer(temporary, level=4, max_entries=1) as archive:
            archive.writestr("chunk", data)
        # Verify the writer result before the chunk becomes reusable.
        with UC2File(temporary, max_entries=1, max_decoder_memory=8 * 1024 * 1024, max_extract_size=MAX_CHUNK,
                             max_read_size=MAX_CHUNK) as archive:
            if archive.read("chunk") != data:
                raise ValueError("new chunk did not round trip")
        size = temporary.stat().st_size
        _publish_file(temporary, path)
        return digest, size
    finally:
        temporary.unlink(missing_ok=True)


def _chunks(source):
    pending = b""
    while True:
        more = source.read(MAX_CHUNK - len(pending))
        pending += more
        if not pending:
            return
        if len(pending) == MAX_CHUNK or not more:
            length = _native.chunk_length(pending)
            yield pending[:length]
            pending = pending[length:]


def create(source, repository, *, label="", max_total_size=1 << 40):
    """Commit a directory version and return its address and measured growth.

    All files are read and hashed, including unchanged files. Reused chunks are
    decoded and checked. A failed run may leave unreferenced valid chunks, but
    never publishes an incomplete snapshot. Concurrent writers are refused.
    """
    _integer(max_total_size, 0, (1 << 63) - 1)
    if not isinstance(label, str) or len(label.encode("utf-8")) > 1024:
        raise ValueError("snapshot label must be at most 1024 UTF-8 bytes")
    source = Path(source).absolute()
    _directory(source)
    dest = Path(repository).resolve()
    if (dest == source.resolve() or source.resolve() in dest.parents or
            dest in source.resolve().parents):
        raise ValueError("repository and source must be outside each other")
    root = _repository(repository, create=True)
    with _lock(root):
        entries, keys = [], set()
        total = added = new_chunks = refs = 0
        directories = []
        def visit(directory, entry=None):
            nonlocal total, added, new_chunks, refs
            initial = directory.lstat()
            if _reparse(initial) or not stat.S_ISDIR(initial.st_mode):
                raise ValueError(f"expected a real directory: {directory}")
            if entry is not None:
                entry.update(_metadata(directory, "directory", initial))
            directories.append((directory, initial))
            with os.scandir(directory) as scan:
                children = sorted(scan, key=lambda e: e.name)
            for child in children:
                path = Path(child.path)
                relative = path.relative_to(source).as_posix()
                _path(relative)
                key = _key(relative)
                if key in keys:
                    raise ValueError(f"case or Unicode path collision: {relative}")
                keys.add(key)
                if len(keys) > MAX_ENTRIES:
                    raise ValueError("snapshot entry limit exceeded")
                path_before = path.lstat()
                mode = path_before.st_mode
                if _reparse(path_before):
                    raise ValueError(f"reparse points are unsupported: {path}")
                if stat.S_ISDIR(mode):
                    entry = dict(path=relative)
                    entries.append(entry)
                    visit(path, entry)
                elif stat.S_ISREG(mode):
                    entry = dict(path=relative, **_metadata(path, "file", path_before), chunks=[])
                    sha, size = hashlib.sha256(), 0
                    with _regular(path) as stream:
                        before = os.fstat(stream.fileno())
                        for data in _chunks(stream):
                            total += len(data)
                            if total > max_total_size:
                                raise ValueError("snapshot total size limit exceeded")
                            refs += 1
                            if refs > MAX_REFS:
                                raise ValueError("snapshot chunk-reference limit exceeded")
                            digest, growth = _store(root, data)
                            added += growth
                            new_chunks += bool(growth)
                            entry["chunks"].append([digest, len(data)])
                            sha.update(data)
                            size += len(data)
                        after = os.fstat(stream.fileno())
                        current = path.lstat()
                        signature = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns)
                        # Windows fstat and lstat can give ctime different
                        # meanings (change time versus creation time). Compare
                        # timestamps only within the same API, and identities
                        # across APIs. Keep both change checks on every OS.
                        if (signature(before) != signature(after) or
                                signature(path_before) != signature(current) or
                                (before.st_dev, before.st_ino) != (path_before.st_dev, path_before.st_ino) or
                                size != before.st_size):
                            raise ValueError(f"source changed during snapshot: {relative}")
                    entry.update(size=size, sha256=sha.hexdigest(), mtime_ns=before.st_mtime_ns,
                                 mode=stat.S_IMODE(before.st_mode) & 0o777)
                    entries.append(entry)
                else:
                    raise ValueError(f"symlink or special file is unsupported: {relative}")
        visit(source)
        for directory, before in directories:
            after = directory.lstat()
            if (before.st_dev, before.st_ino, before.st_mtime_ns, before.st_ctime_ns) != (after.st_dev, after.st_ino, after.st_mtime_ns, after.st_ctime_ns):
                raise ValueError(f"source directory changed during snapshot: {directory}")
        manifest = {"format": FORMAT, "created": datetime.now(timezone.utc).isoformat(),
                    "label": label, "root": _metadata(source, "directory", directories[0][1]),
                    "entries": entries, "total_size": total}
        data = _json(manifest)
        if len(data) > MAX_MANIFEST:
            raise ValueError("snapshot manifest exceeds 64 MiB")
        address = _hash(data)
        manifest_growth = _save_manifest(root, address, data)
        return {"snapshot": address, "files": sum(e["type"] == "file" for e in entries),
                "total_size": total, "new_chunks": new_chunks, "new_chunk_bytes": added,
                "manifest_bytes": len(data), "new_manifest_bytes": manifest_growth,
                "growth_bytes": added + manifest_growth}


def _created(value):
    try:
        parsed = _datetime.datetime.fromisoformat(value.replace("Z", "+00:00"))
        if parsed.utcoffset() is None:
            raise ValueError("missing timezone")
        return parsed.astimezone(timezone.utc)
    except (ValueError, TypeError, OverflowError) as error:
        raise ValueError("invalid snapshot creation timestamp") from error


def _manifest(root, address, max_total_size):
    _digest(address)
    data = _manifest_data(root, address)
    if _hash(data) != address:
        raise ValueError("manifest SHA-256 mismatch")
    m = json.loads(data)
    if not isinstance(m, dict) or m.get("format") != FORMAT:
        raise ValueError("unsupported snapshot manifest")
    if not isinstance(m.get("created"), str) or not isinstance(m.get("label"), str):
        raise ValueError("invalid snapshot description")
    _created(m["created"])
    entries = m.get("entries")
    if not isinstance(entries, list) or len(entries) > MAX_ENTRIES:
        raise ValueError("invalid snapshot entries")
    paths, total, refs = {"": "directory"}, 0, 0
    exact_paths = {""}
    def metadata(e):
        if not isinstance(e, dict) or e.get("type") not in ("file", "directory"):
            raise ValueError("invalid snapshot entry")
        _integer(e.get("mode"), 0, 0o777)
        _integer(e.get("mtime_ns"), -(1 << 63), (1 << 63) - 1)
        if e["type"] == "directory" and "chunks" in e:
            raise ValueError("directory has chunk references")
    metadata(m.get("root"))
    if m["root"]["type"] != "directory":
        raise ValueError("invalid snapshot root")
    for e in entries:
        metadata(e)
        parts = _path(e.get("path"))
        key = _key(e["path"])
        parent = _key("/".join(parts[:-1]))
        if key in paths or paths.get(parent) != "directory" or "/".join(parts[:-1]) not in exact_paths:
            raise ValueError("duplicate path or missing/invalid parent")
        paths[key] = e["type"]
        exact_paths.add(e["path"])
        if e["type"] == "file":
            _integer(e.get("size"), 0, max_total_size)
            _digest(e.get("sha256"))
            chunks = e.get("chunks")
            if not isinstance(chunks, list):
                raise ValueError("invalid chunk list")
            refs += len(chunks)
            if refs > MAX_REFS:
                raise ValueError("snapshot chunk-reference limit exceeded")
            size = 0
            for c in chunks:
                if not isinstance(c, list) or len(c) != 2:
                    raise ValueError("invalid chunk reference")
                _digest(c[0])
                size += _integer(c[1], 1, MAX_CHUNK)
            if size != e["size"]:
                raise ValueError("file and chunk sizes differ")
            total += size
            if total > max_total_size:
                raise ValueError("snapshot total size limit exceeded")
    if total != _integer(m.get("total_size"), 0, max_total_size):
        raise ValueError("invalid snapshot total size")
    return m


def _files(root, manifest, destination=None):
    checked = 0
    verified = _STATE.get().get(str(root), (None, {}, set()))[2]
    for e in manifest["entries"]:
        if e["type"] == "directory":
            if destination is not None:
                (destination / e["path"]).mkdir()
            continue
        identity = hashlib.sha256()
        identity.update(e["sha256"].encode("ascii"))
        identity.update(e["size"].to_bytes(8, "little"))
        for digest, size in e["chunks"]:
            identity.update(digest.encode("ascii"))
            identity.update(size.to_bytes(4, "little"))
        key = identity.digest()
        if destination is None and key in verified:
            checked += 1
            continue
        sha = hashlib.sha256()
        output = None
        try:
            if destination is not None:
                output = (destination / e["path"]).open("xb")
            for digest, size in e["chunks"]:
                data = _chunk(root, digest, size)
                sha.update(data)
                if output is not None:
                    output.write(data)
            if sha.hexdigest() != e["sha256"]:
                raise ValueError(f"file SHA-256 mismatch: {e['path']}")
            if output is not None:
                output.flush()
                os.fsync(output.fileno())
            checked += 1
            if len(verified) < MAX_ENTRIES:
                verified.add(key)
        finally:
            if output is not None:
                output.close()
    return checked


@_locked
def verify(repository, address, *, max_total_size=1 << 40):
    """Verify one manifest, all referenced chunks, and complete file hashes."""
    _integer(max_total_size, 0, (1 << 63) - 1)
    root = _repository(repository)
    manifest = _manifest(root, address, max_total_size)
    return {"snapshot": address, "verified_files": _files(root, manifest),
            "total_size": manifest["total_size"]}


def _restore_metadata(path, metadata, directory=False):
    # Open before chmod: archived directories may have mode 000. Sync both
    # their entries and final metadata before publishing the restored tree.
    fd = None
    if os.name == "posix":
        fd = os.open(path, os.O_RDONLY | (getattr(os, "O_DIRECTORY", 0) if directory else 0))
    try:
        os.utime(path, ns=(metadata["mtime_ns"],) * 2)
        os.chmod(path, metadata["mode"])
        if fd is not None:
            os.fsync(fd)
    finally:
        if fd is not None:
            os.close(fd)


@_locked
def restore(repository, address, destination, *, max_total_size=1 << 40):
    """Verify and restore into a NEW directory before publishing it.

    A pre-publication failure leaves no partial destination. A final directory
    sync error may be reported after the complete destination is published.
    Cleanup errors propagate; an unavailable filesystem may retain staging.
    """
    _integer(max_total_size, 0, (1 << 63) - 1)
    root = _repository(repository)
    manifest = _manifest(root, address, max_total_size)
    destination = Path(destination).absolute()
    if destination.exists() or destination.is_symlink():
        raise FileExistsError(destination)
    _directory(destination.parent)
    if root.resolve() == destination.resolve() or root.resolve() in destination.resolve().parents:
        raise ValueError("restore destination must be outside the repository")
    stage = Path(tempfile.mkdtemp(prefix=".uc2-restore-", dir=destination.parent))
    reserved = False
    try:
        checked = _files(root, manifest, stage)
        # Children first: readonly directories must not prevent restoration.
        for e in reversed(manifest["entries"]):
            path = stage / e["path"]
            _restore_metadata(path, e, e["type"] == "directory")
        _restore_metadata(stage, manifest["root"], True)
        if os.name == "posix":
            # Reserve the name exclusively; parents are trusted during restore.
            destination.mkdir()
            reserved = True
        stage.rename(destination)
        reserved = False
        _sync_directory(destination.parent)
        return {"snapshot": address, "restored_files": checked,
                "total_size": manifest["total_size"]}
    finally:
        try:
            if reserved and stage.exists():
                destination.rmdir()
        finally:
            if stage.exists():
                # A metadata failure may have made directories readonly.
                os.chmod(stage, 0o700)
                for directory, dirs, files in os.walk(stage):
                    for name in dirs:
                        os.chmod(Path(directory) / name, 0o700)
                    for name in files:
                        os.chmod(Path(directory) / name, 0o600)
                shutil.rmtree(stage)


@_locked
def snapshots(repository):
    """List committed snapshot IDs with validated manifests."""
    root = _repository(repository)
    result = []
    for address in _addresses(root):
        m = _manifest(root, address, (1 << 63) - 1)
        result.append({"snapshot": address, "created": m["created"],
                       "label": m["label"], "total_size": m["total_size"]})
    return result


def _inventory(root):
    connection = _db(root)
    result = {}
    def add(digest, size):
        if len(result) >= MAX_OBJECTS:
            raise ValueError("repository chunk-count limit exceeded (includes orphans)")
        result[_digest(digest)] = _integer(size, 0, (1 << 63) - 1)
    if connection is not None:
        for digest, size in connection.execute("SELECT digest, length(data) FROM chunks"):
            add(digest, size)
        return result
    with os.scandir(root / "chunks") as directories:
        for entry in directories:
            if not re.fullmatch(r"[0-9a-f]{2}", entry.name):
                continue
            directory = Path(entry.path)
            _directory(directory)
            with os.scandir(directory) as entries:
                for item in entries:
                    if not item.name.endswith(".uc2") or not DIGEST.fullmatch(item.name[:-4]):
                        continue
                    digest = item.name[:-4]
                    if digest[:2] != directory.name:
                        raise ValueError("chunk in wrong directory")
                    with _regular(Path(item.path)) as stream:
                        add(digest, os.fstat(stream.fileno()).st_size)
    return result


def _validated(root):
    """Verify all committed files before a destructive maintenance operation."""
    descriptions = []
    for address in _addresses(root):
        manifest = _manifest(root, address, (1 << 63) - 1)
        _files(root, manifest)
        descriptions.append((_created(manifest["created"]), address))
    return descriptions


@_locked
def prune(repository, *, keep_last=1, keep=(), apply=False):
    """Plan or apply explicit retention; verify every snapshot before deleting.

    v1 removes old manifests durably before their unreachable chunks. An
    interruption can leave harmless orphans; rerun prune to finish. v2 uses a
    single SQLite transaction. Older clients must be stopped during maintenance.
    """
    _integer(keep_last, 0, MAX_ENTRIES)
    if not isinstance(apply, bool):
        raise ValueError("apply must be boolean")
    root = _repository(repository)
    descriptions = _validated(root)
    addresses = {d for _, d in descriptions}
    retained = {_digest(d) for d in keep}
    if not retained <= addresses:
        raise ValueError("retained snapshot does not exist")
    if keep_last:
        retained.update(d for _, d in sorted(descriptions)[-keep_last:])
    if not retained and addresses:
        raise ValueError("retention must keep at least one snapshot")
    live = set()
    for address in retained:
        manifest = _manifest(root, address, (1 << 63) - 1)
        live.update(d for e in manifest["entries"] if e["type"] == "file" for d, _ in e["chunks"])
        if len(live) > MAX_OBJECTS:
            raise ValueError("retention live-chunk limit (2000000) exceeded")
    inventory = _inventory(root)
    obsolete = sorted(addresses - retained)
    orphans = sorted(inventory.keys() - live)
    result = {"applied": apply, "retained_snapshots": sorted(retained),
              "removed_snapshots": obsolete, "unreachable_chunks": len(orphans),
              "unreachable_chunk_bytes": sum(inventory[d] for d in orphans)}
    if apply:
        connection = _db(root)
        if connection is not None:
            connection.executemany("DELETE FROM manifests WHERE digest=?", ((d,) for d in obsolete))
            connection.executemany("DELETE FROM chunks WHERE digest=?", ((d,) for d in orphans))
        else:
            for address in obsolete:
                (root / "snapshots" / (address + ".json")).unlink()
            _sync_directory(root / "snapshots")
            parents = set()
            for digest in orphans:
                path = _chunk_path(root, digest)
                path.unlink()
                parents.add(path.parent)
            for parent in parents:
                _sync_directory(parent)
    return result


def _replace_file(path, data):
    if path.exists() or path.is_symlink():
        info = path.lstat()
        if _reparse(info) or not stat.S_ISREG(info.st_mode):
            raise ValueError("replacement target must be a regular file")
    fd, name = tempfile.mkstemp(prefix=".repair-", dir=path.parent)
    temporary = Path(name)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        _sync_directory(path.parent)
    finally:
        temporary.unlink(missing_ok=True)


def repair(repository, replica, *, addresses=()):
    """Repair selected (or all replica) snapshots from a fully verified copy.

    Destination-only snapshots are retained. A v1 interruption may leave some
    repaired objects; repeated repair is safe. v2 commits all replacements once.
    """
    root, source = _repository(repository), _repository(replica)
    if root.resolve() == source.resolve():
        raise ValueError("replica must be a separate repository")
    with _lock(root), _lock(source):
        selected = list(dict.fromkeys(_digest(d) for d in addresses)) if addresses else _addresses(source)
        # Complete the donor verification before changing destination objects.
        for address in selected:
            _files(source, _manifest(source, address, (1 << 63) - 1))
        connection = _db(root)
        repaired_chunks = repaired_manifests = 0
        for address in selected:
            manifest = _manifest(source, address, (1 << 63) - 1)
            for entry in manifest["entries"]:
                if entry["type"] != "file":
                    continue
                for digest, size in entry["chunks"]:
                    try:
                        _chunk(root, digest, size)
                    except (ValueError, OSError, sqlite3.DataError):
                        data = _archive_data(source, digest)
                        if connection is None:
                            _replace_file(_chunk_path(root, digest, create=True), data)
                        else:
                            connection.execute("INSERT OR REPLACE INTO chunks VALUES (?, ?)", (digest, data))
                        repaired_chunks += 1
                        _chunk(root, digest, size)
            data = _manifest_data(source, address)
            try:
                current = _manifest_data(root, address)
            except (ValueError, OSError, EOFError, sqlite3.DataError):
                current = None
            if current != data:
                if connection is None:
                    _replace_file(root / "snapshots" / (address + ".json"), data)
                else:
                    connection.execute("INSERT OR REPLACE INTO manifests VALUES (?, ?)", (address, gzip.compress(data, mtime=0)))
                repaired_manifests += 1
            _files(root, _manifest(root, address, (1 << 63) - 1))
        return {"snapshots": selected, "repaired_chunks": repaired_chunks,
                "repaired_manifests": repaired_manifests}


@_locked
def compact(repository):
    """Convert v1 loose objects to v2 SQLite, or reclaim v2 free pages.

    All snapshots are verified before publication. Each UC2 blob remains an
    independent archive; manifests use bounded gzip. The config switch is atomic.
    v1 readers reject the new config. No snapshot history is discarded.
    """
    root = _repository(repository)
    descriptions = _validated(root)
    addresses = {d for _, d in descriptions}
    connection = _db(root)
    if connection is None:
        fd, name = tempfile.mkstemp(prefix=".compact-", dir=root)
        os.close(fd)
        temporary = Path(name)
        packed = None
        try:
            packed = _connect(temporary, create=True)
            packed.execute("CREATE TABLE chunks (digest TEXT PRIMARY KEY, data BLOB NOT NULL)")
            packed.execute("CREATE TABLE manifests (digest TEXT PRIMARY KEY, data BLOB NOT NULL)")
            for digest in sorted(_inventory(root)):
                data = _archive_data(root, digest)
                with UC2File(io.BytesIO(data), max_entries=1, max_decoder_memory=8 * 1024 * 1024, max_extract_size=MAX_CHUNK,
                             max_read_size=MAX_CHUNK) as archive:
                    entries = archive.infolist(all_versions=True)
                    if (len(entries) != 1 or entries[0].is_dir() or
                            not 1 <= entries[0].file_size <= MAX_CHUNK or
                            _hash(archive.read(entries[0])) != digest):
                        raise ValueError("invalid orphan chunk: " + digest)
                packed.execute("INSERT INTO chunks VALUES (?, ?)", (digest, data))
            for _, address in descriptions:
                packed.execute("INSERT INTO manifests VALUES (?, ?)", (address, gzip.compress(_manifest_data(root, address), mtime=0)))
            packed.commit()
            packed.close()
            packed = None
            # An interrupted earlier conversion may have left a complete DB.
            destination = root / "objects.sqlite3"
            if destination.exists() or destination.is_symlink():
                with _regular(destination):
                    pass
            with temporary.open("r+b") as stream:
                os.fsync(stream.fileno())
            os.replace(temporary, destination)
            _sync_directory(root)
            _replace_file(root / "config.json", _json(PACKED_FORMAT))
        finally:
            if packed is not None:
                packed.close()
            temporary.unlink(missing_ok=True)
    # Cleanup follows the durable config switch. Only known v1 object names
    # are removed; a failed cleanup can be finished by another compact call.
    removed = 0
    for directory in (root / "chunks").iterdir():
        if not re.fullmatch(r"[0-9a-f]{2}", directory.name):
            continue
        _directory(directory)
        for path in directory.glob("*.uc2"):
            if not DIGEST.fullmatch(path.stem):
                continue
            # The verified v2 catalog is authoritative after config publication;
            # a later prune may already have removed this backup's packed copy.
            with _regular(path):
                pass
            path.unlink()
            removed += 1
        _sync_directory(directory)
    for path in (root / "snapshots").glob("*.json"):
        if not DIGEST.fullmatch(path.stem):
            continue
        with _regular(path):
            pass
        path.unlink()
        removed += 1
    _sync_directory(root / "snapshots")
    _sync_directory(root / "chunks")
    if connection is not None:
        connection.commit()
        connection.execute("VACUUM")
    return {"format_version": 2, "snapshots": len(descriptions), "removed_loose_files": removed,
            "database_bytes": (root / "objects.sqlite3").stat().st_size}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    new = commands.add_parser("create", help="snapshot a directory")
    new.add_argument("source")
    new.add_argument("repository")
    new.add_argument("--label", default="")
    for name in ("list", "verify", "restore"):
        command = commands.add_parser(name)
        command.add_argument("repository")
        if name != "list":
            command.add_argument("snapshot")
        if name == "restore":
            command.add_argument("destination")
        if name != "list":
            command.add_argument("--max-total-size", type=int, default=1 << 40)
    new.add_argument("--max-total-size", type=int, default=1 << 40)
    maintain = commands.add_parser("prune", help="plan retention; --apply deletes unreachable data")
    maintain.add_argument("repository")
    maintain.add_argument("--keep-last", type=int, default=1)
    maintain.add_argument("--keep", action="append", default=[])
    maintain.add_argument("--apply", action="store_true")
    packed = commands.add_parser("compact", help="verify and pack storage; v1 clients must stop")
    packed.add_argument("repository")
    recovery = commands.add_parser("repair", help="repair from a verified replica")
    recovery.add_argument("repository")
    recovery.add_argument("replica")
    recovery.add_argument("--snapshot", dest="addresses", action="append", default=[])
    args = vars(parser.parse_args(argv))
    command = args.pop("command")
    if "snapshot" in args:
        args["address"] = args.pop("snapshot")
    try:
        result = {"create": create, "list": snapshots, "verify": verify,
                  "restore": restore, "prune": prune, "compact": compact, "repair": repair}[command](**args)
    except (OSError, ValueError, TypeError, KeyError, sqlite3.Error, EOFError) as error:
        parser.exit(1, f"uc2-snapshot: {error}\n")
    print(json.dumps(result, ensure_ascii=True, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
