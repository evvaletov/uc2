# Reviving UltraCompressor II

UltraCompressor II was a DOS archiver written by Nico de Vries with Danny
Bezemer in the 1990s. Its shared master dictionaries and stored file versions
make it an interesting format to preserve. UC2 v3 adds a portable compressor
to Jan Bobrowski's reader, so current machines can create and recover UC2
archives.

## Where the code came from

In 2015, Vladislav Sagunov asked de Vries to release the source under a free
software license. De Vries published it under LGPL-3.0, with a Borland runtime
exception for parts of the DOS build. That release is preserved in
`original/UC2_source/`.

Bobrowski wrote the portable `unuc2` reader in 2020-2021. The derived library
files retain LGPL-3.0-only and the derived CLI retains GPL-3.0-or-later.
Eremey Valetov's v3 compressor and new core modules use GPL-3.0-or-later.
The adapter and integration license choices are described in the
[per-file audit](../license-audit.md).

## Reading and writing archives

The CLI creates, lists, tests, and extracts archives with nested directories
and stored versions. Levels 2-5 use the legacy Huffman format; levels 6-9 use
rANS and require the v3 reader. DOSBox-X tests check supported Huffman fixtures
in both directions against the original UC2 Pro. They do not establish
support for every historical archive: Turbo/method 80 remains unsupported.

The writer groups input files by shared CDC chunk fingerprints and builds
custom dictionaries from their content. Each file is compressed against its
assigned dictionary. The separate `--ingest` mode splits a byte stream into
chunks and stores identical chunks once. A tar stream stays a byte stream;
its members do not become individually named UC2 entries.

The library also contains experimental block-store, Merkle, SimHash, delta,
and preprocessing modules. Their presence in the library does not mean the
CLI provides incremental snapshots or applies every module to its archives.

## Integrity and timestamps

The archive format uses a checksum to detect accidental corruption. It is
not cryptographic authentication. The standalone BLAKE3 module now passes
the official unkeyed vectors after an audit found incorrect block and tree
finalization. It is not connected to the archive format.

OpenTimestamps support hashes the recorded archive bytes with SHA-256 and
attaches a supplied `.ots` proof in a trailer. UC2 checks the proof structure
and matching leaf digest. Bitcoin attestation verification still requires
an external verifier such as `ots verify`; attaching a pending calendar
receipt does not establish a confirmed timestamp.

```sh
uc2 -w -L 4 demo.uc2 file1.txt file2.txt
uc2 -t demo.uc2
ots stamp demo.uc2
uc2 --ots-attach demo.uc2.ots demo.uc2
```

## Using the project today

The [PyPI package](https://pypi.org/project/uc2/) includes the native CLI and
Python API. Releases also provide standalone executables, a Midnight
Commander helper, and Total Commander plugins. A Linux FUSE mount exposes
newest files and separate version views. The libarchive reader is available
as a build recipe and a prepared upstream proposal; it has not been accepted
upstream.

[Collection benchmarks](../benchmarks/2026-11-05-compression.md) record archive
size, time, memory, and verified restored bytes. The alpha5 compressor improved
UC2's results on the tested source collections, while 7z still produced smaller
archives. Those measurements guide further compression work.

The next work is listed in the [roadmap](../../ROADMAP.md). A browser reader
and a migration toolkit would make old archives easier to inspect and export
without requiring users to adopt UC2 for new backups.

Updated November 5, 2026.
