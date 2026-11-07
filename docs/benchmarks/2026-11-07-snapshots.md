# Two directory versions: snapshot store and tar ingestion

Measured locally on 2026-11-07 using the reproducible
`tests/scripts/benchmark_snapshots.py` script. This is one controlled run,
not a general performance ranking. The corpus contains two identical 1 MiB
random files, repeated text and forty small files. Version two inserts 4,369
bytes into one random file, renames one small file, deletes another and adds
one new file. Both directory versions restore to the expected file hashes.

| Measurement | First version | Second version |
|---|---:|---:|
| Logical bytes | 3,149,898 | 3,154,196 |
| New unique chunks | 131 | 2 |
| New compressed chunk bytes | 1,096,689 | 16,082 |
| Manifest bytes | 26,704 | 26,700 |
| Total snapshot growth | 1,123,393 | 42,782 |
| Separate tar ingest bytes | 1,089,347 | 1,104,851 |
| Snapshot create, seconds | 9.041 | 0.808 |
| Snapshot restore, seconds | 1.359 | 1.242 |
| Tar creation + ingest, seconds | 0.662 | 1.099 |
| Tar stream restore + member hashing, seconds | 0.123 | 0.174 |

Version two adds 3.87% of the bytes needed by
its separate tar ingest. The manifest is about 0.85% of logical
file bytes and accounts for most of that incremental growth. The store reads
and verifies existing chunks on reuse; it does not trust unchanged timestamps.
The first snapshot pays for separate archive creation, reopening and filesystem
publication for each chunk. This makes it slower than a single tar ingest.

Snapshot restore writes individual files and applies metadata. The tar timing
writes the reconstructed tar and hashes its members, but does not create a
second extracted directory. These times therefore describe different amounts
of filesystem work and should not be treated as a direct speed ratio.

Overwriting a shared chunk with invalid bytes caused verification to fail.
Replacing it with the exact saved copy restored successful verification of the
first version. This demonstrates detection and recovery from a second copy,
not automatic repair. A single corrupt shared chunk can affect several files
or snapshots. No data was pruned in this experiment.

[Raw measurements](2026-11-07-snapshots.json) include the snapshot IDs and
script inputs. Re-run with an installed candidate package and a matching CLI:

```sh
python tests/scripts/benchmark_snapshots.py --uc2 build/cli/uc2 --output results.json
```
