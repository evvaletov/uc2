# Credits

## Original UltraCompressor II

**Nico de Vries** created UltraCompressor II (1992--1996), a DOS archiver
with LZ77+Huffman compression, master-block deduplication, and file
versioning.  The original source code is preserved in `original/UC2_source/`.

- Website: <https://nicodevries.com/professional/>

## 2015 LGPL Source Release

In December 2015, **Vladislav Sagunov** asked Nico de Vries whether
the UC2 source could be re-released under a free licence.  De Vries
agreed and personally re-published the full UC2 source under the GNU
LGPL-3.0 (with a small Borland C/C++ runtime carve-out for DOS-specific
code).  The release notes are preserved verbatim in
`original/UC2_source/Read Me First.txt`.

## Portable Decompressor (unuc2 / libunuc2)

**Jan Bobrowski** wrote a clean-room portable decompressor (2020--2021) that
forms the foundation of this project's decompression engine.  The library
(`libunuc2`) is licensed under LGPL-3.0; the CLI tool (`unuc2`) is licensed
under GPL-3.0-or-later.

The following files in this repository derive directly from Bobrowski's
work and retain his licence (see `docs/license-audit.md` for the full
provenance table):

- `lib/src/decompress.c` (LGPL-3.0-only)  --  derived from `libunuc2.c`
- `lib/src/list.h` (LGPL-3.0-only)  --  byte-identical to upstream
- `lib/include/uc2/libuc2.h` (LGPL-3.0-only)  --  derived from `libunuc2.h`
- `cli/src/main.c` (GPL-3.0-or-later)  --  derived from `unuc2.c`,
  with substantial additions for archive creation, OTS, and benchmarking
- `cli/src/compat/compat_win32.c` (LGPL-3.0-only)
- `cli/src/compat/compat_dos.c` (LGPL-3.0-only, DOS adaptation by Valetov)

The SuperMaster dictionary (`lib/src/super.bin`) is bit-identical to the
copy shipped in `original/unuc2-0.6/` and to the data extracted from
de Vries's 1992 binaries.

- Website: <http://torinak.com/~jb/unuc2/>
- Original source preserved in `original/unuc2-0.6/`

## Additional Contributors

- **Danny Bezemer**  --  co-authored UC2 with de Vries during the
  original 1992-1996 development.
- **Jan-Pieter Cornet**  --  early testing, archive samples, and format
  documentation contributions to the unuc2 project.

## UC2 v3.0.0 Revival

**Eremey Valetov**  --  project revival, including:

- CMake build system and cross-platform porting (Linux, macOS, MSVC, DJGPP)
- LZ77+Huffman compression engine (compatible with original UC2 Pro)
- rANS entropy coder (method 10, levels 6--9)
- Content-defined chunking (CDC) with Gear rolling hash
- Merkle DAG content addressing
- Cross-archive block store for shared deduplication
- SimHash near-duplicate detection
- Delta compression for binary patching
- Content-aware preprocessing (BCJ, BWT, delta filter)
- LZ4 ultra-fast compression
- BLAKE3 cryptographic hashing
- SHA-256 (FIPS 180-4) implementation
- OpenTimestamps integration (proof parser, walker, archive trailer)
- Dictionary metadata for cross-archive sharing
- Backward compatibility with original UC2 Pro (verified via DOSBox-X)
- Automated test infrastructure (19 unit tests, DOSBox-X cross-tool testing)

All files under "UC2 v3.0.0 Revival" are licensed GPL-3.0-or-later by
Eremey Valetov (2026).  See `docs/license-audit.md` for the per-file
provenance table and the LGPL-3.0 / GPL-3.0 chain rationale.

- GitHub: <https://github.com/evvaletov/uc2>
