# Incremental directory snapshots

Install the Python package (`python -m pip install --pre uc2`) to use `uc2 snapshot` or the
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
The Python API is `uc2.snapshot.create`, `snapshots`, `verify`, `restore`,
`prune`, `repair` and `compact`.
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
one operation may run at a time; a second operation is refused. A killed process may
leave `.writer-lock`; remove that file only after confirming the writer has
stopped. An interrupted create can leave unreferenced chunks, which are safe
to keep. Pruning is explicit; see the maintenance commands below.

Default limits are 100,000 entries, 500,000 chunk references, a 64 MiB manifest
and 1 TiB of logical data. The chunk-reference limit can be reached much
sooner than the byte limit: at most about 16 GiB of chunk references fit, and
random data normally reaches the reference limit around 4–5 GiB. The byte
limit can be lowered with `--max-total-size`; the other v1 limits are fixed.
Each decoded chunk is at most 32 KiB, its stored archive at most 1 MiB, and its
decoder allocation budget is 8 MiB. Metadata memory scales within the manifest,
entry and reference caps. This initial format targets modest collections.

`verify` detects missing or corrupt manifests/chunks. Keep a second copy of the
repository and use `repair` from that copy, then verify again. SHA-256 checks detect corruption; they do not authenticate an untrusted
repository. Repair is explicit, and there is no redundancy or automatic repair.
Keep original archives as well when preserving historical collections.

The [measured two-version comparison](benchmarks/2026-11-07-snapshots.md)
includes shared chunks, tar-ingestion costs and recovery after simulated damage.

## Retention, repair and packed storage (alpha10)

```sh
uc2 snapshot prune ./backup-repository --keep-last 5
uc2 snapshot prune ./backup-repository --keep-last 5 --keep SNAPSHOT_SHA256 --apply
uc2 snapshot repair ./backup-repository ./intact-replica
uc2 snapshot compact ./backup-repository
```

`prune` first verifies every committed snapshot, then reports the snapshots and
unreachable chunks it would remove. Only `--apply` deletes anything. `--keep`
may be repeated; explicitly retained IDs are kept in addition to `--keep-last`.
At least one committed snapshot must remain. Retention sorts by UTC creation
time, breaking ties by the manifest SHA-256 address. Equal timestamps do not
encode insertion order; pin an important snapshot by ID. A corrupt committed
snapshot blocks pruning. Unreferenced corrupt chunks can be removed by pruning
with every desired snapshot retained. There is no background collection.

`repair` verifies the selected donor snapshots completely before changing the
repository. It replaces missing or corrupt chunks and manifests, preserving
history found only in the destination. Repeat `--snapshot ID` to select donor
versions; otherwise all donor versions are used. It does not invent data absent
from the replica. Interrupted v1 repair may have completed some replacements;
rerunning it is safe. Packed repair is a transaction. Physical SQLite damage,
a missing catalog, invalid catalog keys or schema damage require a whole
repository backup or reconstruction in a fresh repository from an intact copy;
object repair is not a general SQLite salvage tool.

`compact` explicitly converts v1 to v2: `objects.sqlite3` stores the same UC2
chunk archives and gzip-compressed complete manifests. Keep it with `config.json`
and the `chunks/` and `snapshots/` directory skeletons. The database is verified
and flushed before the atomic config switch; loose objects are deleted afterward.
Old readers reject v2. Migration preserves even unreferenced chunks and refuses
corrupt ones; run explicit retention first to discard unwanted orphans. A failed
cleanup can be finished by running `compact` again. In v2, `prune` frees database
pages for reuse and `compact` reclaims space with SQLite VACUUM. Allow temporary
disk space for migration and VACUUM; retain a separate backup.

All operations, including listing, verification and restore, now require a
writable repository and take the same exclusive `.writer-lock`. Stop older
clients before maintenance. Remove a leftover lock only after confirming that
no operation is running. An interrupted migration can leave loose backups or
hidden temporary files; resume `compact`, or inspect abandoned temporary files
with all clients stopped. Snapshot commands refuse to proceed while
`.writer-lock` exists.

The per-operation decoded-chunk cache is limited to 8 MiB. Existing per-snapshot
entry, reference and manifest limits still apply; maintenance also caps all inventoried chunks (including orphans) at two million
and repository history at 100,000 manifests. Repositories beyond those limits
require splitting or a later storage format before maintenance. These limits are checked independently of
the optional logical-byte ceiling. Packing reduces filesystem overhead and
manifest storage; it does not make the repository unbounded. See the
[1,045-file measurement](benchmarks/2026-11-07-snapshot-maintenance.md).

## Recovering an interrupted operation

Stop all clients before inspecting an interrupted repository. Read the PID in
`.writer-lock` and confirm that the owning operation has stopped on the machine
that ran it. A PID can be reused, and a repository may have moved between
machines; a failed local PID lookup alone does not establish ownership. Remove
`.writer-lock` only after confirming that its owning operation has stopped.
There is no automatic lock
expiry or lock-stealing command.

Keep `config.json` and every SQLite file, including a rollback journal, together.
Run `uc2 snapshot list REPOSITORY` so SQLite can recover an interrupted
transaction and you can inspect the committed history. Do not delete a journal or change the format marker to
try to repair a failed migration. `config.json` selects the authoritative store:
before the migration switch it is the loose objects; afterward it is the packed
database. Once verification succeeds, rerun `compact` to finish cleanup.

After a failed create, list the history and verify the intended committed IDs.
A loose repository can contain orphan chunks; an explicit `prune --apply` can
remove them after the retained history verifies. Packed create, prune and repair
commit their changes as transactions. Rerun the intended operation after fixing
the failure and checking the history. For a full filesystem, make space for the
operation's temporary files as well as its final output.

A killed restore can leave a `.uc2-restore-*` staging directory. On POSIX it can
also leave an empty destination reserved immediately before publication. Inspect
these paths with the process stopped; preserve them if you cannot confirm that
the stopped restore created them. Restore again to a different, new destination, or remove only a
confirmed abandoned empty reservation before retrying. A completed destination
can already exist when a final directory sync reports an error; check it before
retrying. Keep the repository and original source until the restored data has
been checked.

The [alpha12 scale and recovery record](benchmarks/2026-11-14-scale-recovery.md)
reports a multi-GiB collection, actual default-limit boundaries, process-kill
checkpoints and disk-full tests. Process interruption tests do not establish
durability under power loss or storage-controller failure.
