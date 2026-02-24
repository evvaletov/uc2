# UC2 v3.0.0 — UltraCompressor II

A cross-platform revival of UltraCompressor II, the DOS-era archiver by
Nico de Vries (1992--1996).  UC2 was notable for its advanced deduplication
("master blocks"), file versioning, and competitive compression ratios on
the hardware of its day.

This project brings UC2 back as a modern, portable C99 tool.  Phase 1
(current) provides decompression and archive listing, built on Jan
Bobrowski's clean-room portable decompressor
([unuc2/libunuc2](http://torinak.com/~jb/unuc2/)).  Phase 2 will add
compression using the original algorithms.

## Building

Requires CMake >= 3.16 and a C99 compiler (GCC, Clang, or MSVC).

```sh
cmake -B build
cmake --build build
```

The binary is at `build/cli/uc2`.

## Usage

```
uc2 archive.uc2                       # Extract all files
uc2 -l archive.uc2                    # List contents
uc2 -t archive.uc2                    # Test archive integrity
uc2 -d /tmp/out archive.uc2           # Extract to directory
uc2 -l archive.uc2 '*.txt'            # List matching files
uc2 -p archive.uc2 readme.txt         # Extract to stdout
```

### Options

| Flag | Description |
|------|-------------|
| `-l` | List archive contents |
| `-t` | Test archive integrity |
| `-a` | Include all file versions (not just latest) |
| `-d path` | Extract to specified directory |
| `-f` | Overwrite existing files |
| `-p` | Extract to stdout |
| `-D` | Skip directory metadata; `-DD` also skips file metadata |
| `-T` | Tab-separated output (for scripting) |

## Project Structure

```
UC2/
  lib/            libuc2 decompression library
  cli/            uc2 command-line tool
  original/       preserved original sources (reference only)
  cmake/          build system modules
  tests/          test archives and test programs
```

## History

- **v1.0--v2.3** (1992--1996) Original DOS releases by Nico de Vries
- **2015** Source code released under LGPL-3.0 by Danny Bezemer
- **2020--2021** Jan Bobrowski writes unuc2/libunuc2 (portable decompressor)
- **2026** UC2 v3.0.0: cross-platform revival

## License

GPL-3.0.  See [LICENSE](LICENSE) and [CREDITS.md](CREDITS.md) for full
attribution.
