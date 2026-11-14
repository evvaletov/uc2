# Collection scale and recovery, 14 November 2026

These Linux measurements cover CLI metadata, snapshot limits and recovery. It uses
synthetic inputs and records each workload separately. It does not measure
power-loss recovery or establish performance on other machines.

## CLI metadata

Both binaries were built with CMake Release on the same Intel Core i9-9900KF
host (16 logical CPUs, XFS). The baseline is
`97e355c5f3be0e53627b9bd611c58559c7125700`. Fixtures were created with
`UC2Writer(level=2)` and contain empty files. Each timing is one invocation of
`uc2 -qalT`, including reading the archive and collecting its complete listing.
Other local workloads were running, so these are observations rather than
isolated microbenchmarks.

| Layout | Files | Listed entries | Baseline seconds | Alpha12 seconds |
| --- | ---: | ---: | ---: | ---: |
| Flat names | 10,000 | 10,000 | 1.998 | 0.042 |
| Flat names | 50,000 | 50,000 | >120 | 0.314 |
| Flat names | 100,000 | 100,000 | >120 | 0.500 |
| One directory per file | 10,000 | 20,000 | 3.385 | 0.114 |
| One directory per file | 50,000 | 100,000 | >120 | 0.813 |
| One directory per file | 100,000 | 200,000 | >120 | 2.019 |
| Versions of one file | 10,000 | 10,000 | 1.050 | 0.039 |
| Versions of one file | 50,000 | 50,000 | >120 | 0.192 |
| Versions of one file | 100,000 | 100,000 | >120 | 0.386 |

The three baseline cases that completed have byte-identical listing hashes in
alpha12. Larger baseline cases were stopped at 120 seconds. The regression test
also compares an independent list-based model on mixed names, directory IDs
and versions. It checks latest-version and explicit-version selection, missing
parents, CP850 names and a 100,000-file listing. These changes cover metadata
ingestion; file decoding, pattern matching and recursive traversal have their
own costs.

Evidence: [timings and hashes](2026-11-14-scale-recovery/cli-metadata.json).
Reproduce with the installed Python package and two CLI builds:

```sh
python tests/scripts/benchmark_cli_metadata.py --uc2 /path/to/baseline/uc2 \
  --work /tmp/uc2-cli-scale --label baseline
python tests/scripts/benchmark_cli_metadata.py --uc2 /path/to/alpha12/uc2 \
  --work /tmp/uc2-cli-scale --label alpha12
```

## Snapshot workload

The large corpus contains 1 GiB of seeded random data, a 2.25 GiB file repeating
a seeded 256 KiB block, one small text file and an empty directory. Its logical
size is 3.25 GiB; its unique data is approximately 1 GiB. The larger file crosses
the 2 GiB boundary. This workload stays within the existing reference cap.

The workflow creates, verifies and restores a loose v1 repository, packs it to
v2, then edits 1 MiB of the random file, appends to the larger file, renames the
text file and adds another. It creates and restores the second version, damages
one stored chunk, repairs it from an intact packed replica, prunes old history
and vacuums the database. Independent inventories check restored bytes, paths,
sizes, modification times and ordinary permission bits.

The complete snapshot run used starfield01: AMD Ryzen 9 7950X, 32 logical CPUs,
124 GiB RAM, XFS and CPython 3.12.13. An earlier run on the busy CLI-test host
was stopped during creation and is excluded from the completed results.
The same alpha12 wheel passed a complete smoke run on starfield01 before
the large run. Each timed API call uses a fresh Linux process with a 2 GiB
address-space limit. Resident-memory measurements use `/proc/self/status`'s `VmHWM`:
`getrusage().ru_maxrss` retained the launcher's pre-exec high-water mark here.
API timings include their own validation. Generation and independent file
hashing are outside those timings. The memory figures cover operation workers;
the corpus generator and independent inventory process are separate.
Repositories, source and restores are kept
for inspection by the opt-in harness:

```sh
python tests/scripts/benchmark_snapshot_scale.py --work /tmp/uc2-snapshot-scale
```


