# Reviving UltraCompressor II: a 1992 DOS archiver, ported forward

Subtitle candidates:
- *Show HN: UC2 v3 - 1992 DOS archiver, ported to modern C99* (HN)
- *UltraCompressor II revival: rANS, CDC, BLAKE3, OpenTimestamps* (Lobsters)

---

In 1992, Nico de Vries shipped UltraCompressor II for DOS.  It
competed with PKZIP and ARJ.  Among the things it did that were
unusual for the era: master-block deduplication.  If an archive
contained several similar files, UC2 could store one shared "master"
block and represent each file as a delta against it - within a
single archive, on a single floppy, in 4 MB of RAM.

UC2 v3.0.0-alpha.1 is a port forward.  Its compressor produces
bitstreams that the original `uc2pro.exe` (UC2 Pro v2.3, 1992)
accepts and extracts at byte-identical fidelity.  It also adds
content-defined chunking, an rANS entropy coder, BLAKE3 + SHA-256
hashing, and an OpenTimestamps integration so an archive can be
anchored to a Bitcoin block at creation time.

This post is the story of how it got here.

## The lineage

UC2 has passed through several pairs of hands across thirty-four
years:

1. **Nico de Vries (1992-1996)** wrote UC2, with assistance from
   Danny Bezemer, Jan-Pieter Cornet, and others credited in the
   original `U_MANUAL.TXT`.

2. **2015 LGPL release**.  In December 2015, Vladislav Sagunov asked
   de Vries whether the source could be re-released under a free
   licence.  De Vries agreed and published the full source under the
   GNU LGPL-3.0 (with a small Borland C/C++ runtime carve-out for
   DOS-specific code).  That release lives in this repo unchanged
   under `original/UC2_source/`, including the `Read Me First.txt`
   from de Vries himself.

3. **Jan Bobrowski (2020-2021)** wrote a clean-room portable
   *decompressor* in C, called `unuc2` / `libunuc2`.  The library is
   LGPL-3.0; the CLI tool is GPL-3.0-or-later.  Bobrowski's
   decompressor compiles cleanly on POSIX, MSVC, and (with care) DOS,
   and his code is what most modern UC2 work builds on.

4. **Eremey Valetov (2026)** is who I am.  What I've added is the
   *compressor* that pairs with Bobrowski's decompressor, plus
   several modules of compression / dedup / integrity work.

Bobrowski-derived files in the repo retain LGPL-3.0; new work is
GPL-3.0-or-later.  Per-file provenance is in
[`docs/license-audit.md`](../license-audit.md); the 1992 source and
the 2020 release are preserved unmodified.

## What's in v3

### Byte-bitstream-compatible LZ77 + Huffman

The compressor in `lib/src/compress.c` is the inverse of Bobrowski's
decompressor and produces UC2 v3 archives whose method-4 bitstream is
accepted by `uc2pro.exe`.  Cross-tool round-trip is in CI: a
`tests/scripts/roundtrip_dosbox.sh` job runs the original 1992 binary
in DOSBox-X against archives built by the modern tool (and vice
versa) and verifies that extracted files are bit-identical to the
inputs.

Compression levels 2-5 (Fast, Normal, Tight, Ultra) match the
original's IDs.  The original's `bFlag` heuristic for choosing
between default and custom Huffman trees on small blocks is
preserved.

### Master-block deduplication, modernised

The 1992 UC2 grouped files by an exact prefix match and built one
master block per group.  v3 layers content-defined chunking (CDC) on
top: file pairs that share large blocks of content at *non-aligned*
positions can also be grouped, since the chunker breaks both files
on the same content-defined boundaries.  CDC uses a Gear rolling
hash with an average chunk size of 4 KiB.

Several additional modules ship as libraries with their own unit
tests, used so far by the compressor's master-block selection logic
and exposed for callers:
- a Merkle DAG of deduplicated blocks (`uc2_merkle.h`),
- a content-addressable cross-archive block store (`uc2_blockstore.h`),
- SimHash near-duplicate detection (`uc2_simhash.h`),
- byte-level delta compression (`uc2_delta.h`).

These extend the format with new metadata records.  Method-4 (the
1992 bitstream) remains untouched, so old readers handle the file
data; new readers see the additional dedup hints.

### Modern compression backends

