# SAS and UC2SEA input

Alpha10 opens original SAS split archives and DOS UC2SEA 2.3 archives through
the native CLI and Python path API:

```sh
uc2 -l collection.P01
uc2 -t collection.P03
uc2 -x collection.EXE
```

```python
from uc2 import UC2File
with UC2File("collection.003") as archive:
    archive.testzip()
    data = archive.read("README.TXT")
```

Keep every part together under its original base name. SAS uses `.p01`, `.p02`,
… (or uppercase `.P01`); indices 100–999 use three digits. UC2SEA uses a first
`.EXE`/`.exe` and `.001`, `.002`, … continuations. Any part can be selected.
Noncanonical SAS suffixes such as `.p1` or `.p001` are rejected. File-like Python
objects cannot resolve sibling paths; use a filesystem path for split input.
The browser reader, WCX and optional libarchive reader do not yet use this path
resolver. They continue to accept ordinary UC2 containers.

SAS headers supply total size, physical part length and ordinal. Missing,
truncated, reordered and inconsistent parts fail explicitly. Reassembly uses
an anonymous disk spool, with at most 999 parts and UINT32_MAX payload bytes.
Original SAS has no set ID or per-part checksum. Same-size mixed parts therefore
need native UC2 checksum verification; those historical checksums are corruption
checks, not authentication. Extra sibling files after the declared complete
set are ignored.

The UC2SEA reader recognizes the DOS MZ envelope, optional text banner, config
and standalone Huffman stream. It verifies per-file checksums and converts
individual records into a temporary ordinary UC2 archive. Executable code and
banners are never run or displayed; `SEASTART` files remain ordinary entries.
Names decode from CP850. Unsafe paths and unsupported file records are rejected.
The Windows 95 extraction module has not been validated.

UC2SEA limits are 999 continuation parts, 512 MiB compressed stream, 512 MiB
decoded records and 100,000 total normalized entries. It can use three temporary
disk spools and recompresses the verified records, so opening a large SEA can
be slow. Individual native allocations are capped below 8 MiB; metadata and
compression work have the writer's additional bounds. Sibling files must be
regular files, with symlinks/reparse points refused; parent directories and
files must remain stable during the operation. The API is data-only and does
not execute the input even when its name ends in `.EXE`.

Original-encoder tests cover a six-volume set and a single fast-compressed
archive with banner, nested path, empty file and startup batch file. Committed
fixtures retain their compressed records but replace the original restricted
DOS extraction program with a synthetic non-runnable MZ envelope. Provenance
and SHA-256 values are in `tests/archives/sea/manifest.json`.
