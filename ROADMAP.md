# UC2 Roadmap

## Recommended next priorities (2026-11-07)

Alpha11 adds optional shared chunks across named files and versions. The
libarchive contribution is submitted; the next priorities are:

1. [ ] **Take the Total Commander and Midnight Commander proposals upstream.**
   Review the [TC catalog draft](contrib/totalcommander/CATALOG-DRAFT.md) and
   [MC extfs patch](contrib/mc/upstream/REVIEW.md), then authorize the first
   contact/submission. Confirm MC's DCO sign-off. No external contact yet.
   Tracking: 4b7fbd1.

2. [ ] **Respond to libarchive review and settle the dependency/format ID.**
   [PR #3832](https://github.com/libarchive/libarchive/pull/3832) proposes the
   default-off shared LGPL reader. Submission is complete; maintainer review,
   dependency acceptance and the proposed format ID remain pending.
   Tracking: 7bbb6b6.

3. [ ] **Complete the project-wide task-qa + task-at audit.**
   Audit the remaining core and maintained documentation. Release-scoped panels
   do not close this task. Tracking: c9edf0b.

4. [ ] **Measure and harden multi-gigabyte snapshot collections.**
   Alpha10's 1,045-file, 13 MiB measurement verifies restore and storage savings.
   Measure near the reference/manifest caps, test process-kill and low-disk
   recovery, and assess read-only/concurrent readers. Current operations require
   a writable, exclusively locked repository. [Contract](docs/snapshots.md).
   Tracking: a3bcfb4.

5. [ ] **Tune shared-chunk compression and mode selection.**
   Chunk reuse substantially reduces the measured edited-binary collections,
   but the public-source case is 9.1% larger than dictionary mode. Compare
   adaptive boundaries/dictionaries and automatic mode selection while retaining
   the existing default and limits. [Measurements](docs/benchmarks/2026-11-07-chunk-sharing.md).
   Tracking: 3fdd442.

### Implemented for 3.0.0a11

- [x] Opt-in `--chunked` / `UC2Writer(chunked=True)` shares independently stored
  chunks across partly changed named files and stored versions. Tar entries
  remain individually recoverable; opaque ingestion uses one `data` entry.
- [x] Backward direct references, chunk/recipe/file SHA-256, historical checksums,
  strict compressed-word bounds and bounded metadata. Old readers reject the
  extension; default archives and old ingestion formats remain supported.
- [x] Repeated-name metadata uses a 64 MiB absolute ceiling, replacing a ratio
  heuristic that rejected valid archives with many stored versions.
- [x] Submitted the optional legacy libarchive reader, with full upstream suite,
  CMake enabled/disabled, Autotools, real bsdtar and quota validation.
- Evidence: [alpha11 verification](docs/releases/3.0.0a11-qa.md),
  [format and compatibility](docs/chunk-sharing.md).

### Implemented for 3.0.0a10

- [x] Snapshot dry-run/apply retention after full history verification; explicit
  repair from a verified replica; atomic migration to SQLite-packed UC2 blobs
  and compressed manifests, with bounded caches and retained v1 readability.
- [x] Native CLI/Python path opening of original SAS and DOS UC2SEA sets,
  missing/damaged-part diagnostics, data-only startup handling and bounded spools.
  Creating new volume sets/platform SFX remains open under 2b65f0a/51eae2d.
- [x] `--ingest-tar` handles V7/USTAR/GNU long names and supported PAX metadata,
  exposes separate files/directories and stored versions, and reuses exact
  duplicate payloads. Unsupported tar features fail before publication.
- [x] Reviewable MC patch with parser fixtures and TC catalog draft, both unsent.
- Evidence: [alpha10 verification](docs/releases/3.0.0a10-qa.md).

### Implemented for 3.0.0a9

- [x] Native 32-/64-bit Total Commander 11.58.1 opens and extracts `.uc2`
  directly with normal WCX registration. Removed the `.u2r` workaround and
  documented migration from the forced internal association that caused it.
- [x] Browser selection of files/versions, verified ZIP STORE exports with
  portable paths, metadata and SHA-256 inventory; explicit count/output limits
  and no partial download on failure or cancellation.
- [x] Optional offline reader with digest-checked static assets, cache repair
  and updates that wait for existing tabs to close. User archives are never cached.
  Install metadata supports browsers that offer a standalone shortcut.
- [x] Chromium, Firefox and WebKit automation covers ZIP bytes/CRCs, historical
  codecs, stored versions, offline export, cache eviction/repair and update
  integrity. OS-specific installation prompts are not covered.
- Evidence: [alpha9 verification](docs/releases/3.0.0a9-qa.md).

### Implemented for 3.0.0a8

- [x] Python `uc2 snapshot` / `uc2-snapshot`: independent directory versions,
  shared SHA-256-addressed UC2 chunks, portable manifests and verified staged
  restores. Reuses the native Gear chunker. Source changes, unsafe names,
  corruption, limits and existing destinations fail explicitly.
- [x] Two-version benchmark: version two added 42,782 bytes (two new chunks
  plus its manifest), versus 1,104,851 bytes for its separate tar ingest.
  Both versions restored correctly. Corruption was detected and recovery from
  an intact copy verified. [Measurements and limits](docs/benchmarks/2026-11-07-snapshots.md).
- [x] Turbo/method 80 decoder, with SuperMaster/NoMaster fixtures verified by
  the original DOS executable and an independent Go decoder. Eighteen
  length/master cases, chained dictionaries, malformed streams and
  allocation-failure retries exercise the new code.
- [x] Optional corpus expanded to 39 archives and 684 entries: 13 historical
  collections, four unarc-rs fixtures and 22 original-encoder oracle archives.
  Every file size/hash agrees with Go; ZIP and tar migrations pass. Three SAS
  parts have pinned hashes and verified original reassembly.
- [x] libarchive variable-memory budget covers metadata and cached masters;
  entry-count/input/entry-output/total-output quotas are configurable. Directory
  IDs use a sorted index. The opt-in LGPL reader includes Turbo; rANS stays in
  the full package. CMake dependency checks and Autotools link checks are in
  the prepared patch. No upstream submission has been made.

- Release and scoped review evidence: [alpha8 verification](docs/releases/3.0.0a8-qa.md).

### Delivered in 3.0.0a7: browser reader and preservation toolkit

- [x] Local-only [browser reader](https://evvaletov.github.io/uc2/reader/):
  inspect/search paths and versions, verify supported files, download decoded
  files, and save an inventory. A WebAssembly worker bounds input, decoder
  memory, output and elapsed time; cancellation terminates decoding. No archive
  upload is used. [Build instructions and limits](web/README.md).
- [x] `uc2-preserve`: recursive collection inventory and ZIP/tar migration with
  source and decoded SHA-256 hashes, all file versions, original paths, raw DOS
  timestamps and attributes. Each output container is reopened and verified.
  Originals remain in place, existing destinations are refused, source failures
  produce explicit partial results, and output failures abort staged batches.
  [Preservation guide](docs/preservation.md).
- [x] Five bundled original-encoder fixtures with pinned hashes, plus optional
  checks for three historical archives containing 88 entries. ZIP/tar migration
  passes and all 88 decoded hashes/sizes agree with an independent Go decoder.
  Third-party archive bytes are fetched separately; only URLs, hashes and
  metadata are redistributed. [Corpus provenance](tests/archives/README.md).
At alpha7, Turbo and multi-volume decoding were unsupported. Alpha8 adds
Turbo. Alpha9 adds Firefox and WebKit coverage alongside Chromium; alpha10
adds direct SAS/UC2SEA path input.
- Release and scoped review evidence: [alpha7 verification](docs/releases/3.0.0a7-qa.md).

### Delivered in 3.0.0a6: writer APIs and audit repairs

- [x] Moved CLI archive assembly into the supported C `uc2_writer` API. Added
  Python `UC2Writer` for recursive directories, files, bytes, and streams.
  Spooling bounds payload RAM; names and format sizes are checked explicitly.
  CLI/Python destinations are replaced after successful assembly and close.
- [x] Repaired BLAKE3 with 35 official vectors, corrected LZ4 block endings
  against the reference library, rejected incomplete delta streams, and fixed
  CDC boundary/documentation issues. Audited maintained documentation claims
  and removed repeated dash clauses while preserving command flags.
- [x] Local C/Python tests and sanitizer checks, plus real DOS application
  round trips. The DOSBox harness now uses a supported headless SDL setup.
- [x] Published [alpha6 on PyPI](https://pypi.org/project/uc2/3.0.0a6/) and
  [GitHub](https://github.com/evvaletov/uc2/releases/tag/v3.0.0-alpha.6), with
  twenty wheels, four native CLI packages, integration packages, and source.
- [ ] Complete the independent three-reviewer audit panel. Subscription/access
  failures, exhausted quota/credit, and timeouts prevented completed reviews.
  Local fixes are complete; this item remains open. Release validation is in
  [the alpha6 audit notes](docs/releases/3.0.0a6-qa.md).

### Delivered: previous priorities 3-5 (3.0.0a5)

- [x] Repaired LZ77 match lookup, circular-window hashing, stale hash chains,
  and master-prefix alignment. Added compression-ratio regression coverage
  across all eight levels. The two real source collections shrink by 62.6%
  and 69.4% compared with alpha4. Random input costs more CPU; 7z still makes
  smaller archives. See [the measured results](docs/benchmarks/2026-11-05-compression.md).
- [x] Added distributable MC helper/installer and 32-/64-bit WCX packages with
  source references and license notices. Native Total Commander application
  checks complement the DLL ABI and cancellation tests; 32-bit TC uses the
  documented `.u2r` copy workaround for its legacy `.uc2` extraction path.
- [x] Prepared the optional-reader libarchive contribution, including enabled
  and disabled CMake builds, an Autotools build, and upstream-style fixtures.
  The dependency remains LGPL; the core and third-party licenses are unchanged.
- [x] Added `uc2-mount` for Linux, exposing newest files and stored versions.
  Decoded files are verified before being cached; disk and master caches have
  limits. Real FUSE tests cover parallel random reads, eviction, version paths,
  and denied writes. See [mounting archives](docs/mount.md).
- [x] Recorded release checks and remaining limits in
  [the alpha5 verification notes](docs/releases/3.0.0a5-qa.md).

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
  and cancellation preserving existing output. Alpha5 adds native UI/32-bit
  validation and release packages.
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
  Alpha6 completed the local project audit and repairs, including maintained
  prose and auxiliary codecs. Independent review remains incomplete: Claude
  subscription access is disabled, Gemini ran out of quota, Grok timed out,
  and paid fallbacks had no credit at that time. Alpha7 adds completed Gemini
  and Kimi reviews of the new browser/preservation code; Grok has not returned
  a code verdict. Those scoped reviews do not complete this project-wide task.
  Misses are not counted as approval.
  The shared workflows are at
  `/home/evaletov/.claude-skills-shared/task-qa/SKILL.md` and
  `/home/evaletov/.claude-skills-shared/task-at/SKILL.md`.
- [x] Fixed the confirmed BLAKE3 correctness gap: an independent check against
  the [official vectors](https://github.com/BLAKE3-team/BLAKE3/blob/master/test_vectors/test_vectors.json)
  originally matched 11 of 35 cases and failed 24. The repaired implementation
  passes all 35 official vectors, incremental block/chunk boundaries, repeated
  finalization, and updates after finalization. It remains a standalone module;
  the archive format still uses its original checksum. See the
  [alpha6 audit notes](docs/releases/3.0.0a6-qa.md).
- [x] Clean up the README's repeated dash clauses and ranges while preserving
  command options. Correct alpha-release status, compatibility scope, module
  versus CLI features, BLAKE3 claims, test instructions, and attribution. Remove
  invalid cp/rsync pipes and the unsupported ingest novelty claim below.
  The whole-project prose pass is done; its independent panel remains pending.
- [x] Reconcile the release state. The previously unpublished decoder fixes
  and ingest v3 are now included in GitHub `main` and `v3.0.0-alpha.4`.

Assessment evidence: a fresh GCC 14.3.1 Debug build passed 23 selected CTests,
including OTS cross-check and fuzz-regression replay. DOSBox was excluded and
the optional libarchive build was not enabled. The separate BLAKE3 vector
comparison failed as described above; reproduction details are in c9edf0b.
This was a project assessment, not the full task-qa or task-at workflow.
The integration work subsequently passed 25 selected Debug CTests, 24 Release
CTests with assertions enabled, and four focused ASan/UBSan checks. DOSBox, native MSVC, and the native Total Commander
UI were not run in that assessment. Later release checks are recorded in
`docs/releases/3.0.0a4-qa.md` and `docs/releases/3.0.0a5-qa.md`.

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
      (Both directions are covered for supported Huffman fixtures.)
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
- [x] Method 80 (Turbo) reading, including master dictionaries (alpha8).
- [ ] Multi-volume archive spanning across physical media (floppies)
- [ ] Self-extracting archives per platform (DOS COM/EXE, Linux ELF, Windows PE)
- [ ] ANSI art progress display, CP850 codepage handling
- [ ] Position as the archiver for retrocomputing preservation:
      disk images, ROM collections, BBS archive redistribution

## Phase 7: Cryptographic Integrity & Timestamping

- [x] BLAKE3 content hashing (`uc2_blake3.h`): repaired and checked against
      all 35 official unkeyed vectors, with incremental and finalization tests.
      Not integrated into the archive format.
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
- [x] Incremental directory snapshots: `uc2 snapshot create /path repository`
      through the Python CLI; shared verified chunks and independent restores.
      Retention, pruning and larger-collection work remain open (ecb9dc7).

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
      native 32-/64-bit host tests and packaged builds added in alpha5

## Phase 11: Advanced Features

- [x] Archive-as-filesystem: read-only Linux FUSE mount with version views,
      verified decoded-file caching, and bounded master caching (`uc2-mount`).
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

- 2026-11-07: Published alpha9 from `19c725a`: direct `.uc2` WCX routing in
  native TC32/TC64, selected browser ZIP exports, offline cache/repair/update
  support, and Chromium/Firefox/WebKit coverage. Public artifact hashes, live
  offline ZIP exports and the installed package are verified. Closed 81c0c8a
  and 5332dc5. Updated the next five above; the full-project audit stays open.
  Evidence: `docs/releases/3.0.0a9-qa.md`.

- 2026-11-07: Published alpha8 from `0597a35`: incremental directory snapshots,
  Turbo/method 80, the 39-archive/684-entry interoperability corpus, and bounded
  libarchive review preparation. Verified public package/release hashes, live
  Turbo downloads and the installed commands. Snapshots and Turbo are closed;
  retention/recovery, direct multi-volume input and upstream review remain open.
  The next five priorities are listed at the top of this file. Evidence:
  `docs/releases/3.0.0a8-qa.md`; the full-project audit remains open.

- 2026-11-07: Published alpha7 from `3d2e3de`: local-only browser reader,
  verified collection inventory/ZIP/tar migration, original-encoder corpus
  provenance and three historical interoperability samples. Public artifact
  hashes, live reader behavior and the installed tools are verified. Updated
  priorities are snapshots, historical compatibility, libarchive review,
  Total Commander 32-bit routing, and browser bulk/offline support. See
  `docs/releases/3.0.0a7-qa.md`; the whole-project audit remains open.

- 2026-11-07: Updated the GitHub repository description to "UltraCompressor II
  for Linux, macOS, Windows and DOS" and verified it through the GitHub API.
  Gemini, Grok, and Kimi reviewed the original and replacement. The final
  eight-word description passes the prose scan. Optional suggestions to add
  "archiver", a serial comma, or "(DJGPP)" were left out to keep the description
  short; build and packaging details remain in the README. This review covers
  the description only. The project-wide independent audit (c9edf0b) remains open.

- 2026-11-05: Completed the compression, integration packaging, and FUSE work
  for alpha5. The measured source-collection improvements and CPU tradeoffs
  are in `docs/benchmarks/2026-11-05-compression.md`. The libarchive review patch
  is prepared locally; upstream acceptance and the full audit remain open.

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