Phase 4 added pluggable backends behind new method IDs.  Method 4
(the original Huffman) is kept as-is for round-trip with the 1992
binary.

- **Method 10**: rANS entropy coder.  32-bit table-based.  Selected
  by levels 6-9.
- **LZ4**: ultra-fast mode, exposed via the `uc2_lz4.h` library and
  the `uc2 -B` benchmark; not yet a first-class archive backend.
- **Content-aware preprocessing** (`uc2_preprocess.h`): BCJ for x86
  address normalisation, BWT for text, byte-stride delta filter.
- **Dictionary metadata** (`uc2_dict.h`): zstd-inspired formal
  dictionary records with content-hash IDs and integrity checksums.
- **`uc2 -B`**: built-in benchmark mode runs all methods on the
  input and prints ratio + timing per method.

### Cryptographic integrity

Phase 7 anchored the archive's content hash:

- **BLAKE3** (`uc2_blake3.h`) for fast content hashing.
- **SHA-256** (`uc2_sha256.h`, FIPS 180-4) for OpenTimestamps
  compatibility.
- **OpenTimestamps integration** (`uc2_ots.h`): the archive's SHA-256
  can be anchored to a Bitcoin block via a public calendar server,
  and the resulting proof is stored in a magic-bracketed sidecar
  trailer appended after the recorded archive bytes.  The 1992 reader
  ignores the trailer (it uses the front header's recorded length),
  preserving backward compatibility.  Extracted proofs are
  byte-identical to standard `.ots` files; the cross-check test runs
  them through `python-opentimestamps` to confirm round-trip parsing.

The OTS parser is conservative about hostile input: strict-canonical
varints, depth-bounded recursion, varbytes size cap, leaf digest must
match the recomputed archive SHA-256 before `--ots-attach` accepts a
proof.

## A demonstration

```sh
# Create an archive
$ uc2 -w -L4 demo.uc2 file1.txt file2.txt
UC2 compression level: Tight
Created demo.uc2 (2 files, 0 dirs, 1 master, 215 bytes)
Everything went OK

# Extract with the original UC2 Pro v2.3 in DOSBox-X
$ dosbox -conf <(echo -e "[autoexec]\nmount C: .\nC:\nuc2pro.exe -x demo.uc2")
# -> bit-identical files

# Anchor the archive to the Bitcoin blockchain
$ ots stamp demo.uc2          # produces demo.uc2.ots from a calendar
$ uc2 --ots-attach demo.uc2.ots demo.uc2
Attached 396-byte OTS proof to demo.uc2

$ uc2 -t demo.uc2
Testing archive integrity...
OTS proof: leaf matches; structure verified
Everything went OK
```

Cross-tool round-trip is in CI.  The OTS extracted output is
verified against the upstream `python-opentimestamps` parser when
that package is installed (the test skips cleanly otherwise).

## What's coming

The roadmap is in [`ROADMAP.md`](../../ROADMAP.md), with each item
tracked as a `git-bug` issue.  The next things on the list are
practical:

- **DJGPP cross-compile** so v3 actually runs on DOS.  The compat
  layer is already in `cli/src/compat/compat_dos.c`; the
  cross-compile target and DOSBox-X CI are the missing pieces.
- **libarchive read handler** so `.uc2` is a recognised format for
  tools that integrate with libarchive.
- **`uc2 --ingest` streaming mode** for piping `tar` or `rsync` into
  a deduplicating sink.

Beyond that, the issue tracker has speculative items for
post-quantum encryption, IPFS integration, and zero-knowledge
proofs.  Those are research directions, not promises.

## Why bother?

Two reasons.

First, archive formats are a load-bearing piece of computing
history.  Preserving the 1992 source unchanged, the 2015 LGPL
re-release unchanged, and the 2020 portable decompressor unchanged
- all in the same repository as the modern port - is what makes
the format survive its hosting choices.

Second, the master-block design from 1992 turns out to be a
surprisingly clean substrate to bolt content-defined chunking,
content-addressable storage, and verifiable timestamps onto.  Phase
3 and Phase 7 work landed without breaking the 1992 reader.  Doing
the same project as a wrapper around `gzip` would have been more
work for less reach.

The repo, with full source, license trail, test suite, and
roadmap, is at <https://github.com/evvaletov/uc2>.

---

*Eremey Valetov, May 2026.*
