# UC2: UltraCompressor II

A cross-platform revival of UltraCompressor II, the DOS-era archiver by
Nico de Vries (1992-1996). UC2 used shared compression dictionaries
("master blocks") and file versioning.

UC2 v3 is under development in portable C99. The current alpha is
[3.0.0a8](https://pypi.org/project/uc2/3.0.0a8/).
Compression levels 2-5 use the original Huffman format; levels 6-9 require the
new rANS reader. Original-format compatibility is
covered by DOSBox-X fixtures, including Turbo/method 80 reads. Multi-volume
sets still require reassembly before use.

## Features

The command-line tool creates, lists, tests, and extracts archives. It supports
nested directories, stored file versions, shared master blocks, output to
stdout, tab-separated listings, and a quiet mode for scripts.

- LZ77 with Huffman coding at levels 2-5, or rANS at levels 6-9.
- Content-defined chunking and identical-chunk deduplication through `--ingest`.
  The resulting archive is self-contained; `--ingest-restore` reconstructs the
  input stream. A tar input remains a byte stream of `part-N` entries.
- Compression comparisons through `-B`, including the library's LZ4 method.
- OpenTimestamps proof attachment, extraction, and inspection.
- Readers for [libarchive/bsdtar](contrib/libarchive/README.md),
  [Midnight Commander](contrib/mc/README.md), and
  [Total Commander](contrib/totalcommander/README.md).
- A [Python API](python/README.md) with entry metadata, stored versions,
  streamed decoding, verification, extraction limits, and `UC2Writer` creation.
- A read-only [Linux FUSE mount](docs/mount.md) with separate version views
  and a bounded cache of verified decoded files.
- Builds for Linux, macOS, Windows (MSVC), and DOS (DJGPP).
- A [browser reader](https://evvaletov.github.io/uc2/reader/) for local archive
  inspection, stored versions, checksum verification, and file downloads.
- A [preservation toolkit](docs/preservation.md) for batch inventory and
  verified ZIP/tar exports, retaining original paths and metadata in manifests.

The library also contains a 64-bit Merkle DAG, a shared block store, SimHash,
binary delta coding, dictionary metadata, and BCJ/BWT/delta preprocessing.
These modules are not all connected to the CLI archive writer. The standalone BLAKE3
module passes the official unkeyed test vectors. It is not used by the current
archive read/write path; archive checksums do not authenticate content.

See the [updated collection benchmarks](docs/benchmarks/2026-11-05-compression.md) for measured
size, time, and memory comparisons with 7z and tar+zstd.

## Install

```sh
python -m pip install --pre uc2
uc2 -l archive.uc2
uc2-preserve old-archives -o migrated --format zip
```

The Python package includes the native CLI. Wheels support CPython 3.10-3.14
on Linux x86-64, Windows x86-64, and Intel/Apple Silicon macOS.
[Standalone CLI downloads](https://github.com/evvaletov/uc2/releases)
do not require Python. These are alpha releases.

```python
from uc2 import UC2File, UC2Writer

with UC2Writer("backup.uc2") as archive:
    archive.write("project", arcname="source")

with UC2File("archive.uc2") as archive:
    print(archive.namelist())
    archive.verify()
    archive.extractall("restored")
```

See the [Python documentation](python/README.md) for limits, stored versions,
error handling, and extraction behavior.

## Building

Requires CMake >= 3.16 and a C99 compiler (GCC, Clang, or MSVC).

```sh
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure -E roundtrip_dosbox
```

The CMake CLI integration tests require Python 3.11 or newer. The Python
package itself supports Python 3.10 or newer. The separate
`roundtrip_dosbox` test requires the DOSBox-X environment described in
`tests/scripts/roundtrip_dosbox.sh`.

## Usage

```sh
uc2 -w archive.uc2 files...            # Create archive
uc2 archive.uc2                        # Extract all files
uc2 -l archive.uc2                     # List contents
uc2 -t archive.uc2                     # Test integrity
uc2 -d /tmp/out archive.uc2            # Extract to directory
uc2 -w -L 8 archive.uc2 files...      # Create with rANS Tight
uc2 -B files...                        # Benchmark all methods
```

### Compression Levels

| Level | Method | Description |
|-------|--------|-------------|
| 2 | Huffman | Fast |
| 3 | Huffman | Normal |
| 4 | Huffman | Tight (default) |
| 5 | Huffman | Ultra |
| 6 | rANS | Fast |
| 7 | rANS | Normal |
| 8 | rANS | Tight |
| 9 | rANS | Ultra |

Levels 2-5 produce archives readable by the original 1992 UC2 Pro.
Levels 6-9 use rANS entropy coding (UC2 v3 only).

### Options

| Flag | Description |
|------|-------------|
| `-w` | Create archive |
| `-l` | List archive contents |
| `-t` | Test archive integrity |
| `-L n` | Compression level (2-9) |
| `-B` | Benchmark all methods on input files |
| `-d path` | Extract to specified directory |
| `-f` | Overwrite existing files |
| `-p` | Extract to stdout |
| `-q` | Quiet (suppress status messages) |
| `-a` | Include all file versions |
| `-D` | Skip directory metadata; `-DD` also skips file metadata |
| `-T` | Tab-separated output |

### Stream ingestion

```sh
tar cf - project/ | uc2 --ingest backup.uc2
uc2 --ingest-restore backup.uc2 > restored.tar
```

Ingestion keeps a small payload buffer and spools unique raw chunks to a
temporary file. Memory for chunk indexes and directory entries grows with the
number of chunks. The writer replaces the output only after completing the
archive. A killed process can leave a sibling `.tmp-*` file; the previous
output remains intact. The format and this writer use 32-bit size limits.

## Project Structure

```
UC2/
  lib/              libuc2 compression/decompression library
    include/uc2/    public headers (libuc2, uc2_cdc, uc2_merkle, uc2_rans, ...)
    src/            library implementation
  cli/              uc2 command-line tool
  python/           Python reader/writer API and native extension
  tests/            unit tests and test corpus
  original/         preserved original UC2 Pro sources (reference only)
  contrib/          libarchive and file-manager integrations
  docs/             documentation and benchmark results
```

## Credits

- **Nico de Vries**: Original UltraCompressor II (1992-1996)
- **Danny Bezemer**: Co-developed the original UC2
- **Vladislav Sagunov**: Requested the free-software source release (2015)
- **Jan Bobrowski**: Clean-room portable decompressor (unuc2/libunuc2, 2020-2021)
- **Eremey Valetov**: UC2 v3 revival, compression engine, deduplication, and ongoing development

See [CREDITS.md](CREDITS.md) for full attribution.

## History

- **v1.0 to v2.3** (1992-1996) Original DOS releases by Nico de Vries
- **2015** Nico de Vries releases the source under LGPL-3.0 after a request
  from Vladislav Sagunov
- **2020-2021** Jan Bobrowski writes unuc2/libunuc2 (portable decompressor)
- **2026** UC2 v3 development adds a compression engine, rANS, and stream
  deduplication

## License

The project combines GPL-3.0-or-later code and LGPL-3.0-only components.
The libarchive adapter and independently written integration glue are available
under `GPL-3.0-or-later OR BSD-2-Clause`. The Total Commander adapter retains
Bobrowski's LGPL license. See [LICENSE](LICENSE), [CREDITS.md](CREDITS.md), and
the [per-file license audit](docs/license-audit.md).

Incremental directory versions are available through `uc2 snapshot` in the
Python package, with shared SHA-256-addressed UC2 chunks and verified restores.
See [the snapshot guide](https://github.com/evvaletov/uc2/blob/main/docs/snapshots.md).
