# UC2 Roadmap

## Recommended next priorities (2026-11-05)

The Python reader and native CLI releases are published. These are the next
five priorities. The phases below remain the longer-term feature inventory;
the additional project-wide task-qa + task-at audit remains open.

1. [ ] **Build a browser-only UC2 reader and inspector.**
   Let someone drop an old archive onto a page, inspect entries and versions,
   verify supported content, and download extracted files. Process archive bytes
   locally, with no upload. Compile the C reader using
   [Emscripten](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html)
   and run it in a worker with resource limits. First prove listing and reading
   one fixture; use the existing generated-C approach for the SuperMaster data
   instead of assuming the native assembly embedding works in WebAssembly.
   This would give preservation users a useful first experience without an
   installation and provide a concrete demo to share.

2. [ ] **Make UC2 a preservation and migration toolkit.**
   Add batch inventory and migration to ZIP or tar, with a machine-readable
   manifest of original paths, versions, timestamps, attributes, and verified
   file hashes. Preserve the source archive and report metadata that cannot be
   represented in the destination. Reuse existing output-format libraries.
   Build a redistributable compatibility corpus from real old archives and use
   it to prioritize missing methods, especially Turbo/method 80 (b3e413f).
   Start with one directory-to-manifest migration and verify every restored
   file. The product hypothesis is that rescuing old collections is a clearer
   reason to adopt UC2 than general compression claims.

3. [ ] **Improve compression where the measurements show a gap.**
   The [five collection benchmarks](docs/benchmarks/2026-11-05.md) verified all
   60 round trips. Ingest improves on ordinary UC2 for related versions, but 7z
   and tar+zstd make smaller archives on every tested collection. Profile match
   selection, master construction, and per-chunk metadata before adding
   snapshots or version bundles. Retest promising changes on the synthetic
   corpus and redistributable real collections, including the random-data
   control and all archive overhead.

4. [ ] **Package the working integrations for users and upstream review.**
   Package native releases with matching sources and license notices. Test the
   WCX plugin inside Total Commander on native Windows and build the 32-bit
   variant. Prepare the libarchive contribution around its BSD-option adapter;
   resolve the core dependency license and official format ID before seeking
   acceptance. The current recipe produces a patched bsdtar using private APIs.
   No upstream submission has been made. Tracking: 7bbb6b6, 61ed856, 65daf3b.

