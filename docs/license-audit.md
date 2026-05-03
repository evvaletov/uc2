# UC2 License Audit

Status: 2026-05-03. Maintained by Eremey Valetov.

UC2 v3 builds on three layers of prior work, each released under its
own free-software licence.  This document records per-file provenance,
the LGPL-3.0 -> GPL-3.0 transition rationale, and the SPDX identifiers
applied across the source tree.

## Layer 1: Nico de Vries (1992-1996), released 2015

Nico de Vries authored UltraCompressor II as proprietary DOS software
between 1992 and 1996.  Danny Bezemer obtained permission to release
the source code publicly and did so in 2015 under the GNU Lesser
General Public License v3 (LGPL-3.0).

The 2015 release is preserved in this repository under
`original/UC2_source/` byte-for-byte unchanged, including its license
header (`GNU LESSER GENERAL PUBLIC LICENSE V3.txt`) and the original
binaries (`uc2pro.exe`, `uc237b.exe`, `ue.exe`).  No file in `lib/` or
`cli/` is a verbatim copy of any file in that release.  The 2015 source
serves as the format specification: it is read for documentation
purposes (the on-disk archive layout, the SuperMaster dictionary
contents, the Huffman tree encoding) but its C code is not linked in.

Relicensing impact: none.  Layer 1 is preserved under its original
LGPL-3.0 licence; nothing is moved upward to GPL-3.0.

## Layer 2: Jan Bobrowski (2020-2021), libunuc2 / unuc2

Jan Bobrowski wrote a clean-room portable decompressor for UC2 v3
archives between 2020 and 2021.  He released two products:

- `libunuc2` (decompression library) under LGPL-3.0
- `unuc2` (CLI tool) under GPL-3.0-or-later

The upstream source is preserved in `original/unuc2-0.6/`.  The
following files in this repository derive from Bobrowski's work and
retain his original licence:

| Current file                          | Upstream origin                          | Licence       |
|---------------------------------------|------------------------------------------|---------------|
| `lib/src/decompress.c`                | `original/unuc2-0.6/libunuc2.c`          | LGPL-3.0-only |
| `lib/src/list.h`                      | `original/unuc2-0.6/list.h` (identical)  | LGPL-3.0-only |
| `lib/src/super.bin`                   | `original/unuc2-0.6/super.bin` (identical) | data (de Vries) |
| `lib/include/uc2/libuc2.h`            | `original/unuc2-0.6/libunuc2.h`          | LGPL-3.0-only |
| `cli/src/main.c`                      | `original/unuc2-0.6/unuc2.c`             | GPL-3.0-or-later |
| `cli/src/compat/compat_win32.c`       | `original/unuc2-0.6/compat/compat.c` (Win32 portions) | LGPL-3.0-only |
| `cli/src/compat/compat_dos.c`         | derived from `compat/compat.c` (DOS adaptation by Valetov) | LGPL-3.0-only |

Modifications by Valetov in 2026 are released under the same licence
as the file's upstream origin (LGPL-3.0 stays LGPL-3.0; GPL-3.0 stays
GPL-3.0).  No unilateral upgrade from LGPL to GPL has been applied to
Bobrowski's work.

`lib/src/super.bin` is the SuperMaster dictionary block from the 1992
distribution.  It is data, not code: a fixed binary table used as a
compression-context priming dictionary.  It is bit-identical to the
file in Bobrowski's release and to the data extracted from de Vries's
1992 binaries.

## Layer 3: Eremey Valetov (2026), UC2 v3 revival

The following files are new work by Valetov, originally authored for
the UC2 v3 revival project, released under GPL-3.0-or-later:

