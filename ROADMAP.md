# UC2 Roadmap

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

- [x] DJGPP cross-compilation toolchain: `cmake/djgpp-toolchain.cmake`
      builds `uc2.exe` against the prebuilt DJGPP gcc 7.2 / 12.2 from
      `andrewwutw/build-djgpp`.  Output is a 32-bit DPMI DOS executable
      (MZ + COFF + go32 stub).  See `cmake/README-djgpp.md` for the
      one-time setup (CPATH unset is required on hosts that export it).
- [ ] Test on real vintage hardware and DOSBox/FreeDOS
- [ ] Method 80 (Turbo) support
- [ ] Multi-volume archive spanning across physical media (floppies)
- [ ] Self-extracting archives per platform (DOS COM/EXE, Linux ELF, Windows PE)
- [ ] ANSI art progress display, CP850 codepage handling
- [ ] Position as the archiver for retrocomputing preservation:
      disk images, ROM collections, BBS archive redistribution

## Phase 7: Cryptographic Integrity & Timestamping

- [x] BLAKE3 content hashing (`uc2_blake3.h`): pure C implementation,
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

Highest-leverage integration.  Adding UC2 read/write support to libarchive
makes `.uc2` a first-class format for `bsdtar`, `cmake`, `pkg(8)`,
file-roller, Ark, and dozens of other tools across the Linux ecosystem.

- [-] libarchive read handler (decompression/listing): milestone 1
      shipped -- bid() recognises UC2 magic via libarchive's private
      __archive_read_ahead, plugin self-registers via
      __archive_read_register_format.  Builds cleanly against a
      libarchive source tree at -DLIBARCHIVE_SOURCE_DIR=<path>.  Full
      read_header / read_data wiring to libuc2 is milestone 2+.
- [ ] libarchive write handler (compression, once Phase 2 is done)

### Streaming dedup ingestion

Position UC2 as a deduplicating storage layer that other tools pipe into.
No other CLI archiver offers this.

```sh
rsync -a /data/ | uc2 --ingest repo.uc2      # dedup on receive
tar cf - /project | uc2 --ingest backup.uc2   # dedup tar stream
cp -a /snapshot/ | uc2 --ingest backup.uc2    # incremental dedup
```

- [x] `uc2 --ingest` mode v1: stdin -> CDC -> sidecar blockstore at
      `<archive>.blocks/` -> chunk-hash manifest.  `uc2 --ingest-restore`
      reverses the round-trip.  Tested: small/multichunk round-trip,
      idempotent dedup on repeat ingest, empty stream, bad-magic
      rejection.  Not yet integrated with the master-block archive
      layout (sidecar blockstore is a separate format with magic
      `UC2INGST`); tar entry boundaries are not preserved.  Follow-up
      tracked separately.
- [ ] `uc2 --ingest` v2: integrate with master-block archive layout
      so output is a real UC2 v3 archive, not a sidecar manifest
- [ ] Tar-entry preservation: parse tar boundaries inside --ingest
      so individual files are recoverable as archive entries
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

- [ ] Midnight Commander VFS plugin (update `misc/mc.ext` and `misc/uuc2`)
- [ ] Total Commander WCX plugin (update `misc/unuc2-wcx.c`)

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
