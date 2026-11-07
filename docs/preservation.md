# Inventory and migrate UC2 collections

Install the alpha package, then inventory a directory:

```sh
python -m pip install --pre uc2
uc2-preserve old-archives -o inventory
```

Directories are searched recursively for `.uc2` files, ignoring case. You can
also supply individual archives. The output directory must be new. Source
archives are opened read-only and kept in place.

To migrate a collection:

```sh
uc2-preserve old-archives -o migrated-zip --format zip
uc2-preserve old-archives -o migrated-tar --format tar
```

The tool checks UC2 checksums while decoding and computes SHA-256 for each
stored file version. Each generated ZIP/tar is reopened and its files are
hashed again before the batch output is published. Failed files are omitted
from exports and recorded in the manifest. A bad source archive does not stop
the other archives in a batch.
Destination write failures abort the batch without publishing it. Use an output
parent directory under your control; concurrent changes to its contents are
outside the supported workflow.

`inventory.json` lists each source and its result. Each source has its own
directory containing `manifest.json` and, when requested, `content.zip` or
`content.tar`. The manifest records:

- Source archive path, size and SHA-256.
- Every original entry path, including directories and older versions.
- Original DOS attributes and the raw DOS timestamp, plus a parsed local date
  when valid. DOS timestamps do not identify a timezone.
- Compression method, compressed and decoded sizes, verification result,
  decoded SHA-256, and the exported member path.
- Archive-level errors and metadata conversion limits.

Version zero is newest. Exports use `files/000001/entry-filename` paths. The
number identifies the entry within the source archive. This avoids overwriting
versions or confusing case variants, reserved names and unsafe original paths.
Use the manifest to map exported files back to their original names.

ZIP stores valid local DOS timestamps and supported DOS attributes. Tar uses
ordinary file permissions, `mtime=0`, and UC2 metadata in PAX headers; choosing
an epoch timestamp would require an unknown timezone. Directory records remain
in the manifest. Master blocks, compressed streams and arbitrary UC2 tags are
not reproduced in ZIP/tar. Keep the originals for those details.

The exit status is 0 when all archives succeed, 1 for a published partial
result, and 2 for an invocation or output error. Check the manifest before
treating a collection as fully migrated. A SHA-256 hash identifies decoded
bytes; it does not establish the archive's author or authenticity.

Default limits per source archive are 1 GiB of input, 100,000 entries, 64 MiB
of decoder allocation, 1 GiB per decoded file, and 4 GiB across all versions.
Entries exceeding a file or remaining total limit are recorded and skipped;
smaller entries can still be processed. Failed decode attempts count toward
the total budget at their declared size.
The corresponding `--max-archive-size`, `--max-entries`,
`--max-decoder-memory`, `--max-extract-size` and `--max-total-size` options
accept byte counts except for the entry count. Payloads spool to temporary
disk files. Allow space for exports, one decoded file, and the input archive
already on disk. Metadata remains proportional to the entry count.

Python callers can use the same implementation:

```python
from uc2.preserve import preserve

report = preserve(["old-archives"], "migrated", format="zip")
assert report["status"] == "ok"
```

The original-encoder compatibility corpus and its known gaps are documented in
[`tests/archives/README.md`](../tests/archives/README.md).
