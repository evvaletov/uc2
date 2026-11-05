# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Read and create UltraCompressor II archives with the native library."""
from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime
from importlib.metadata import version as _package_version
import os
from pathlib import Path
from typing import BinaryIO

from . import _native
from ._writer import UC2Writer
from ._extract import UnsafePathError, extract_entry, validate_members

__version__ = _package_version("uc2")
BadArchiveError = _native.BadArchiveError
ResourceLimitError = _native.ResourceLimitError
__all__ = ["UC2Writer", "UC2File", "UC2Info", "BadArchiveError", "ResourceLimitError", "UnsafePathError"]


@dataclass(frozen=True, slots=True)
class UC2Info:
    """Entry metadata; version zero is the newest stored version of a path."""
    filename: str
    file_size: int
    compress_size: int
    date_time: datetime | None
    attributes: int
    method: int
    version: int
    _directory: bool = field(repr=False)
    _index: int = field(repr=False)

    def is_dir(self) -> bool:
        return self._directory


def _positive(name: str, value: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not 0 < value <= 2**63 - 1:
        raise ValueError(f"{name} must be a positive integer below 2**63")
    return value


def _date(value: int) -> datetime | None:
    try:
        return datetime(1980 + (value >> 25), (value >> 21) & 15,
                        (value >> 16) & 31, (value >> 11) & 31,
                        (value >> 5) & 63, (value & 31) * 2)
    except ValueError:
        return None


class UC2File:
    """Read a path or seekable binary file. Caller-owned files stay open.

    The source must remain unchanged while this object is open. Concurrent or
    reentrant decoding on one object is rejected; use a separate object per
    thread. DOS timestamps have no timezone. Extraction creates ordinary files
    and directories without applying archived permissions or timestamps.
    """

    def __init__(self, file, mode: str = "r", *, max_entries: int = 100_000,
                 max_decoder_memory: int = 64 * 1024**2,
                 max_read_size: int = 64 * 1024**2,
                 max_extract_size: int = 1024**3,
                 max_total_size: int = 1024**3):
        if mode != "r":
            raise ValueError("UC2File supports read mode only; use UC2Writer to create archives")
        self._reader = None
        self._source = None
        self._owned = False
        self.max_read_size = _positive("max_read_size", max_read_size)
        self.max_extract_size = _positive("max_extract_size", max_extract_size)
        self.max_total_size = _positive("max_total_size", max_total_size)
        _positive("max_entries", max_entries)
        _positive("max_decoder_memory", max_decoder_memory)
        if isinstance(file, (str, bytes, os.PathLike)):
            self._source = open(file, "rb")
            self._owned = True
        else:
            self._source = file
        try:
            source = self._source
            if not hasattr(source, "seek") or not hasattr(source, "read"):
                raise TypeError("file must be a path or seekable binary file")
            source.seek(0, os.SEEK_END)
            size = source.tell()
            if size > 0xFFFFFFFF:
                raise ResourceLimitError("archive exceeds the 32-bit UC2 format limit")
            source.seek(0)
            self._reader = _native.Reader(source, max_entries, max_decoder_memory)
            self._build_index(self._reader.entries())
        except BaseException:
            self.close()
            raise

    def _build_index(self, rows):
        directories = {}
        for parent, identifier, directory, name, *_ in rows:
            if not name or "/" in name or "\\" in name:
                raise BadArchiveError("invalid archive name component")
            if directory:
                if identifier == 0 or identifier in directories:
                    raise BadArchiveError("duplicate or invalid directory identifier")
                directories[identifier] = (parent, name)

        def parent_names(identifier):
            parts = []
            seen = set()
            while identifier:
                if identifier in seen or len(seen) >= 64:
                    raise BadArchiveError("cyclic or excessively deep archive directory")
                seen.add(identifier)
                try:
                    identifier, name = directories[identifier]
                except KeyError:
                    raise BadArchiveError("missing archive parent directory") from None
                parts.append(name)
            return list(reversed(parts))

        paths = ["/".join([*parent_names(row[0]), row[3]]) for row in rows]
        kinds = {}
        for row, path in zip(rows, paths):
            if path in kinds and (row[2] or kinds[path]):
                raise BadArchiveError("conflicting archive directory paths")
            kinds[path] = bool(row[2])
        versions = {}
        infos = [None] * len(rows)
        for index in reversed(range(len(rows))):
            parent, identifier, directory, name, size, packed, date, attr, method = rows[index]
            filename = paths[index]
            version = versions.get(filename, 0)
            versions[filename] = version + 1
            infos[index] = UC2Info(filename, size, packed, _date(date), attr, method,
                                   version, bool(directory), index)
        self._infos = tuple(infos)
        self._lookup = {(info.filename, info.version): info for info in infos}

    @property
    def closed(self) -> bool:
        return self._reader is None

    def _check_open(self):
        if self.closed:
            raise ValueError("archive is closed")

    def close(self) -> None:
        if self._reader is not None:
            self._reader.close()
            self._reader = None
        if self._owned and self._source is not None:
            self._source.close()
        self._source = None

    def __enter__(self):
        self._check_open()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def __del__(self):
        if hasattr(self, "_reader"):
            self.close()

    def infolist(self, *, all_versions: bool = False) -> list[UC2Info]:
        self._check_open()
        return [x for x in self._infos if all_versions or x.version == 0]

    def namelist(self, *, all_versions: bool = False) -> list[str]:
        return [x.filename for x in self.infolist(all_versions=all_versions)]

    def getinfo(self, name: str, *, version: int = 0) -> UC2Info:
        self._check_open()
        if isinstance(version, bool) or not isinstance(version, int) or version < 0:
            raise ValueError("version must be a nonnegative integer")
        return self._lookup[(name, version)]

    def _member(self, member: str | UC2Info, version: int = 0) -> UC2Info:
        self._check_open()
        if isinstance(member, UC2Info):
            if version != 0:
                raise ValueError("select a version by name or pass UC2Info without version")
            if not 0 <= member._index < len(self._infos) or self._infos[member._index] is not member:
                raise ValueError("UC2Info belongs to another archive")
            return member
        if not isinstance(member, str):
            raise TypeError("member must be a filename or UC2Info")
        return self.getinfo(member, version=version)

    def read(self, member: str | UC2Info, *, version: int = 0) -> bytes:
        info = self._member(member, version)
        return self._reader.read(info._index, self.max_read_size)

    def copy_to(self, member: str | UC2Info, destination: BinaryIO, *, version: int = 0) -> int:
        """Decode into a binary stream; a failure may leave partial stream output."""
        info = self._member(member, version)
        return self._reader.copy_to(info._index, destination.write, self.max_extract_size)

    def verify(self, *, all_versions: bool = True) -> None:
        """Decode and check UC2 checksums; raise on corruption or a resource limit."""
        infos = [x for x in self.infolist(all_versions=all_versions) if not x.is_dir()]
        self._check_sizes(infos)
        for info in infos:
            self._reader.copy_to(info._index, None, self.max_extract_size)

    def _check_sizes(self, infos):
        if any(x.file_size > self.max_extract_size for x in infos):
            raise ResourceLimitError("entry exceeds max_extract_size")
        if sum(x.file_size for x in infos if not x.is_dir()) > self.max_total_size:
            raise ResourceLimitError("operation exceeds max_total_size")

    def extract(self, member: str | UC2Info, path=".", *, version: int = 0,
                overwrite: bool = False) -> Path:
        info = self._member(member, version)
        self._check_sizes([info])
        validate_members([info])
        return extract_entry(self, info, Path(path), overwrite)

    def extractall(self, path=".", members=None, *, overwrite: bool = False) -> None:
        """Extract newest versions by default; reject unsafe or colliding paths.

        Files are committed individually after checksum verification. This is
        not a transaction covering the whole directory: earlier verified files
        remain if a later extraction fails.
        """
        self._check_open()
        infos = self.infolist() if members is None else [self._member(x) for x in members]
        self._check_sizes(infos)
        validate_members(infos)
        for info in infos:
            extract_entry(self, info, Path(path), overwrite)