| File                                  | Function                                           |
|---------------------------------------|----------------------------------------------------|
| `lib/src/compress.c`                  | LZ77+Huffman compressor (inverse of decompress.c)  |
| `lib/src/uc2_tables.c`                | Huffman delta-coding lookup tables                 |
| `lib/src/uc2_internal.h`              | Shared compressor/decompressor types and constants |
| `lib/src/uc2_cdc.c` + `.h`            | Content-defined chunking (Gear hash)               |
| `lib/src/uc2_merkle.c` + `.h`         | Merkle DAG of deduplicated blocks                  |
| `lib/src/uc2_blockstore.c` + `.h`     | Cross-archive content-addressable block store     |
| `lib/src/uc2_simhash.c` + `.h`        | SimHash near-duplicate detection                   |
| `lib/src/uc2_delta.c` + `.h`          | Binary delta compression                           |
| `lib/src/uc2_rans.c` + `.h`           | rANS entropy coder (method 10)                     |
| `lib/src/uc2_dict.c` + `.h`           | Dictionary metadata for cross-archive sharing      |
| `lib/src/uc2_preprocess.c` + `.h`     | BCJ / BWT / delta-filter preprocessing             |
| `lib/src/uc2_lz4.c` + `.h`            | LZ4 ultra-fast compression                         |
| `lib/src/uc2_blake3.c` + `.h`         | BLAKE3 cryptographic hashing                       |
| `lib/src/uc2_sha256.c` + `.h`         | SHA-256 (FIPS 180-4)                               |
| `lib/src/uc2_ots.c` + `.h`            | OpenTimestamps proof parser, walker, trailer      |
| `cli/src/compat/getopt.c`             | Minimal POSIX getopt for MSVC                      |
| `cli/src/main.c` (post-`9525a81` additions) | OTS attach/extract/info, archive creation, scanning, benchmark | GPL-3.0-or-later (matches upstream `unuc2.c`) |

The Phase 3-7 modules are independent implementations.  They reference
the UC2 v3 archive format (which is a bitstream layout, not a
copyrighted work) and use BLAKE3, SHA-256, LZ4, rANS, etc. from
public-domain or self-authored reference implementations.  None of
these modules link to or derive from Bobrowski's code beyond using
shared header types declared in `uc2_internal.h`.

## Relicensing rationale

The composite project links Bobrowski's LGPL-3.0 library (`lib/`) into
a GPL-3.0-or-later executable (`cli/`).  This combination is permitted
by LGPL-3.0 sec. 4 (Combined Works): the LGPL library may be used in
GPL-licensed work without requiring the library itself to be relicensed.

No code has been moved from LGPL-3.0 to GPL-3.0 in this project.
LGPL §3 permits a one-way upgrade from LGPL to GPL but exercising it
is unnecessary here: the LGPL files remain LGPL, the GPL files remain
GPL, and the combined work is distributable under GPL-3.0-or-later (as
recorded in the project root `LICENSE` file).

If a downstream user wishes to redistribute `lib/` standalone under
LGPL-3.0, the LGPL-3.0 files are individually identifiable via their
SPDX-License-Identifier headers.

## SPDX policy

All source files in `lib/` and `cli/` carry one of two SPDX
identifiers near the top:

- `SPDX-License-Identifier: LGPL-3.0-only` for files derived from
  Bobrowski's libunuc2 / compat code.
- `SPDX-License-Identifier: GPL-3.0-or-later` for `cli/src/main.c`
  (matches Bobrowski's original GPL-3.0-or-later choice for the CLI
  tool) and for all Valetov-authored Phase 2 through Phase 7 work.

Original copyright lines authored by Bobrowski are preserved verbatim
where present.  Where Valetov has added substantial new content to a
Bobrowski-originated file (notably `cli/src/main.c` and
`compat_dos.c`), an additional Valetov copyright line has been added
without removing the original.

The 2015 LGPL-3.0 release in `original/UC2_source/` and the 2020-2021
release in `original/unuc2-0.6/` are preserved unchanged and are not
subject to this policy: they retain whatever licence headers their
authors shipped them with.

## Audit checklist

- [x] LGPL-3.0 release by Bezemer/de Vries preserved unchanged in
      `original/UC2_source/`
- [x] LGPL-3.0 / GPL-3.0 release by Bobrowski preserved unchanged in
      `original/unuc2-0.6/`
- [x] Per-file provenance table above
- [x] SPDX-License-Identifier on every source file in `lib/` and `cli/`
- [x] CREDITS.md attributes Bobrowski specifically for libunuc2-derived
      files, not as generic "inspiration"
- [x] LICENSE-HISTORY summary published as this file
      (`docs/license-audit.md`)
- [x] No silent LGPL-to-GPL upgrade: every Bobrowski-origin file
      retains LGPL-3.0-only

## References

- LGPL-3.0 text: <https://www.gnu.org/licenses/lgpl-3.0.html>
- GPL-3.0 text: see `LICENSE` in repository root
- Bobrowski upstream: <http://torinak.com/~jb/unuc2/>
- Bezemer 2015 release notes: `original/UC2_source/Read Me First.txt`
- LGPL-3.0 sec. 3 (allowing one-way upgrade to GPL):
  <https://www.gnu.org/licenses/lgpl-3.0.html#section3>
- LGPL-3.0 sec. 4 (Combined Works): same document, sec. 4
