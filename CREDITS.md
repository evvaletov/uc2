# Credits

## Original UltraCompressor II

**Nico de Vries** created UltraCompressor II (1992--1996), a DOS archiver
with LZ77+Huffman compression, master-block deduplication, and file
versioning.  The original source code is preserved in `original/UC2_source/`.

- Website: <https://nicodevries.com/professional/>

## Source Code Release

**Danny Bezemer** facilitated the public release of the UC2 source code in
2015 under the LGPL-3.0 license.

## Portable Decompressor (unuc2 / libunuc2)

**Jan Bobrowski** wrote a clean-room portable decompressor (2020--2021) that
forms the foundation of this project's decompression engine.  The library
(`libunuc2`) is licensed under LGPL-3.0; the CLI tool (`unuc2`) is licensed
under GPL-3.0.

- Website: <http://torinak.com/~jb/unuc2/>
- Original source preserved in `original/unuc2-0.6/`

## Additional Contributors

- **Jan-Pieter Cornet**  --  early testing, archive samples, and format
  documentation contributions to the unuc2 project.
- **Vladislav Sagunov**  --  maintained UC2 resources and documentation.

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
- Dictionary metadata for cross-archive sharing
- Backward compatibility with original UC2 Pro (verified via DOSBox-X)
- Automated test infrastructure (16 unit tests, DOSBox-X cross-tool testing)

- GitHub: <https://github.com/evvaletov/uc2>
