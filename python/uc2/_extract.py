# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Extraction path checks and per-file atomic installation."""
from contextlib import contextmanager
import os
from pathlib import Path
import secrets
import stat
import tempfile


class UnsafePathError(ValueError):
    """A member path could escape the destination or has ambiguous semantics."""


def _parts(name):
    parts = name.split("/")
    for part in parts:
        if (part in ("", ".", "..") or part.endswith((".", " ")) or
                any(ord(c) < 32 or ord(c) == 127 or c in '\\:<>"|?*' for c in part)):
            raise UnsafePathError(f"unsafe archive pathname: {name!r}")
        base = part.split(".", 1)[0].rstrip(" ").upper()
        if (base in ("CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$") or
                (len(base) == 4 and base[:3] in ("COM", "LPT") and base[-1] in "123456789¹²³")):
            raise UnsafePathError(f"reserved archive pathname: {name!r}")
    return parts


def validate_members(infos):
    paths = {}
    for info in infos:
        key = tuple(p.casefold() for p in _parts(info.filename))
        if key in paths:
            raise UnsafePathError(f"colliding extraction paths: {info.filename!r}")
        paths[key] = info.is_dir()
    for key in paths:
        if any(paths.get(key[:i]) is False for i in range(1, len(key))):
            raise UnsafePathError("file and directory extraction paths conflict")


def _ordinary(path, *, directory=False):
    try:
        st = path.lstat()
    except FileNotFoundError:
        return False
    if stat.S_ISLNK(st.st_mode) or getattr(st, "st_file_attributes", 0) & 0x400:
        raise UnsafePathError(f"refusing to follow a link or reparse point: {path}")
    if directory and not stat.S_ISDIR(st.st_mode):
        raise NotADirectoryError(path)
    if not directory and not stat.S_ISREG(st.st_mode):
        raise UnsafePathError(f"destination is not a regular file: {path}")
    return True


@contextmanager
def _parent_fd(root, parents):
    fd = os.open(root, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        for part in parents:
            try:
                os.mkdir(part, mode=0o755, dir_fd=fd)
            except FileExistsError:
                pass
            try:
                child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            except OSError as error:
                raise UnsafePathError(f"unsafe extraction directory: {part!r}") from error
            os.close(fd)
            fd = child
        yield fd
    finally:
        os.close(fd)


def _posix_extract(archive, info, root, parts, overwrite):
    with _parent_fd(root, parts[:-1]) as parent:
        leaf = parts[-1]
        if info.is_dir():
            try:
                os.mkdir(leaf, mode=0o755, dir_fd=parent)
            except FileExistsError:
                pass
            try:
                check = os.open(leaf, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=parent)
            except OSError as error:
                raise UnsafePathError(f"unsafe extraction directory: {info.filename!r}") from error
            os.close(check)
            return
        try:
            st = os.stat(leaf, dir_fd=parent, follow_symlinks=False)
        except FileNotFoundError:
            st = None
        if st is not None:
            if not stat.S_ISREG(st.st_mode):
                raise UnsafePathError(f"destination is not a regular file: {info.filename!r}")
            if not overwrite:
                raise FileExistsError(root.joinpath(*parts))
        temporary = ".uc2-" + secrets.token_hex(16) + ".tmp"
        fd = os.open(temporary, os.O_CREAT | os.O_EXCL | os.O_WRONLY | os.O_NOFOLLOW,
                     0o600, dir_fd=parent)
        try:
            with os.fdopen(fd, "wb") as out:
                archive.copy_to(info, out)
            if overwrite:
                os.replace(temporary, leaf, src_dir_fd=parent, dst_dir_fd=parent)
            else:
                # Hard-link creation fails atomically if the destination exists.
                os.link(temporary, leaf, src_dir_fd=parent, dst_dir_fd=parent, follow_symlinks=False)
        finally:
            try:
                os.unlink(temporary, dir_fd=parent)
            except FileNotFoundError:
                pass


def _windows_extract(archive, info, root, parts, overwrite):
    parent = root
    for part in parts[:-1]:
        parent /= part
        if not _ordinary(parent, directory=True):
            parent.mkdir()
        _ordinary(parent, directory=True)
    target = parent / parts[-1]
    if info.is_dir():
        if not _ordinary(target, directory=True):
            target.mkdir()
        return
    if _ordinary(target) and not overwrite:
        raise FileExistsError(target)
    fd, temporary = tempfile.mkstemp(prefix=".uc2-", suffix=".tmp", dir=parent)
    try:
        with os.fdopen(fd, "wb") as out:
            archive.copy_to(info, out)
        _ordinary(target)
        if overwrite:
            os.replace(temporary, target)
        else:
            os.rename(temporary, target)  # Windows rename refuses existing targets.
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def extract_entry(archive, info, destination: Path, overwrite):
    parts = _parts(info.filename)
    destination.mkdir(parents=True, exist_ok=True)
    root = destination.resolve()
    if os.name == "posix" and hasattr(os, "O_NOFOLLOW"):
        _posix_extract(archive, info, root, parts, overwrite)
    elif os.name == "nt":
        _windows_extract(archive, info, root, parts, overwrite)
    else:
        raise NotImplementedError("safe extraction is supported on POSIX and Windows")
    return root.joinpath(*parts)
