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
      (Direction: original creates → UC2 v3 extracts — verified.
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
- [ ] Give UC2 a voice: status and progress messages with personality,
      continuing the original's tradition ("Do not worry, you have got
      the tree", "decompression always lightspeed", FAST/TIGHT/S-TIGHT
      levels, `BoosterOn()`).  Not a --fun flag — just how UC2 talks.
      Warm, confident, slightly quirky, never annoying.  Suppressed
      by -q/--quiet for scripting.

## Phase 3: Modernized Master-Block Deduplication

UC2's signature feature from 1992, ahead of its time.  Modernize into
something no mainstream archiver offers.

- [x] Content-fingerprint file grouping (FNV-1a hash of first 4096 bytes)
- [x] Custom master-block generation from largest file in each group
- [x] MASMETA central directory records with full metadata
- [x] Masters compressed with SuperMaster, files compressed with custom master
- [x] CLI integration test validating master deduplication round-trip
- [ ] Content-defined chunking (CDC) with rolling hashes (Gear or Rabin-Karp)
      replacing fixed-block exact matching
- [ ] Merkle DAG of deduplicated blocks (Git pack-style content addressing)
- [ ] Cross-archive and cross-version dedup via shared block stores
- [ ] Near-duplicate detection via simhash/minhash for fuzzy dedup
      (patched executables, slightly edited documents)
- [ ] Delta compression across file versions (xdelta/bsdiff-style binary
      deltas stored against master blocks)

## Phase 4: Modern Compression Backends

Pluggable algorithms behind new method IDs; original Method 4 kept for
backward compatibility.

- [ ] ANS/rANS entropy coder as drop-in Huffman replacement (~5KB pure C,
      5--15% ratio improvement on skewed distributions)
- [ ] zstd-inspired dictionary compression integrated with master blocks
      (deduped masters become shared zstd dictionaries — unique synergy)
- [ ] LZ4 ultra-fast mode for real-time or low-resource scenarios
- [ ] Content-aware preprocessing pipeline:
      - BWT (Burrows-Wheeler) for text
      - E8/E9 transform for x86 executables (BCJ filter)
      - Delta filter for structured/tabular data
- [ ] Built-in `uc2 --benchmark` mode: test all methods on input, report results

## Phase 5: Quantum-Resistant Encryption

No mainstream archiver offers post-quantum encryption.

- [ ] CRYSTALS-Kyber (NIST PQC standard) for key encapsulation, pure C
      (PQClean project, public domain)
- [ ] AES-256-GCM for authenticated payload encryption
- [ ] Hybrid mode: classical ECDH + Kyber for transition period
- [ ] Passphrase-based key derivation via Argon2
- [ ] Per-file selective encryption within archives

## Phase 6: DOS / FreeDOS / Retro-Computing

- [ ] DJGPP cross-compilation toolchain
- [ ] Test on real vintage hardware and DOSBox/FreeDOS
- [ ] Method 80 (Turbo) support
- [ ] Multi-volume archive spanning across physical media (floppies)
- [ ] Self-extracting archives per platform (DOS COM/EXE, Linux ELF, Windows PE)
- [ ] ANSI art progress display, CP850 codepage handling
- [ ] Position as the archiver for retrocomputing preservation:
      disk images, ROM collections, BBS archive redistribution

## Phase 7: Cryptographic Integrity & Timestamping

- [ ] BLAKE3 content hashing for every block (fast, pure C, ~15KB)
- [ ] OpenTimestamps integration: cryptographic proof of archive creation
      time anchored to Bitcoin blockchain (one HTTP call, small proof blob
      stored in archive metadata)
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

- [ ] **Prove archive integrity without revealing contents** — ZK proof
      that the archive's Merkle root matches claimed file hashes, without
      exposing the tree structure.  Enables auditing of IPFS-shared
      encrypted archives.
- [ ] **Selective disclosure from encrypted archives** — prove a specific
      file (by hash) exists in an encrypted archive without decrypting
      anything else.  Useful for collaborative encrypted team archives.
- [ ] **Verifiable deduplication** — ZK proof that master-block dedup was
      performed correctly across archives without revealing block contents.
      Builds trust in distributed dedup without data leaks.
- [ ] **Compliance proofs** — prove properties ("archive created before
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

- [ ] libarchive read handler (decompression/listing)
- [ ] libarchive write handler (compression, once Phase 2 is done)

### Streaming dedup ingestion

Position UC2 as a deduplicating storage layer that other tools pipe into.
No other CLI archiver offers this.

```sh
rsync -a /data/ | uc2 --ingest repo.uc2      # dedup on receive
tar cf - /project | uc2 --ingest backup.uc2   # dedup tar stream
cp -a /snapshot/ | uc2 --ingest backup.uc2    # incremental dedup
```

- [ ] `uc2 --ingest` mode: streaming input with master-block dedup
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
      not DOS — optional compile-time module)
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
