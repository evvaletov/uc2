# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Read-only Linux FUSE mount with a bounded, verified decoded-file cache."""
from __future__ import annotations

import argparse
from collections import OrderedDict
import errno
import os
from pathlib import Path
import stat
import sys
import tempfile
import threading

from . import BadArchiveError, ResourceLimitError, UC2File, _positive


class ArchiveFS:
    """Filesystem implementation, independent of the optional FUSE binding.

    Files appear under /files; /versions/N exposes version N of each member.
    A cache miss decodes the whole requested file to a private temporary file.
    Only successful checksum-verified decodes enter the cache. Each decode uses
    a fresh reader, releasing its master cache before the next operation.
    """

    def __init__(self, archive, *, cache_bytes=512 * 1024**2, cache_entries=128,
                 decoder_bytes=64 * 1024**2, max_entries=100_000,
                 max_nodes=200_000, cache_dir=None):
        for name, value in locals().copy().items():
            if name in ("cache_bytes", "cache_entries", "decoder_bytes", "max_entries", "max_nodes"):
                _positive(name, value)
        self.cache_bytes = cache_bytes
        self.cache_entries = cache_entries
        self.decoder_bytes = decoder_bytes
        self.max_entries = max_entries
        self.max_nodes = max_nodes
        if max_nodes < 3:
            raise ResourceLimitError("mount node limit exceeded")
        self.cached_bytes = 0
        self.cache = OrderedDict()
        self.nodes = {"/": None, "/files": None, "/versions": None}
        self.children = {key: set() for key in self.nodes}
        self.children["/"].update(("files", "versions"))
        self.lock = threading.RLock()
        self.source = open(archive, "rb")
        self.temporary = None
        try:
            if not stat.S_ISREG(os.fstat(self.source.fileno()).st_mode):
                raise ValueError("mount source must be a regular archive file")
            self.signature = self._signature()
            with self._reader() as reader:
                for info in reader.infolist(all_versions=True):
                    parts = info.filename.split("/")
                    if any(p in ("", ".", "..") or "\x00" in p for p in parts):
                        raise BadArchiveError("unsafe mount pathname")
                    if info.version == 0:
                        self._add("/files/" + info.filename, info)
                    self._add(f"/versions/{info.version}/" + info.filename, info)
            self._unchanged()
            self.temporary = tempfile.TemporaryDirectory(prefix="uc2-mount-", dir=cache_dir)
        except BaseException:
            self.close()
            raise

    def _signature(self):
        s = os.fstat(self.source.fileno())
        return s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns

    def _unchanged(self):
        if self._signature() != self.signature:
            raise OSError(errno.ESTALE, "archive changed while mounted")

    def _reader(self):
        return UC2File(self.source, max_entries=self.max_entries,
                       max_decoder_memory=self.decoder_bytes,
                       max_extract_size=self.cache_bytes)

    def _add(self, path, info):
        parts = path.strip("/").split("/")
        parent = "/"
        for i, part in enumerate(parts):
            child = parent.rstrip("/") + "/" + part
            value = info if i == len(parts) - 1 and not info.is_dir() else None
            if child in self.nodes:
                if self.nodes[child] is not None or value is not None:
                    raise BadArchiveError("conflicting mount paths")
            else:
                if len(self.nodes) >= self.max_nodes:
                    raise ResourceLimitError("mount node limit exceeded")
                self.nodes[child] = value
                self.children[parent].add(part)
                if value is None:
                    self.children[child] = set()
            parent = child

    def _node(self, path):
        self._unchanged()
        try:
            return self.nodes[path]
        except KeyError:
            raise OSError(errno.ENOENT, "archive member not found") from None

    def getattr(self, path, fh=None):
        with self.lock:
            info = self._node(path)
            directory = info is None
            timestamp = 0
            if info is not None and info.date_time is not None:
                try:
                    timestamp = info.date_time.timestamp()
                except (ValueError, OverflowError, OSError):
                    pass
            return dict(st_mode=(stat.S_IFDIR | 0o555) if directory else (stat.S_IFREG | 0o444),
                        st_nlink=2 if directory else 1, st_size=0 if directory else info.file_size,
                        st_uid=os.getuid(), st_gid=os.getgid(), st_mtime=timestamp,
                        st_ctime=timestamp, st_atime=timestamp)

    def readdir(self, path, fh=None):
        with self.lock:
            if self._node(path) is not None:
                raise OSError(errno.ENOTDIR, "not a directory")
            return [".", "..", *sorted(self.children[path])]

    def access(self, path, mode):
        self.getattr(path)
        if mode & os.W_OK:
            raise OSError(errno.EROFS, "UC2 mount is read-only")
        if mode & os.X_OK and self.nodes[path] is not None:
            raise OSError(errno.EACCES, "archive files are not executable")
        return 0

    def _decoded(self, info):
        key = info._index
        if key in self.cache:
            self.cache.move_to_end(key)
            return self.cache[key][0]
        if info.file_size > self.cache_bytes:
            raise OSError(errno.EFBIG, "entry exceeds the decoded cache budget")
        while self.cache and (self.cached_bytes + info.file_size > self.cache_bytes or
                              len(self.cache) >= self.cache_entries):
            _, (old, size) = self.cache.popitem(last=False)
            old.close()
            self.cached_bytes -= size
        output = tempfile.TemporaryFile(dir=self.temporary.name)
        try:
            with self._reader() as reader:
                reader.copy_to(reader.getinfo(info.filename, version=info.version), output)
            output.flush()
            self._unchanged()
        except BaseException:
            output.close()
            raise
        self.cache[key] = output, info.file_size
        self.cached_bytes += info.file_size
        return output

    def open(self, path, flags):
        with self.lock:
            if flags & (os.O_ACCMODE | os.O_TRUNC | os.O_APPEND | os.O_CREAT):
                raise OSError(errno.EROFS, "UC2 mount is read-only")
            info = self._node(path)
            if info is None:
                raise OSError(errno.EISDIR, "not a file")
            self._decoded(info)
            return info._index + 1

    def read(self, path, size, offset, fh=None):
        with self.lock:
            if size < 0 or offset < 0:
                raise OSError(errno.EINVAL, "negative read range")
            info = self._node(path)
            if info is None:
                raise OSError(errno.EISDIR, "not a file")
            if offset >= info.file_size or not size:
                return b""
            data = self._decoded(info)
            data.seek(offset)
            return data.read(min(size, 1024**2, info.file_size - offset))

    def statfs(self, path):
        self.getattr(path)
        return dict(f_bsize=4096, f_frsize=4096, f_blocks=0, f_bfree=0,
                    f_bavail=0, f_files=len(self.nodes), f_ffree=0, f_favail=0,
                    f_namemax=255, f_flag=getattr(os, "ST_RDONLY", 1))

    def close(self):
        with self.lock:
            for output, _ in self.cache.values():
                output.close()
            self.cache.clear()
            self.cached_bytes = 0
            if self.temporary is not None:
                self.temporary.cleanup()
                self.temporary = None
            self.source.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("mountpoint", type=Path, help="existing empty directory")
    parser.add_argument("--cache-mib", type=int, default=512)
    parser.add_argument("--cache-entries", type=int, default=128)
    parser.add_argument("--decoder-mib", type=int, default=64)
    parser.add_argument("--max-entries", type=int, default=100_000)
    parser.add_argument("--max-nodes", type=int, default=200_000)
    parser.add_argument("--cache-dir", type=Path)
    args = parser.parse_args()
    if not sys.platform.startswith("linux"):
        parser.error("uc2-mount currently supports Linux")
    try:
        from fuse import FUSE, FuseOSError, Operations
    except (ImportError, OSError) as error:
        parser.error(f"install uc2[mount] and your distribution's libfuse2 runtime: {error}")
    mountpoint = args.mountpoint.resolve()
    if not mountpoint.is_dir() or any(mountpoint.iterdir()):
        parser.error("mountpoint must be an existing empty directory")
    cache_dir = (args.cache_dir or Path(tempfile.gettempdir())).resolve()
    if mountpoint in (cache_dir, *cache_dir.parents):
        parser.error("cache directory must be outside the mountpoint")
    fs = None
    try:
        fs = ArchiveFS(args.archive, cache_bytes=args.cache_mib * 1024**2,
                       cache_entries=args.cache_entries, decoder_bytes=args.decoder_mib * 1024**2,
                       max_entries=args.max_entries, max_nodes=args.max_nodes,
                       cache_dir=cache_dir)

        class ReadOnly(Operations):
            def __call__(self, op, *arguments):
                if op in ("init", "destroy", "release", "flush", "fsync", "releasedir", "fsyncdir"):
                    return 0
                if op in ("create", "write", "truncate", "unlink", "mkdir", "rmdir", "rename",
                          "symlink", "link", "mknod", "chmod", "chown", "utimens", "setxattr", "removexattr"):
                    raise FuseOSError(errno.EROFS)
                try:
                    function = getattr(fs, op, None)
                    return function(*arguments) if function else super().__call__(op, *arguments)
                except ResourceLimitError:
                    raise FuseOSError(errno.ENOMEM) from None
                except BadArchiveError:
                    raise FuseOSError(errno.EIO) from None
                except OSError as error:
                    raise FuseOSError(error.errno or errno.EIO) from None

        # Foreground operation makes lifetime and unmounting explicit. Single
        # threaded FUSE callbacks plus the core lock bound concurrent decoding.
        FUSE(ReadOnly(), str(mountpoint), foreground=True, nothreads=True, ro=True,
             default_permissions=True, nodev=True, nosuid=True, noexec=True,
             fsname="uc2", direct_io=True, attr_timeout=0, entry_timeout=0, negative_timeout=0)
    except (OSError, ValueError, BadArchiveError, ResourceLimitError, RuntimeError) as error:
        parser.exit(1, f"uc2-mount: {error}\n")
    finally:
        if fs is not None:
            fs.close()


if __name__ == "__main__":
    main()
