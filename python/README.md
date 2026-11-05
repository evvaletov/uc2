# UC2 for Python

Read UltraCompressor II archives using the native UC2 decoder. The package also
includes the UC2 command-line program for listing, extraction, archive creation,
and stream ingestion. Wheels contain the native code; no separate UC2 install
or compiler is needed when a wheel is available.

This is an alpha release. It supports the implemented legacy Huffman methods,
master dictionaries, directories, stored versions, and UC2 v3 rANS entries.
Turbo/method 80 and multi-volume archives remain unsupported. Python archive
creation is not yet an API; the bundled CLI provides that operation.

## Install and read

```sh
python -m pip install --pre uc2
uc2 -l archive.uc2
python -m uc2 -t archive.uc2
```

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
`is_dir()`. Metadata objects can be passed directly to `read`, `extract`, and
`copy_to`. They belong to the archive that produced them.

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
be a symlink. Files and directories receive ordinary new-file permissions;
archived permissions and timestamps are exposed as metadata but not applied.

Errors include `BadArchiveError`, `ResourceLimitError`, `UnsafePathError`, and
ordinary Python I/O exceptions. Verification uses the archive's legacy
checksums, which are not cryptographic authentication. The known unfinished
BLAKE3 module in the larger project is not exposed by this API or used by the
archive reader.

## Build from source

Requires Python 3.10+, CMake, and a C99 compiler. The build backend obtains
CMake when necessary. Building a source distribution requires a compiler;
install a matching wheel to avoid that requirement.

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
