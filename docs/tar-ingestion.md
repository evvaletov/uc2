# Tar-entry ingestion

```sh
tar cf - project/ | uc2 --ingest-tar backup.uc2
uc2 -l backup.uc2
uc2 -p backup.uc2 project/README.txt
```

`--ingest-tar` reads stdin sequentially and creates separate UC2 file and directory
entries. It verifies tar checksums and framing before publishing the archive.
A malformed or unsupported member leaves an existing output archive unchanged.
Use ordinary list, print and extract commands to recover files. Existing
`--ingest` and `--ingest-restore` still preserve an opaque stream exactly.

Supported input includes regular files and directories in V7/USTAR, GNU long
names, and PAX `path`, `size` and `mtime` records. Links, devices, FIFOs, sparse
extensions, xattrs and unknown PAX keys are refused. Absolute paths, `..`, empty
components, control characters, colons and backslashes are refused. Names must
be representable in CP850, as required by the UC2 writer. The parser caps paths
at 4,095 bytes and 64 components, extended metadata at 1 MiB and 128 records
between members, and directories at 100,000. The writer also limits combined
entries to 100,000 and logical input/output to its 2^32−1-byte format boundary.

Tar ownership and Unix permission bits are not fully representable in UC2.
Write permission maps to the DOS read-only bit; other Unix permissions, uid/gid,
ownership names, atime and ctime are discarded. Mtimes use the host's local
zone and DOS two-second precision. Unrepresentable `time_t` or DOS dates become
zero; negative PAX dates are refused. Directory metadata is recorded at its
first appearance, including an implicit parent created for a child. Emit explicit
directory records before children when their DOS metadata matters. Duplicate
file names create stored versions.

Identical files share one compressed payload after SHA-256 and exact-byte
comparison. The writer also uses content-defined chunks to form shared master
dictionaries. Add `--chunked` to share independently stored payload chunks across partially
changed files. This opt-in format requires the full a11 or newer reader; see
[chunk sharing](chunk-sharing.md) for compatibility, limits and examples.
