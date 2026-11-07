# UC2 for Python

Read and create UltraCompressor II archives using the native UC2 library. The package also
includes the UC2 command-line program for listing, extraction, archive creation,
and stream ingestion. Wheels contain the native code; no separate UC2 install
or compiler is needed when a wheel is available.

This is an alpha release. It supports the implemented legacy Huffman methods,
master dictionaries, directories, stored versions, and UC2 v3 rANS entries.
Turbo/method 80 is supported for reading. Multi-volume sets require reassembly. Use `UC2File` for reading and `UC2Writer` for creation.

## Install and read

```sh
python -m pip install --pre uc2
uc2 -l archive.uc2
python -m uc2 -t archive.uc2
```

Wheels support CPython 3.10-3.14 on Linux x86-64 (glibc), Windows x86-64,
and Intel/Apple Silicon macOS.

```python
from uc2 import UC2File

with UC2File("archive.uc2") as archive:
    for info in archive.infolist():
        print(info.filename, info.file_size, info.version)
    contents = archive.read("notes.txt")
    archive.verify()
    archive.extractall("restored")
```

The constructor accepts a path or a seekable binary file, including `BytesIO`.
Caller-owned files stay open; their seek position changes. Keep the source
unchanged while the archive is open. Each thread should use its own `UC2File`.

`infolist()` and `namelist()` return the newest version of each path by default.
Use `infolist(all_versions=True)` to include older entries. A literal semicolon
in a filename remains part of the name:

```python
with UC2File("archive.uc2") as archive:
    newest = archive.read("notes.txt")
    older = archive.read("notes.txt", version=1)
    literal_name = archive.read("notes.txt;1")
```

`UC2Info` records the name, uncompressed and compressed sizes, DOS timestamp
(a naive `datetime`, or `None` if invalid), attributes, method, version, and
`is_dir()`. The `dos_time` field retains the original 32-bit timestamp, including
invalid date encodings. Metadata objects can be passed directly to `read`, `extract`, and
`copy_to`. They belong to the archive that produced them.

## Preserve a collection

```sh
uc2-preserve old-archives -o inventory
uc2-preserve old-archives -o migrated --format zip
uc2-preserve old-archives -o migrated-tar --format tar
```

Each command requires a new output directory. Inputs can be individual archives
or directories. Every stored version is decoded and checked, and manifests
record original paths, raw timestamps, attributes and SHA-256 hashes. Exports
use separate numbered paths for each entry, so old versions and case variants
cannot overwrite each other. Generated ZIP/tar files are reopened and their
contents verified. Failed entries are recorded and omitted from exports; a
partial batch returns exit status 1. Sources are read-only.

