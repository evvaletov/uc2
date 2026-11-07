# Incremental directory snapshots

Install the Python package (`pip install uc2`) to use `uc2 snapshot` or the
identical `uc2-snapshot` command:

```sh
uc2 snapshot create ./collection ./backup-repository --label first
uc2 snapshot list ./backup-repository
uc2 snapshot verify ./backup-repository SNAPSHOT_SHA256
uc2 snapshot restore ./backup-repository SNAPSHOT_SHA256 ./restored
```

Run `create` again after changing the collection. Each result includes its
snapshot ID, new chunk count, compressed chunk bytes, manifest bytes and total
growth. Pass the full ID printed by `create` or `list` to `verify` and `restore`.
The Python API is `uc2.snapshot.create`, `snapshots`, `verify` and `restore`.
Standalone C CLI downloads do not include Python snapshot commands.

A repository is a directory containing `config.json`, `snapshots/` and
`chunks/`. Keep all three together. Each snapshot has its own JSON manifest;
SHA-256 names protect manifests against accidental changes. File chunks use
the existing Gear chunker, with a 2 KiB minimum, 8 KiB target and 32 KiB maximum.
The shared store uses full SHA-256 addresses and one independently readable
UC2 archive per chunk. It supersedes the experimental raw FNV-addressed store
for this workflow; the two store layouts are not interchangeable.

Every source file is read, including unchanged files. An existing chunk is
reused only after decoding and comparing its contents. New chunks are also
reopened and checked. The manifest records relative Unicode paths, empty
directories, file sizes, full-file SHA-256, nanosecond modification times and
ordinary permission bits. Each manifest is a complete directory version, so
renames and deletions in later versions leave earlier snapshots restorable.

Restore requires a new destination. It validates paths and budgets, checks
each chunk and full-file hash, writes into a temporary sibling directory, and
publishes that directory only after completion. It does not replace an existing
destination. POSIX restores permission bits; Windows supports its normal
read-only interpretation. Timestamp precision depends on the destination
filesystem. Unsupported metadata operations abort rather than silently omit
metadata. Ownership, ACLs, extended attributes, hardlink identity, sparse-file
layout, symlinks and special files are not preserved. Symlinks and special
files cause creation to fail. Names that cannot be restored safely on Windows,
case collisions and Unicode-normalization collisions are refused.

Keep the source quiescent while creating a snapshot. File and directory stat
checks detect common changes, but this is not an atomic filesystem snapshot.
Source and repository must be separate trees. Repository and destination
parents must be controlled by the caller and stable during operations. Only
one writer may run at a time; a second writer is refused. A killed process may
leave `.writer-lock`; remove that file only after confirming the writer has
stopped. An interrupted create can leave unreferenced chunks, which are safe
to keep. There is no automatic pruning or garbage collection yet.

Default limits are 100,000 entries, 500,000 chunk references, a 64 MiB manifest
and 1 TiB of logical data. The chunk-reference limit can be reached much
sooner than the byte limit: at most about 16 GiB of chunk references fit, and
random data normally reaches the reference limit around 4–5 GiB. The byte
limit can be lowered with `--max-total-size`; the other v1 limits are fixed.
Each decoded chunk is at most 32 KiB, its stored archive at most 1 MiB, and its
decoder allocation budget is 8 MiB. Metadata memory scales within the manifest,
entry and reference caps. This initial format targets modest collections.

`verify` detects missing or corrupt manifests/chunks. Keep a second copy of the
repository: restore a damaged chunk or manifest from that copy, then verify
again. SHA-256 checks detect corruption; they do not authenticate an untrusted
repository. Repair is explicit, and there is no redundancy or automatic repair.
Keep original archives as well when preserving historical collections.

The [measured two-version comparison](benchmarks/2026-11-07-snapshots.md)
includes shared chunks, tar-ingestion costs and recovery after simulated damage.
