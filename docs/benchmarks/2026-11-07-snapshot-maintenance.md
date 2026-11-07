# Snapshot maintenance measurement — November 7, 2026

Corpus: 1,045 tracked regular files, 13,707,834 bytes, from public
[Midnight Commander](https://github.com/MidnightCommander/mc/tree/3228c23fd57db28c20c35db8e0e1c6d3b087b3c7).
Tracked symlinks were excluded. Measurements are one Linux run with concurrent
review work; timings are observations, not stable performance guarantees.

| Operation | Seconds | Result |
| --- | ---: | --- |
| Create loose v1 | 178.0 | 2,003 new chunks; 4,595,895 bytes of objects/manifest |
| Pack to v2 | 43.4 | 5,726,208-byte SQLite catalog |
| Verify packed version | 6.0 | 1,045 verified files |
| Snapshot unchanged tree | 24.8 | Zero new chunks; 118,714-byte manifest |
| Edit, rename, add file | 8.7 | Three new chunks, 2,521 bytes; 118,849-byte manifest |
| Restore changed version | 47.7 | All 1,046 relative paths and bytes match |
| Plan / apply retention | 10.9 / 8.8 | Keep newest; remove two manifests and one unreachable chunk |
| Reclaim packed space | 6.3 | 5,709,824-byte catalog |

Allocated file storage fell from 9,789,440 bytes across 2,005 files in v1 to
5,734,400 bytes across two files after migration. These figures exclude directory
inodes. Logical stored bytes grew from 4,596,007 to 5,726,327 because SQLite page
storage differs from tightly counted loose-file payload bytes; the filesystem
allocation reduction is the measured benefit. Prune reuses pages; VACUUM reclaims
them. Restore equality includes file contents and relative names; this benchmark
does not assert preservation of metadata beyond the snapshot contract.

This 13 MiB case exercises a larger file count than the first benchmark. It is
not evidence for multi-gigabyte capacity or performance. Fixed entry/reference
and manifest limits remain documented in [snapshots](../snapshots.md).
[Machine-readable measurements](2026-11-07-snapshot-maintenance.json).
