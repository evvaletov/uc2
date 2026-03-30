# UC2 v3.0.0 — UltraCompressor II

A cross-platform revival of UltraCompressor II, the DOS-era archiver by
Nico de Vries (1992–1996).  UC2 was notable for its advanced deduplication
("master blocks"), file versioning, and competitive compression ratios on
the hardware of its day.

UC2 v3 brings it back as a modern, portable C99 tool with full
backward compatibility — archives created by UC2 v3 can be extracted
by the original 1992 UC2 Pro, and vice versa.

## Features

- **Full compression and decompression** — LZ77+Huffman (levels 2–5),
  rANS entropy coding (levels 6–9), LZ4 ultra-fast mode
- **Backward compatible** with the original UC2 Pro (verified via
  automated DOSBox-X cross-tool round-trip testing)
- **Content-defined chunking** (CDC) with Gear rolling hash for
  position-independent deduplication
- **Merkle DAG** content addressing with 64-bit hashes
- **Cross-archive dedup** via shared block store
- **Near-duplicate detection** via SimHash
- **Delta compression** for binary patching
- **Content-aware preprocessing** — BCJ (x86), BWT (text), delta filter
- **BLAKE3** cryptographic hashing for archive integrity
- **Benchmark mode** — test all methods on input data
- **Personality** — warm, confident status messages (`-q` for scripting)
- Directory archival with nested hierarchies
- Cross-platform: Linux, macOS, Windows (MSVC), DOS (DJGPP)

## Building

Requires CMake ≥ 3.16 and a C99 compiler (GCC, Clang, or MSVC).

```sh
cmake -B build
cmake --build build
ctest --test-dir build    # 16 unit tests
```

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

Levels 2–5 produce archives readable by the original 1992 UC2 Pro.
Levels 6–9 use rANS entropy coding (UC2 v3 only, better compression).

### Options

| Flag | Description |
|------|-------------|
| `-w` | Create archive |
| `-l` | List archive contents |
| `-t` | Test archive integrity |
| `-L n` | Compression level (2–9) |
| `-B` | Benchmark all methods on input files |
| `-d path` | Extract to specified directory |
| `-f` | Overwrite existing files |
| `-p` | Extract to stdout |
| `-q` | Quiet (suppress status messages) |
| `-a` | Include all file versions |
| `-D` | Skip directory metadata; `-DD` also skips file metadata |
| `-T` | Tab-separated output |

## Project Structure

```
UC2/
  lib/              libuc2 compression/decompression library
    include/uc2/    public headers (libuc2, uc2_cdc, uc2_merkle, uc2_rans, ...)
    src/            library implementation
  cli/              uc2 command-line tool
  tests/            unit tests and test corpus
  original/         preserved original UC2 Pro sources (reference only)
  docs/             Sphinx documentation
```

## Credits

- **Nico de Vries** — Original UltraCompressor II (1992–1996)
- **Danny Bezemer** — Facilitated source code release (2015)
- **Jan Bobrowski** — Clean-room portable decompressor (unuc2/libunuc2, 2020–2021)
- **Eremey Valetov** — UC2 v3 revival, compression engine, deduplication, and ongoing development

See [CREDITS.md](CREDITS.md) for full attribution.

## History

- **v1.0–v2.3** (1992–1996) Original DOS releases by Nico de Vries
- **2015** Source code released under LGPL-3.0 by Danny Bezemer
- **2020–2021** Jan Bobrowski writes unuc2/libunuc2 (portable decompressor)
- **2026** UC2 v3.0.0: cross-platform revival with full compression engine,
  backward compatibility, and modern deduplication

## License

GPL-3.0.  See [LICENSE](LICENSE) and [CREDITS.md](CREDITS.md).
