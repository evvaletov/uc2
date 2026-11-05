# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Create UC2 archives through the native writer."""
from __future__ import annotations

from datetime import datetime
from functools import wraps
import io
import os
from pathlib import Path
import stat
import tempfile

from . import _native


def _integer(name, value, maximum):
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= maximum:
        raise ValueError(f"{name} must be an integer from 0 through {maximum}")
    return value


def _timestamp(value):
    if value is None:
        return 0
    if not isinstance(value, datetime) or value.tzinfo is not None:
        raise ValueError("date_time must be a naive datetime or None")
    if not 1980 <= value.year <= 2107:
        raise ValueError("DOS timestamps cover years 1980 through 2107")
    return ((value.year - 1980) << 25 | value.month << 21 | value.day << 16 |
            value.hour << 11 | value.minute << 5 | value.second // 2)


def _filesystem_time(value):
    try:
        result = datetime.fromtimestamp(value)
        return result if 1980 <= result.year <= 2107 else None
    except (OverflowError, OSError, ValueError):
        return None


def _operation(method):
    @wraps(method)
    def call(self, *args, **kwargs):
        self._check_open()
        if self._busy:
            raise RuntimeError("writer is already in use")
        self._busy = True
        try:
            return method(self, *args, **kwargs)
        except BaseException:
            self._writer.abort()
            self._writer = None
            raise
        finally:
            self._busy = False
    return call


class UC2Writer:
    """Create an archive at a path, replacing it atomically on successful close.

    Input streams are consumed immediately into a native temporary spool; they
    need no seek/tell and remain open. Directory trees are added recursively.
    Sources must remain unchanged during write(). Symlinks and special files
    are rejected. A failed operation aborts the writer, preserving an existing
    destination. Explicit abort(), an exception in the context, and destruction
    discard work. close() commits; destruction never commits implicitly.
    """

    def __init__(self, file, *, level: int = 4, max_entries: int = 100_000,
                 max_total_size: int = 0xFFFFFFFF):
        self._writer = None
        self._busy = False
        self._temporary = None
        self.stats = None
        if not isinstance(file, (str, os.PathLike)):
            raise TypeError("destination must be a filesystem path")
        self._destination = Path(file).absolute()
        _integer("level", level, 9)
        if level < 2:
            raise ValueError("level must be 2 through 9")
        _integer("max_entries", max_entries, 1_000_000)
        _integer("max_total_size", max_total_size, 0xFFFFFFFF)
        if not max_entries or not max_total_size:
            raise ValueError("writer limits must be positive")
        self._directories = {(): 0}
        self._files = set()
        self._writer = _native.Writer(level, max_entries, max_total_size)

    @property
    def closed(self) -> bool:
        return self._writer is None

    def _check_open(self):
        if self.closed:
            raise ValueError("writer is closed")

    @staticmethod
    def _parts(name):
        if not isinstance(name, str):
            raise TypeError("archive name must be a string using '/' separators")
        if not name or name.startswith("/") or "\\" in name or ":" in name or "\0" in name:
            raise ValueError("archive name must be a relative path")
        parts = tuple(name.split("/"))
        if len(parts) > 65 or any(p in ("", ".", "..") for p in parts):
            raise ValueError("invalid or excessively deep archive path")
        for part in parts:
            if any(ord(c) < 32 or ord(c) == 127 for c in part):
                raise ValueError("archive names cannot contain control characters")
            if len(part.encode("cp850")) > 255 or len(part.encode("utf-8")) > 299:
                raise ValueError("name exceeds 255 CP850 or 299 UTF-8 bytes")
        return parts

    def _mkdir(self, parts, time=0, attr=0):
        if len(parts) > 64:
            raise ValueError("directory nesting exceeds 64")
        for depth in range(1, len(parts) + 1):
            path = parts[:depth]
            if path in self._files:
                raise ValueError("file conflicts with directory path")
            if path not in self._directories:
                self._directories[path] = self._writer.add_directory(
                    self._directories[path[:-1]], path[-1],
                    time if depth == len(parts) else 0, attr if depth == len(parts) else 0)
        return self._directories[parts]

    @_operation
    def mkdir(self, arcname: str, *, date_time: datetime | None = None,
              attributes: int = 0) -> None:
        """Add an empty directory and missing parents; existing directories stay unchanged."""
        _integer("attributes", attributes, 0x27)
        self._mkdir(self._parts(arcname), _timestamp(date_time), attributes)

    def _stream(self, arcname, source, size, date_time, attributes):
        parts = self._parts(arcname)
        if parts in self._directories:
            raise ValueError("file conflicts with directory path")
        if size is not None:
            _integer("size", size, 0xFFFFFFFF)
        _integer("attributes", attributes, 0x27)
        if attributes & ~0x27:
            raise ValueError("unsupported DOS attributes")
        time = _timestamp(date_time)
        parent = self._mkdir(parts[:-1])
        self._writer.add_file(parent, parts[-1], time, attributes, size, source)
        self._files.add(parts)

    @_operation
    def write_stream(self, arcname: str, source, *, size: int | None = None,
                     date_time: datetime | None = None, attributes: int = 0x20) -> None:
        """Consume read(n) through EOF. If supplied, size must match exactly."""
        self._stream(arcname, source, size, date_time, attributes)

    @_operation
    def writestr(self, arcname: str, data, *, date_time: datetime | None = None,
                 attributes: int = 0x20) -> None:
        """Add bytes. Encode text explicitly before passing it here."""
        if not isinstance(data, (bytes, bytearray, memoryview)):
            raise TypeError("data must be bytes, bytearray, or memoryview")
        with io.BytesIO(data) as source:
            self._stream(arcname, source, source.getbuffer().nbytes, date_time, attributes)

    def _path(self, source, arcname):
        info = source.lstat()
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
            raise ValueError(f"symlinks and reparse points are unsupported: {source}")
        date = _filesystem_time(info.st_mtime)
        if stat.S_ISDIR(info.st_mode):
            self._mkdir(self._parts(arcname), _timestamp(date))
            for child in sorted(source.iterdir()):
                self._path(child, arcname + "/" + child.name)
        elif stat.S_ISREG(info.st_mode):
            flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0)
            fd = os.open(source, flags)
            try:
                actual = os.fstat(fd)
                if not stat.S_ISREG(actual.st_mode):
                    raise ValueError("input is no longer a regular file")
                file = os.fdopen(fd, "rb")
            except BaseException:
                os.close(fd)
                raise
            with file:
                self._stream(arcname, file, actual.st_size,
                             _filesystem_time(actual.st_mtime), 0x20)
        else:
            raise ValueError(f"only regular files and directories are supported: {source}")

    @_operation
    def write(self, filename, arcname: str | None = None) -> None:
        """Add a file or recursive directory tree, using its basename by default."""
        source = Path(filename)
        if arcname is None:
            arcname = source.absolute().name
        self._parts(arcname)
        self._path(source, arcname)

    def close(self) -> None:
        if self._busy:
            raise RuntimeError("writer is already in use")
        if self.closed:
            return
        self._busy = True
        try:
            fd, name = tempfile.mkstemp(prefix=f".{self._destination.name}.", suffix=".tmp",
                                        dir=self._destination.parent)
            self._temporary = Path(name)
            try:
                output = os.fdopen(fd, "wb")
            except BaseException:
                os.close(fd)
                raise
            with output:
                stats = self._writer.finish(output)
                output.flush()
                os.fsync(output.fileno())
            os.replace(self._temporary, self._destination)
            self._temporary = None
            self.stats = stats
        finally:
            self._busy = False
            self.abort()

    def abort(self) -> None:
        if self._busy:
            raise RuntimeError("writer is already in use")
        if self._writer is not None:
            self._writer.abort()
            self._writer = None
        if self._temporary is not None:
            self._temporary.unlink(missing_ok=True)
            self._temporary = None

    def __enter__(self):
        self._check_open()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if exc_type is None:
            self.close()
        else:
            self.abort()

    def __del__(self):
        if hasattr(self, "_writer"):
            self.abort()
