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
import errno
import hashlib
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
MAX_CHUNK = 32768
MAX_MANIFEST = 64 * 1024 * 1024
MAX_ENTRIES = 100_000
MAX_REFS = 500_000
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


def _directory(path):
    if not stat.S_ISDIR(path.lstat().st_mode):
        raise ValueError(f"expected a real directory: {path}")


@contextmanager
def _regular(path):
    before = path.lstat()
    if not stat.S_ISREG(before.st_mode):
        raise ValueError(f"expected a regular file: {path}")
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
    flags |= getattr(os, "O_NONBLOCK", 0)
    fd = os.open(path, flags)
    with os.fdopen(fd, "rb") as source:
        opened = os.fstat(source.fileno())
        if not stat.S_ISREG(opened.st_mode) or (before.st_dev, before.st_ino) != (opened.st_dev, opened.st_ino):
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
    if json.loads(_read(config, 4096)) != FORMAT:
        raise ValueError("unsupported snapshot repository format")
    for name in ("chunks", "snapshots"):
        _directory(root / name)
    return root


@contextmanager
def _lock(root):
    path = root / ".writer-lock"
    # O_EXCL also rejects symlinks. A killed writer leaves a visible lock;
    # remove it only after confirming that no writer is running.
    with path.open("x", encoding="ascii") as lock:
        lock.write(str(os.getpid()) + "\n")
    try:
        yield
    finally:
        path.unlink()


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
    path = _chunk_path(root, digest)
    with _regular(path) as source:
        if os.fstat(source.fileno()).st_size > 1024 * 1024:
            raise ValueError("chunk archive exceeds 1 MiB")
        with UC2File(source, max_entries=1, max_decoder_memory=8 * 1024 * 1024, max_extract_size=MAX_CHUNK) as archive:
            entries = archive.infolist(all_versions=True)
            if len(entries) != 1 or entries[0].is_dir() or entries[0].file_size != size:
                raise ValueError(f"invalid chunk archive: {digest}")
            data = archive.read(entries[0])
    if _hash(data) != digest:
        raise ValueError(f"chunk SHA-256 mismatch: {digest}")
    return data


def _store(root, data):
    digest = _hash(data)
    path = _chunk_path(root, digest, create=True)
    if path.exists() or path.is_symlink():
        if _chunk(root, digest, len(data)) != data:
            raise ValueError("chunk collision")
        return digest, 0
    fd, name = tempfile.mkstemp(prefix=".pending-", dir=path.parent)
    os.close(fd)
    temporary = Path(name)
    try:
        with UC2Writer(temporary, level=4, max_entries=1) as archive:
            archive.writestr("chunk", data)
        # Verify the writer result before the chunk becomes reusable.
        with UC2File(temporary, max_entries=1, max_extract_size=MAX_CHUNK) as archive:
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
            if not stat.S_ISDIR(initial.st_mode):
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
        published = _save(root / "snapshots" / (address + ".json"), data)
        return {"snapshot": address, "files": sum(e["type"] == "file" for e in entries),
                "total_size": total, "new_chunks": new_chunks, "new_chunk_bytes": added,
                "manifest_bytes": len(data), "new_manifest_bytes": len(data) if published else 0,
                "growth_bytes": added + (len(data) if published else 0)}


def _manifest(root, address, max_total_size):
    _digest(address)
    data = _read(root / "snapshots" / (address + ".json"), MAX_MANIFEST)
    if _hash(data) != address:
        raise ValueError("manifest SHA-256 mismatch")
    m = json.loads(data)
    if not isinstance(m, dict) or m.get("format") != FORMAT:
        raise ValueError("unsupported snapshot manifest")
    if not isinstance(m.get("created"), str) or not isinstance(m.get("label"), str):
        raise ValueError("invalid snapshot description")
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
    for e in manifest["entries"]:
        if e["type"] == "directory":
            if destination is not None:
                (destination / e["path"]).mkdir()
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
        finally:
            if output is not None:
                output.close()
    return checked


def verify(repository, address, *, max_total_size=1 << 40):
    """Verify one manifest, all referenced chunks, and complete file hashes."""
    _integer(max_total_size, 0, (1 << 63) - 1)
    root = _repository(repository)
    manifest = _manifest(root, address, max_total_size)
    return {"snapshot": address, "verified_files": _files(root, manifest),
            "total_size": manifest["total_size"]}


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
            os.utime(path, ns=(e["mtime_ns"], e["mtime_ns"]))
            os.chmod(path, e["mode"])
        os.utime(stage, ns=(manifest["root"]["mtime_ns"],) * 2)
        os.chmod(stage, manifest["root"]["mode"])
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
        if reserved and stage.exists():
            destination.rmdir()
        if stage.exists():
            # A metadata failure may have made directories readonly.
            os.chmod(stage, 0o700)
            for directory, dirs, files in os.walk(stage):
                for name in dirs:
                    os.chmod(Path(directory) / name, 0o700)
                for name in files:
                    os.chmod(Path(directory) / name, 0o600)
            shutil.rmtree(stage)


def snapshots(repository):
    """List committed snapshot IDs with validated manifests."""
    root = _repository(repository)
    result = []
    for path in sorted((root / "snapshots").glob("*.json")):
        m = _manifest(root, path.stem, (1 << 63) - 1)
        result.append({"snapshot": path.stem, "created": m["created"],
                       "label": m["label"], "total_size": m["total_size"]})
    return result


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
    args = vars(parser.parse_args(argv))
    command = args.pop("command")
    if "snapshot" in args:
        args["address"] = args.pop("snapshot")
    try:
        result = {"create": create, "list": snapshots, "verify": verify,
                  "restore": restore}[command](**args)
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.exit(1, f"uc2-snapshot: {error}\n")
    print(json.dumps(result, ensure_ascii=True, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
