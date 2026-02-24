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

- **Jan-Pieter Cornet** -- early testing, archive samples, and format
  documentation contributions to the unuc2 project.
- **Vladislav Sagunov** -- maintained UC2 resources and documentation.

## UC2 v3.0.0 Revival

**Eremey Valetov** -- project revival, CMake build system, cross-platform
porting, and ongoing development.

- GitHub: <https://github.com/evvaletov/uc2>