5. [ ] **Mount UC2 archives as read-only folders.**
   Prototype a Linux [FUSE](https://github.com/libfuse/libfuse) filesystem with
   listing, metadata, and file reads. Expose stored versions in a separate
   namespace so ordinary tools can compare them. Bound decoded-file and master
   caches, define behavior for corrupted entries, and test concurrent reads.
   Start with one reference archive mounted without extracting the whole
   collection to disk. Reuse the reader API before expanding platform support.

### Delivered: Python package and native CLI releases (2026-11-05)

- [x] Published [uc2 3.0.0a4 on PyPI](https://pypi.org/project/uc2/3.0.0a4/):
  `UC2File`, versioned entry metadata, reads, streamed copies, verification,
  bounded extraction, and the bundled CLI. Python archive creation remains
  available through the CLI, pending a library writer API.
- [x] Twenty wheels cover CPython 3.10-3.14 on Linux x86-64, Windows x86-64,
  and Intel/Apple Silicon macOS. Each installed wheel passes 35 tests.
- [x] [Native CLI downloads](https://github.com/evvaletov/uc2/releases/tag/v3.0.0-alpha.4)
  for all four targets, matching source, license texts, build information, and
  SHA-256 checksums. The release commit is `bc33446`.
- [x] Source-distribution rebuild, ASan/UBSan, native platform tests, bounded
  fuzzing, actual bsdtar, and native Windows WCX ABI checks. Windows ingestion
  and stdout extraction now preserve binary bytes.
- [x] Release-scoped QA and prose checks produced fixes for negative limits,
  ambiguous metadata, exception consistency, installation rules, and permission
  documentation. Only one external reviewer completed; the full audit below
  remains open. See [release verification](docs/releases/3.0.0a4-qa.md).

### Delivered from the previous priorities 4-5 (2026-11-05)

- [x] Actual patched bsdtar 3.8.11 with pipe input, directories, master blocks,
  Huffman/rANS, and selective extraction. A 128 MiB entry restores under a
  96 MiB address-space limit. Payloads spool to temporary files; metadata and
  cached masters still grow with archive contents.
- [x] Read-only Midnight Commander extfs reader, tested through its interface
  and in the actual MC TUI. Literal wildcard and version-looking names work.
- [x] 64-bit Total Commander WCX reader, built with MinGW and exercised through
  a Wine DLL host: Unicode archive path, list/skip/test/extract, file comparisons,
  and cancellation preserving existing output. Native UI/32-bit tests remain.
- [x] Targeted dual license approved by Eremey: GPL-3.0-or-later OR BSD-2-Clause
  for the libarchive adapter and independent glue. Third-party and core licenses
  stay in place; the derived WCX adapter retains LGPL.
- [x] Reproducible 7z/tar+zstd benchmarks with JSON/CSV and SHA-256 verification.
  Version-bundle expansion is deferred based on these measurements.
- [x] Streamed CLI ingest with bounded payload buffers, collision byte checks
  via a disk spool, staged destination replacement, and explicit format limits.
  A 64 MiB input passes under a 48 MiB address-space limit.

### Additional TODO requested by the user: task-qa + task-at

- [ ] Complete the project-wide **task-qa + task-at** audit. Tracked in
  git-bug **c9edf0b**. This is an additional task alongside the five priorities.
  The Python alpha received release-scoped checks and one completed Gemini
  review; Claude access failed, Grok timed out, and API fallbacks were
  unavailable. This does not complete the three-reviewer or whole-project audit.
  The shared workflows are at
  `/home/evaletov/.claude-skills-shared/task-qa/SKILL.md` and
  `/home/evaletov/.claude-skills-shared/task-at/SKILL.md`.
- [ ] Fix the confirmed BLAKE3 correctness gap: an independent check against
  the [official vectors](https://github.com/BLAKE3-team/BLAKE3/blob/master/test_vectors/test_vectors.json)
  matched 11 of 35 cases and failed 24, starting at 65 input bytes. The current
  unit tests pass without checking those outputs. Include official vectors and
  incremental block/chunk boundaries in the repair. No BLAKE3 call was found
  in the current CLI, compressor, decompressor, or blockstore; do not infer
  ordinary archive corruption from this standalone-module finding.
- [x] Clean up the README's repeated dash clauses and ranges while preserving
  command options. Correct alpha-release status, compatibility scope, module
  versus CLI features, BLAKE3 claims, test instructions, and attribution. Remove
  invalid cp/rsync pipes and the unsupported ingest novelty claim below.
  The whole-project task-at workflow remains pending.
- [x] Reconcile the release state. The previously unpublished decoder fixes
  and ingest v3 are now included in GitHub `main` and `v3.0.0-alpha.4`.

Assessment evidence: a fresh GCC 14.3.1 Debug build passed 23 selected CTests,
including OTS cross-check and fuzz-regression replay. DOSBox was excluded and
the optional libarchive build was not enabled. The separate BLAKE3 vector
comparison failed as described above; reproduction details are in c9edf0b.
This was a project assessment, not the full task-qa or task-at workflow.
The integration work subsequently passed 25 selected Debug CTests, 24 Release
CTests with assertions enabled, and four focused ASan/UBSan checks. DOSBox, native MSVC, and the native Total Commander
UI were not run in this pass. New CI jobs are configuration awaiting execution.

## Phase 1: Decompression MVP (DONE)

- [x] Port Bobrowski's libunuc2 decompression engine
- [x] CLI tool with list/extract/test/pipe modes
- [x] CMake build system (Linux, MSVC fallback for super.bin)
- [x] Win32 compat layer carried over
- [x] Tagged v3.0.0-alpha.1

## Phase 2: Original Compression Engine (DONE)

- [x] Port LZ77+Huffman compressor from `ULTRACMP.CPP`, `TREEGEN.CPP`, `TREEENC.CPP`
- [x] Write as the inverse of the decompressor (Bobrowski's code is the spec)
- [x] Compression levels 2=Fast, 3=Normal, 4=Tight, 5=Ultra
- [x] CLI create mode (`uc2 -w`), compression level flag (`-L`)
- [x] SuperMaster dictionary support (built-in 49 KB dictionary)
- [x] Round-trip testing: 37 unit tests + CLI integration tests
- [x] Round-trip testing vs original `uc2pro.exe` in DOSBox
      (Direction: original creates -> UC2 v3 extracts  --  verified.
       Reverse direction is a known limitation: the original UC2 Pro
       cannot read UC2 v3 archives due to compression bitstream
       differences.)
- [x] Backward compatibility with original UC2 Pro (listing +
      extraction verified for multi-file archives in both directions
      in automated DOSBox-X test).
- [x] Custom Huffman tree optimization: use default tree for first
      small block (< 256 ibuf entries), custom trees for larger
      blocks.  Matches the original's bFlag logic.  37% compression
      improvement on text data while maintaining backward compat.
- [x] UC2 personality: status messages continuing the original's
      tradition ("Everything went OK", compression level names,
      "Fast, reliable and superior compression").  Suppressed by -q.

## Phase 3: Modernized Master-Block Deduplication

UC2's signature feature from 1992, ahead of its time.  Modernize into
something no mainstream archiver offers.

- [x] Content-fingerprint file grouping (FNV-1a hash of first 4096 bytes)
- [x] Custom master-block generation from largest file in each group
- [x] MASMETA central directory records with full metadata
- [x] Masters compressed with SuperMaster, files compressed with custom master
- [x] CLI integration test validating master deduplication round-trip
- [x] Content-defined chunking (CDC) with Gear rolling hash
      (`uc2_cdc.h`): chunker library + integration into archive
      creation.  Files sharing content at ANY position (not just
      identical prefixes) are now grouped for master-block dedup.
- [x] Merkle DAG of deduplicated blocks (`uc2_merkle.h`):
      content-addressable chunk trees with 64-bit FNV-1a hashes,
      structural similarity comparison, single-byte-change resilience.
      8 unit tests including partial overlap detection.
- [x] Cross-archive dedup via shared block store (`uc2_blockstore.h`):
      content-addressable chunk storage with two-level directory
      layout, dedup statistics, read-back verification.
      6 unit tests including cross-archive dedup scenario.
- [x] Near-duplicate detection via SimHash (`uc2_simhash.h`):
      64-bit locality-sensitive fingerprint with Hamming distance,
      detects patched executables (16 changed bytes in 8KB: dist <= 8).
      6 unit tests.
- [x] Delta compression (`uc2_delta.h`): binary diff with COPY/INSERT
      instructions, hash-based source matching. 96-byte patch in 16KB
      file -> >50% size savings.  6 unit tests including round-trip.

## Phase 4: Modern Compression Backends

Pluggable algorithms behind new method IDs; original Method 4 kept for
backward compatibility.

- [x] rANS entropy coder (`uc2_rans.h`) integrated into archive format
      as method 10.  Levels 6-9 use rANS (vs 2-5 Huffman).  32-bit
      table-based rANS, <5% overhead vs Shannon entropy.  End-to-end
      round-trip verified (create/list/extract/verify).
- [x] zstd-inspired dictionary compression (`uc2_dict.h`): formal
      dictionary metadata with content-hash IDs, integrity checksums,
      serialization format, and cross-archive sharing via block store.
      6 unit tests including round-trip and corruption detection.
- [x] LZ4 ultra-fast mode (`uc2_lz4.h`): single-probe hash table,
      O(1) match finding, 4-byte minimum match, variable-length
      literal/match token encoding.  6 unit tests including
      text, binary, all-same, incompressible, and small inputs.
- [x] Content-aware preprocessing (`uc2_preprocess.h`):
      BCJ (E8/E9 x86 address normalization), BWT (Burrows-Wheeler
      for text), delta filter (byte-wise with configurable stride),
      automatic content detection (text/x86/structured/binary).
      11 unit tests.
- [x] Built-in benchmark mode (`uc2 -B files...`): tests all 8 Huffman/rANS
      levels plus LZ4, reports compressed size, ratio, and timing.

## Phase 5: Quantum-Resistant Encryption

No mainstream archiver offers post-quantum encryption.

- [ ] CRYSTALS-Kyber (NIST PQC standard) for key encapsulation, pure C
      (PQClean project, public domain)
- [ ] AES-256-GCM for authenticated payload encryption
- [ ] Hybrid mode: classical ECDH + Kyber for transition period
- [ ] Passphrase-based key derivation via Argon2
- [ ] Per-file selective encryption within archives
- [ ] Plausible deniability: multi-archive-in-one with separate passwords.
      Each password decrypts a different archive layer.  Under hostile
      pressure, revealing one password gives access to a decoy layer
      while the real archive remains hidden and indistinguishable from
      random padding.  (Inspired by VeraCrypt hidden volumes.)

## Phase 6: DOS / FreeDOS / Retro-Computing

- [x] DJGPP cross-compilation toolchain: `cmake/djgpp.cmake`
      builds `uc2.exe` against the prebuilt DJGPP gcc 7.2 / 12.2 from
      `andrewwutw/build-djgpp`.  Output is a 32-bit DPMI DOS executable
      (MZ + COFF + go32 stub).  See `cmake/README-djgpp.md` for the
      one-time setup (CPATH unset is required on hosts that export it).
- [x] DOSBox-X smoke test: `tests/scripts/dos_smoke.sh` runs `uc2 -h`
      and `uc2 -l <archive>` under DOSBox-X via the flatpak; verifies
      the cross-compiled binary actually loads under a real DPMI host.
      Real vintage hardware test still pending.
- [ ] Method 80 (Turbo) support
- [ ] Multi-volume archive spanning across physical media (floppies)
- [ ] Self-extracting archives per platform (DOS COM/EXE, Linux ELF, Windows PE)
- [ ] ANSI art progress display, CP850 codepage handling
- [ ] Position as the archiver for retrocomputing preservation:
      disk images, ROM collections, BBS archive redistribution

## Phase 7: Cryptographic Integrity & Timestamping

- [-] BLAKE3 content hashing (`uc2_blake3.h`): implementation present, but
      official-vector failures require repair (c9edf0b). The original module has
      256-bit digests, incremental and one-shot API, constant-time
      comparison, tree hashing structure.  7 unit tests including
      avalanche, incremental-vs-oneshot, and single-byte updates.
- [x] SHA-256 (`uc2_sha256.h`): pure-C FIPS 180-4 implementation,
      one-shot and incremental API.  6 unit tests against published
      test vectors (empty, "abc", 56-byte, 1M `'a'`, byte-by-byte
      incremental, every-split-point boundary).
- [x] OpenTimestamps integration (`uc2_ots.h`): pure-C parser,
      serializer, and walker for the standard `.ots` proof format.
      Append-only sidecar trailer (magic-bracketed, reverse-scan-safe)
      stores the proof verbatim and preserves backward compatibility
      with the original UC2 Pro reader.  Walker supports the
      calendar-path subset (APPEND, PREPEND, SHA256); proofs with other
      crypto ops are accepted as structurally valid but flagged for
      `ots verify` follow-up.  CLI: `--ots-attach`, `--ots-extract`,
      `--ots-info`; `uc2 -t` recomputes archive SHA-256 and verifies
      the leaf and walk.  Strict-canonical-varint parser, 64-bit
      overflow check, depth-bounded recursion, varbytes cap.
      17 unit tests.
- [ ] OTS upgrade: fetch the upgraded proof from the calendar after
      the Bitcoin attestation has been minted (~1-6h), replace the
      pending-only trailer with the Bitcoin block-header attestation.
- [ ] Useful for legal/forensic archiving, software provenance, digital
      preservation

## Phase 8: Decentralized & Cloud Integration

- [ ] IPFS pinning: `uc2 --ipfs-pin archive.uc2` to publish,
      `uc2 --ipfs-get <CID>` to retrieve
- [ ] Content-addressable dedup maps directly to IPFS CIDs;
      master blocks become sharable across users ("swarm dedup")
- [ ] Cloud archiving backend: `uc2 --s3 s3://bucket/path` for
      streaming compress-to-cloud with dedup-aware incremental uploads
- [ ] Filecoin/Sia for decentralized paid storage (optional)

## Phase 9: Zero-Knowledge Proofs (Experimental)

ZK proofs extend the Merkle DAG and encryption layers with
privacy-preserving verification.  Most valuable for decentralized and
compliance scenarios; heavyweight, so implemented as an optional module.

- [ ] **Prove archive integrity without revealing contents**  --  ZK proof
      that the archive's Merkle root matches claimed file hashes, without
      exposing the tree structure.  Enables auditing of IPFS-shared
      encrypted archives.
- [ ] **Selective disclosure from encrypted archives**  --  prove a specific
      file (by hash) exists in an encrypted archive without decrypting
      anything else.  Useful for collaborative encrypted team archives.
- [ ] **Verifiable deduplication**  --  ZK proof that master-block dedup was
      performed correctly across archives without revealing block contents.
      Builds trust in distributed dedup without data leaks.
- [ ] **Compliance proofs**  --  prove properties ("archive created before
      date Y", "archive does not contain file with hash H") without
      revealing contents.  For regulatory/legal use cases.
- [ ] Implementation: Halo2 or Bulletproofs (no trusted setup) via
      Rust-to-C wrapper or WASM bridge; compile-time optional module.
      STARKs preferred over SNARKs for quantum resistance alignment
      with Phase 5.

### ZK Feasibility Notes

ZK adds genuine value for privacy-focused decentralized archiving (Phases
7--8) but is heavyweight for a CLI tool.  SNARKs require pairing-friendly
curves (not quantum-resistant); **STARKs are preferred** as they align
with the post-quantum direction and need no trusted setup.  Proof
generation is slow (seconds to minutes for complex circuits) so this is
an opt-in feature, not on the critical path.  Prototype in a fork first.

## Phase 10: Ecosystem Integrations

### libarchive plugin

A libarchive integration could make UC2 available to its consumers. The
current recipe demonstrates actual bsdtar support; other applications need
their own builds and validation.

- [x] libarchive read handler: magic recognition, directories, long-name tags,
      masters, and Huffman/rANS. Temporary-file spooling bounds payload RAM.
      Both the public-API driver and actual bsdtar are tested. Upstream work
      remains open (7bbb6b6).
- [ ] libarchive write handler (compression, once Phase 2 is done)

### Streaming dedup ingestion

UC2 accepts byte streams from tools that emit archive data. The benchmarks
above determine which performance claims are supported.

```sh
tar cf - project/ | uc2 --ingest backup.uc2
uc2 --ingest-restore backup.uc2 > restored.tar
```

- [x] `uc2 --ingest` mode v1: stdin -> CDC -> sidecar blockstore at
      `<archive>.blocks/` -> chunk-hash manifest.  `uc2 --ingest-restore`
      reverses the round-trip.  Tested: small/multichunk round-trip,
      idempotent dedup on repeat ingest, empty stream, bad-magic
      rejection.  Now legacy: writer defaults to v3.
- [x] `uc2 --ingest` v2 (legacy): self-contained archive with the
      chunk pool embedded inside the archive file itself.  No sidecar
      directory.  Manifest entries carry absolute file offsets;
      duplicate hashes share an offset (intra-call dedup).
      Cross-archive dedup is not preserved -- the trade-off is the
      single-file UX.  v1 archives still readable for restore.
- [x] `uc2 --ingest` v3 (default): output is a real UC2 archive.  Each
      CDC chunk occurrence is a `part-N` file entry in stream order;
      unique chunks are stored once (payload dedup) as NoMaster blocks,
      so `uc2 -l` lists it and `uc2 -x` extracts it like any archive.
      `uc2 --ingest-restore` walks the entries in order and concatenates
      them; v1/v2 archives are still restored via magic auto-detection.
      Tested (`cli_ingest`): dedup on periodic input, byte-exact
      round-trip, extract-and-concatenate, empty stream; ASan-clean on
      large no-dedup and cross-region-dedup streams.  Approach (A) of
      git-bug 59bec0d; the master-block-backed (B) variant, which would
      shrink the central directory to one entry per *unique* chunk, is a
      follow-up.
- [ ] Tar-entry preservation: parse tar boundaries inside --ingest
      so individual files are recoverable as archive entries (bd13adb)
- [ ] Incremental snapshots: `uc2 snapshot /path repo.uc2`
      (borg/restic-style deduplicating backups without filesystem support)

### Foreign archive format support

Read (and optionally write) other archive formats, enabling UC2 as a
universal archive tool and migration path for legacy collections.

- [ ] ZIP read/write (deflate, store; the universal baseline format)
- [ ] RAR read (v4/v5; for extraction from existing collections)
- [ ] TGZ/tar.gz read/write (tar + gzip; Unix ecosystem staple)
- [ ] ISO 9660 read (CD/DVD images; retro-computing preservation)

### File manager plugins

Bobrowski already shipped prototypes; update for UC2 v3.

- [x] Midnight Commander extfs reader (`contrib/mc/`, 61ed856)
- [x] Total Commander WCX reader (`contrib/totalcommander/`, 65daf3b);
      64-bit DLL tested in Wine, native host/32-bit validation pending

## Phase 11: Advanced Features

- [ ] Archive-as-filesystem: FUSE mount for `.uc2` on Linux (read-only,
      decompress-on-the-fly with master-block caching)
- [ ] Compression tournaments / community challenges
- [ ] Neural/learned compression preprocessor (modern platforms only,
      not DOS  --  optional compile-time module)
- [ ] Jupyter kernel for interactive archive exploration and compression
      research (Python, building on foxkernel experience):
      - Rich HTML tables for archive listings with compression ratios
      - Interactive dedup graph visualization (master-block DAG: which
        files share blocks, space savings)
      - Inline benchmark charts comparing methods/levels (ratio vs speed)
      - Version diff visualization between archive snapshots
      - Huffman tree / ANS state table visualization for algorithm
        development

## Testing Strategy

- Create reference UC2 archives using original `uc2pro.exe` in DOSBox
- Unit tests: magic detection, Fletcher checksum, CP850->UTF-8
- Integration: extract test archives, compare SHA-256 against manifest
- Phase 2: round-trip (new compress -> old extract in DOSBox, and vice versa)
- Phase 3+: dedup correctness, cross-archive block sharing
- Phase 5: encryption round-trip, key derivation vectors
- Phase 9: ZK proof soundness and completeness

## Maintenance Log

- 2026-11-05: Published the Python reader and four native CLI packages as
  `3.0.0a4` / `v3.0.0-alpha.4`. Release evidence and review limitations are in
  `docs/releases/3.0.0a4-qa.md`. The full audit and BLAKE3 repair remain open.

- 2026-11-05: Delivered integrations and collection benchmarks described above.
  CLI extraction errors now produce a failing exit status, allowing the MC
  helper to preserve destinations on error. Release test executables retain
  assertions. Added actual bsdtar and native Windows WCX CI jobs. README prose
  and claims corrected; full task-qa + task-at and BLAKE3 repair remain open.


- 2026-06-11: Fixed the rANS (L6-9) extraction crash and >64KB silent
  corruption (git-bug d747658, closed): master COMPRESS records now
  carry the real method (10 at L6-9); the rANS decoder consumes the
  EOB pair instead of desyncing the bit cursor; bits_feed handles
  short reads without overrunning its buffer; compressor chunk loads
  and rANS output flushing respect the 64KB circular-window edge.
  Found debugging extraction on sdf.org (NetBSD 10) but reproducible
  everywhere. New regression test: cli_bigfile. Follow-up filed:
  bf73896 (ftell offsets >4GB truncate silently; P2).

- 2026-06-13: DOS build now has CI coverage (DJGPP v3.4 toolchain,
  sha-pinned; builds uc2.exe via cmake/djgpp.cmake; git-bug 9379647).
  Consolidated the two DJGPP toolchain files onto djgpp.cmake and
  removed the redundant djgpp-toolchain.cmake.
- 2026-06-13: Damaged-archive decode hardening (git-bug f049d6d):
  decompress_block match-length overflow guard (runtime check
  replacing an NDEBUG assert), decompress_cdir end-bounding, and a
  CLI handle/FILE leak fix on the cdir-error path. A prefix-sweep
  fuzzer drove the fixes; a residual rare cdir-parser OOB it surfaces
  is tracked for a systematic hardening + fuzzing pass (git-bug
  69e8e52).

- 2026-06-13: Security task-qa + fixes. A libFuzzer harness (tests/fuzz/)
  found a heap overflow in the damaged-cdir parse path (fixed, 69e8e52);
  also fixed Zip-Slip extraction, decoder bounds (tree/LZ/delta), and
  allocation-overflow guards. v3.0.0-alpha.3 tagged. Residual
  decompression-bomb DoS tracked (b8f933c).

- 2026-06-14: Decompression-bomb DoS fixed (git-bug b8f933c). The
  multi-second decode was an integer-underflow loop, not a ratio bomb:
  a zero-length match code reached the `while (--len)` copy in
  decompress_block and underflowed to ~4.29 billion iterations. Fix
  rejects len==0 and checks the distance/length extra-bit reads for
  truncation. Fuzzing infrastructure landed alongside (git-bug a732e24):
  a `fuzz` CI job (clang + libFuzzer + ASan) over the read path and a new
  compress/round-trip harness, a seed corpus and committed regression
  artifacts, and a portable `fuzz_regression` ctest (30 s TIMEOUT) that
  guards the hang on every platform in the matrix. Follow-up: ht_dec
  ht_mktree return check (git-bug c57b865, low severity).