The three independent restore inventories matched file bytes and sizes, all
paths and types, root/directory/file modification times and ordinary modes.
Both committed versions rejected the deliberately truncated chunk; repair
reported one replacement. The edited version added 96 chunks and 5,459,340
bytes of chunk/manifest data. This is logical object growth, not the physical
SQLite file-growth figure. The final packed database occupies 1,213,132,800
bytes. See [the full result](2026-11-14-scale-recovery/snapshot-scale.json).

| Phase | Seconds | Peak resident MiB |
| --- | ---: | ---: |
| create loose v1 | 322.81 | 151.3 |
| verify loose v1 | 31.44 | 157.4 |
| restore loose v1 | 35.71 | 157.3 |
| compact v1 to v2 | 70.30 | 157.4 |
| create edited packed v2 | 33.90 | 158.1 |
| restore edited packed v2 | 31.13 | 159.3 |
| repair from packed replica | 168.08 | 302.0 |
| verify repaired latest | 28.30 | 160.3 |
| prune old history | 59.41 | 246.8 |
| vacuum packed repository | 34.59 | 159.0 |
| restore after repair prune and vacuum | 31.45 | 158.6 |
| list retained history | 0.23 | 158.3 |

Loose repository files occupied 1,143,712,582 logical bytes before packing; the
first packed database was 1,214,087,168 bytes. Packing reduced the file count,
but SQLite page overhead outweighed the saved manifest bytes in this corpus.
Allocated file bytes on XFS fell from 1,346,727,936 to 1,214,091,264; those
figures exclude directory and filesystem metadata.
Migration needs room for the new database alongside the loose store.

A two-second sampler observed about 4 GiB of allocated temporary-file space
during restore and compaction/VACUUM, including XFS preallocation. These are
sampled lower bounds, excluding filesystem metadata and the source, committed
store and already restored copies. They are not upper space budgets.
[Sampler method and peaks](2026-11-14-scale-recovery/sampled-temporary-space.json).

## Default limits and interruption

The limit harness uses the actual constants. A file made of 500,000 repeated
4,023-byte chunks (2,011,500,000 bytes) is accepted and verified. Appending one
chunk is rejected without changing committed history. Reference density can
therefore exhaust the cap below 2 GiB; the 1 TiB byte ceiling does not imply
support for a collection of that size.

The writer also accepts and verifies 100,000 actual empty files, then rejects
the 100,001st without changing history. A separately constructed valid manifest
with a descriptive padding field is accepted at exactly 64 MiB and rejected
with one extra byte, in both loose v1 and gzip-packed v2 storage. These cases
test the reader's byte boundary; they are not
a claim that the writer generated those padded manifests. The constants were not
lowered for any of these checks. [Limit results](2026-11-14-scale-recovery/limits.json).

Process-kill tests stop a child at a named durability boundary, kill it with
SIGKILL, wait for termination and remove only sentinels containing that child's
PID. The covered boundaries are loose/packed creation, both sides of the
migration config switch, loose/packed pruning, packed repair, and both sides of
restore publication. The [recovery guide](../snapshots.md#recovering-an-interrupted-operation)
describes the manual steps and remaining files.

Disk-full tests run inside a dedicated 32 MiB tmpfs in a private mount namespace.
They reserve the remaining space, exercise the operation, check actual ENOSPC
or SQLite SQLITE_FULL, release the reservation and verify recovery. They cover
loose/packed creation, migration, restore and packed repair. They do not fill
the host's normal filesystem.

```sh
python tests/scripts/check_snapshot_limits.py --work /tmp/uc2-snapshot-limits
python tests/scripts/check_snapshot_recovery.py --work /tmp/uc2-snapshot-kills
mkdir /tmp/uc2-private-disk
unshare -Urnm sh -c 'mount -t tmpfs -o size=32m tmpfs "$1" &&
  python tests/scripts/check_snapshot_recovery.py --disk-full --work "$1"' \
  sh /tmp/uc2-private-disk
```

Use a disposable, private mount for the last command. The harness checks the
mount and capacity before allocating space. These checks establish recovery
for the executed process and space failures; hardware power loss, filesystem
damage and general SQLite salvage require separate evidence and backups.

All nine named SIGKILL checkpoints and all five disk-full cases recovered.
The tests check committed snapshot IDs as well as decoded contents. Receipts:
[process kills](2026-11-14-scale-recovery/process-kill.json),
[disk full](2026-11-14-scale-recovery/disk-full.json).