The Python API is `uc2.preserve.preserve(inputs, output, format="zip")`.
See the [preservation guide](https://github.com/evvaletov/uc2/blob/main/docs/preservation.md)
for limits, manifest fields and metadata conversion details. The
[browser reader](https://evvaletov.github.io/uc2/reader/) handles individual
archives without a Python installation.

## Limits and extraction

Defaults are 100,000 entries, 64 MiB of decoder allocations, 64 MiB per `read`,
1 GiB per streamed file, and 1 GiB total per `extractall` or `verify` call.
Constructor keywords are `max_entries`, `max_decoder_memory`, `max_read_size`,
`max_extract_size`, and `max_total_size`. They must be positive integers.
The decoder limit excludes Python metadata and caller-owned buffers. Archive
offsets and this reader are limited to 32 bits. These limits are not a CPU-time
sandbox for hostile input.

`copy_to(member, binary_file)` decodes without collecting the file in memory.
Its output may be partial if decoding or writing fails. `extract` and
`extractall` instead stage each file and install it after its UC2 checksum
passes. Existing files are preserved unless `overwrite=True` is supplied.
Earlier successfully extracted files remain if a later member fails.

Extraction rejects absolute paths, traversal, reserved Windows names, control
characters, ambiguous case-folded names, and links below the destination. It
uses file-descriptor-relative operations without following links on POSIX.
Windows checks links and reparse points; use a destination that other processes
cannot modify during extraction. The explicitly chosen destination itself may
be a symlink. POSIX files use mode 0600 and directories use 0755, both masked
by the process umask. Archived permissions and timestamps are exposed as
metadata but not applied.

Errors include `BadArchiveError`, `ResourceLimitError`, `UnsafePathError`, and
ordinary Python I/O exceptions. Verification uses the archive's legacy
checksums, which are not cryptographic authentication. A missing entry or
version raises `KeyError`.

## Create archives

```python
from io import BytesIO
from uc2 import UC2Writer

with UC2Writer("backup.uc2", level=4) as archive:
    archive.write("project", arcname="source")  # recursive directory tree
    archive.mkdir("empty-directory")
    archive.writestr("notes.txt", "Saved from Python\n".encode("utf-8"))
    archive.write_stream("data.bin", BytesIO(b"streamed bytes"))
```

The destination is a filesystem path. A successful `close()` replaces it using
an exclusively created sibling temporary file and `os.replace`. An exception,
`abort()`, or a failed add/close leaves an existing destination intact. Any
failed add aborts the writer; start a new one to retry. Destruction discards
uncommitted work. The containing directory must exist and remain under the
caller's control. Atomic replacement is not a guarantee of survival after a
power failure. Replacement creates a new file with temporary-file permissions
(normally 0600 on POSIX), without preserving destination permissions.

`write(path, arcname=None)` adds a regular file or recursively adds a directory.
The default archive name is the input's basename. Symlinks, reparse points,
and special files are rejected. Keep the source tree unchanged while reading.
`mkdir(name)` creates missing parents; repeated directories retain their first
metadata. `writestr(name, data)` accepts bytes, bytearray, or memoryview; encode
text explicitly. `write_stream(name, source, size=None)` consumes `read(n)`
through EOF without seeking and leaves the input open. A supplied size must
match exactly; detecting excess input can consume one extra byte.

Inputs spool to anonymous temporary files as they are added. Compression runs
at close, using CDC-selected custom master dictionaries. Payload buffers have
fixed sizes; metadata grows with entries and chunks. Temporary disk space
covers the uncompressed inputs and completed archive, plus the destination's
sibling temporary file during the final copy. Duplicate file paths store
versions: the last added version is newest. Directories cannot conflict with
files. Independent writer objects may run separately; one object rejects
concurrent or reentrant use.

Writer limits and metadata:

- `level=4` by default; 2-5 use legacy Huffman and 6-9 use v3 rANS.
- `max_entries=100_000`, configurable from 1 through 1,000,000, includes automatic
  parent directories. `max_total_size=0xFFFFFFFF` bounds all input bytes combined
  and may be reduced. Each file and the completed archive also fit 32 bits.
- The central directory is limited to 64 MiB and the CDC index to 1,048,576 chunks.
  Directories may nest 64 levels deep. These are format/API limits, not CPU limits.
- Archive paths use `/`. Components must be representable in CP850, fit 255 CP850
  bytes and 299 UTF-8 bytes, and contain no traversal, separators, colon, or
  control characters. Unsupported Unicode raises an error; names are never
  shortened. Some legal archive names still require renaming to extract on
  Windows; the reader's extraction checks apply unchanged.
- `mkdir`, `writestr`, and `write_stream` accept `date_time` (a naive `datetime`
  in 1980-2107, or `None`) and DOS `attributes` (R/H/S/A bits). Timestamps have
  two-second resolution. `write` takes filesystem modification times; values
  outside that range become unspecified. It stores ordinary file/directory
  attributes, not Unix permissions.

After successful close, `writer.stats` reports file, directory and master counts
and archive size. The C interface is documented in
[`uc2_writer.h`](https://github.com/evvaletov/uc2/blob/main/lib/include/uc2/uc2_writer.h)
and the [library guide](https://github.com/evvaletov/uc2/blob/main/docs/library.rst).

## Read-only Linux mount

```sh
python -m pip install --pre 'uc2[mount]'
mkdir archived
uc2-mount archive.uc2 archived --cache-mib 512
```

The mount runs in the foreground. Use another terminal to browse `archived/files`
and `archived/versions/1`, then unmount with `fusermount -u archived`.
Linux libfuse2 and `/dev/fuse` are required. Files decode on first access into
a checksum-verified disk cache; an entry larger than its budget fails with
`EFBIG`. The default cache holds up to 512 MiB and 128 files. Decoder
allocations are limited to 64 MiB and released after every decode. Python
metadata is outside that budget and has separate entry and node limits.
See the [mount documentation](https://github.com/evvaletov/uc2/blob/main/docs/mount.md)
for options, stored versions, and error behavior.

## Build from source

Requires Python 3.10+, CMake, and a C99 compiler. The build backend obtains
CMake when necessary.

```sh
python -m pip install .
python -m build
```

Source, releases, and the project roadmap are at
[github.com/evvaletov/uc2](https://github.com/evvaletov/uc2).

The distribution contains GPL-3.0-or-later and LGPL-3.0-only code. Independently
written Python integration code also has a BSD-2-Clause option. This does not
change the licenses of the bundled core or third-party code. License texts,
credits, and the per-file audit are included in the distribution.

Incremental directory versions are available through `uc2 snapshot` in the
Python package, with shared SHA-256-addressed UC2 chunks and verified restores.
See [the snapshot guide](https://github.com/evvaletov/uc2/blob/main/docs/snapshots.md).
