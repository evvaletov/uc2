Roadmap
=======

The development roadmap is maintained in ``ROADMAP.md`` at the project
root.  Key phases:

1. **Decompression MVP** — Done. Portable decompressor, CLI tool,
   CMake build system.

2. **Original Compression Engine** — Done. LZ77+Huffman compressor
   producing bitstreams compatible with the original format.

3. **Modernized Master-Block Deduplication** — Content-defined
   chunking, Merkle DAG, cross-archive dedup, near-duplicate detection.

4. **Modern Compression Backends** — ANS entropy coding,
   zstd-inspired dictionary compression, content-aware preprocessing.

5. **Quantum-Resistant Encryption** — CRYSTALS-Kyber + AES-256-GCM.

6. **DOS / FreeDOS / Retro-Computing** — DJGPP toolchain, vintage
   hardware support, self-extracting archives.

7. **Cryptographic Integrity & Timestamping** — BLAKE3 hashing,
   OpenTimestamps.

8. **Decentralized & Cloud Integration** — IPFS pinning,
   content-addressable dedup, cloud archiving.

9. **Zero-Knowledge Proofs** — Privacy-preserving archive verification.

10. **Ecosystem Integrations** — libarchive plugin, streaming dedup
    ingestion, file manager plugins.

See the full roadmap: `ROADMAP.md on GitHub
<https://github.com/evvaletov/uc2/blob/main/ROADMAP.md>`_.
