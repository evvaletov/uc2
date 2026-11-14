# Shared chunks in named archives

UC2 3.0.0a11 adds optional payload sharing between partly changed files and
stored versions. Each original file remains one named entry. The writer splits
it at content-defined boundaries and stores each distinct chunk once within
the archive, after comparing both SHA-256 and the original bytes.

```sh
uc2 --chunked -w versions.uc2 old/ new/
tar cf - project/ | uc2 --chunked --ingest-tar backup.uc2
producer | uc2 --chunked --ingest stream.uc2
uc2 --ingest-restore stream.uc2 > restored-stream
```

Opaque ingestion creates one entry named `data`; its bytes are the complete
original stream. The existing opaque v1/v2/v3-A restore paths remain available.
Tar ingestion preserves named entries, with the metadata limitations in the
[tar guide](tar-ingestion.md). Ordinary list, print and extract commands work
for both modes. The default writer continues to use shared dictionaries and
whole-file payload reuse in the existing format.

```python
from uc2 import UC2Writer

with UC2Writer("versions.uc2", chunked=True, level=4) as archive:
    archive.write("old.bin", "data.bin")
    archive.write("new.bin", "data.bin")
print(archive.stats)  # includes unique_chunks, chunk_references, shared_bytes
```

Use the full a11 or newer reader, browser build or WCX plugin for these archives.
Original DOS UC2, earlier revival versions and the optional LGPL `libuc2read`
dependency reject them. The proposed upstream libarchive reader uses that
legacy dependency and therefore cannot read this extension. UC2's separate
full GPL libarchive adapter can. Existing archives remain readable.

Chunks target 8 KiB, with a 2 KiB minimum except at EOF and a 32 KiB maximum.
Each unique chunk is independently compressed at the requested level; a chunk
that does not shrink is stored raw. Small or dissimilar files can grow because
each file needs an 88-byte recipe plus 48 bytes per chunk reference. There is no
cross-archive reuse. Whole-file duplicates also share the recipe.

The writer caps the entire collection at 1,048,576 chunk references, counting
duplicates, in addition to existing input, output, entry and directory limits. Decoded central-directory
metadata is capped at 64 MiB, independent of its compression ratio.
Its retained chunk index uses at most 64 MiB plus 128 KiB of payload buffers;
array growth temporarily needs both old and new allocations. Existing file
metadata and compression workspaces are additional. Input and
output are spooled to temporary files. Reading checks one descriptor at a time
and decodes at most 32 KiB per chunk, with the codec workspace additional.
It does not retain a decoded-chunk cache.

`shared_bytes` counts logical source bytes served by an earlier chunk or whole
file; it is not the compressed space saving. A method-11 entry's `compress_size`
is its recipe size, excluding shared payloads. Adding entry sizes cannot recover
the archive's disk size. Use the actual file size or writer `archive_size`.

Each chunk, the ordered recipe and the complete decoded file have SHA-256
checks. The historical UC2 XOR checksum is checked too. These detect corruption;
they do not authenticate the author. Python/CLI extraction stages output before
publication. C extraction callbacks can receive earlier verified chunks before
a later corruption or output error, so callers must stage output when needed.

## Method 11 wire format

All integers are little endian. Archive `versionMadeBy` and
`versionNeededToExtract` are 300. The central directory uses the existing codec
and has one ordinary entry per source file, with method 11, master ID 1 and an
offset to the recipe. The entry's compressed-size field in the central directory
equals the recipe length. Chunked archives produced by this writer have no master records.

| Recipe header offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 8 | `UC2CHN1` followed by NUL |
| 8 | 4 | Chunk count |
| 12 | 4 | Decoded file size, matching the central directory |
| 16 | 4 | Record table length, exactly count × 48 |
| 20 | 4 | Flags, currently zero |
| 24 | 32 | SHA-256 of the complete decoded file |
| 56 | 32 | SHA-256 of header bytes 0–55 followed by the ordered record table |

| Record offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 4 | Absolute chunk payload offset |
| 4 | 4 | Stored byte length |
| 8 | 4 | Decoded length, 1–32,768 |
| 12 | 2 | Method: 0 raw, 1–9 Huffman without a master/delta, 10 rANS |
| 14 | 2 | Historical XOR checksum of decoded chunk |
| 16 | 32 | SHA-256 of decoded chunk |

All referenced payloads begin at offset 29 or later and end at or before their
recipe. Recipes must end before the central directory. References are direct,
never recursive. Raw stored and decoded sizes must match; compressed payloads
must have a positive even length of at most 65,536 bytes and consume exactly
their encoded word range. The sum of decoded lengths must equal the file size.
An empty file has zero records and SHA-256 of the empty byte string. Readers
validate the whole table before delivering any payload and check the full-file
hash after delivering the chunks.
