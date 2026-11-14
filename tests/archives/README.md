# Original-encoder compatibility corpus

These five archives were made with UC2 Pro 2.3 in DOSBox-X from the payloads
in `tests/corpus/`. They exercise the actual original encoder. They were
generated for this project, not collected in the 1990s.

`manifest.json` pins every archive, every decoded payload, and the original
encoder distribution by SHA-256. The payloads and fixtures are distributed
under the repository's GPL-3.0-or-later license. The original DOS executable
is not part of the browser bundle or Python source distribution.

The corpus covers text, binary data, an empty file, repeated zeros, and a
multi-file archive with master blocks. Python migration tests decode these
archives, compare each file hash to its known payload, then reopen both ZIP
and tar exports and compare their contents. Browser tests use the same
archives and compare downloaded files with the native reader.

Additional Turbo/method 80, SAS and UC2SEA fixtures are described below.
`tests/scripts/create_archives.sh` documents the
original fixture-generation procedure; regeneration changes archive timestamps
and requires reviewing and updating the pinned hashes.

## Historical collections

`historical.json` includes three independently sourced archives from the
[Sembiance format samples](https://sembiance.com/fileFormatSamples/archive/ultraCompressor2/):
BLKTXT.UC2, MKMSG.UC2 and UC2INFO.UC2. Their 88 entries carry DOS timestamps
from 1993-1995. Both ZIP and tar migrations passed on November 7, 2026.
All decoded file sizes and SHA-256 hashes also match the independent
[Go decoder](https://github.com/klauspost/uc2) at commit
`53622f89ada1cf0d4a69f91b653bdd44ec5b0f92`, with ASCII filename case normalized.

The repository distributes URLs, hashes and expected metadata for these
archives. Their third-party payloads are downloaded separately because their
redistribution permissions have not been established. Nothing from these
archives is executed. The Go project's pinned sample list supplied the URLs;
the JSON records exact provenance and hashes.

Run the optional network check with an installed UC2 package:

```sh
python tests/scripts/check_historical.py --cache /tmp/uc2-historical
```

The five bundled original-encoder archives form the basic offline CI corpus.

Turbo fixtures `turbo0.uc2` and `turbo1.uc2` are generated format fixtures,
with SuperMaster and NoMaster respectively. Their original-DOS and independent
Go output checks are pinned in `turbo.json`. They are not historical captures
or original-encoder output. `test_turbo` covers 18 length/master combinations
and malformed block/literal/terminator/output-bound cases.

The optional `historical.json` corpus now covers 39 archives and 684 entries:
13 historical collections, four unarc-rs test archives, and 22 original-DOS
encoder oracle archives from the pinned Go repository (revisions 2, 2.3 and
2.37b; long names, revisions, delta methods and damage protection). All file
sizes and SHA-256 hashes agree with that independent decoder. External bytes
are fetched separately; only provenance and expected metadata/hashes ship here.

`sas/` contains three original SAS 2.3 split parts of the
generated Turbo fixture. The original `SAS B SPLIT` tool reassembled them
byte for byte. Their manifest records hashes and provenance. Native reads
reassemble these parts directly in alpha10. The `sea/` fixtures preserve original
UC2SEA compressed records under a synthetic envelope without the restricted
executable stub; their manifest records payload and part hashes.
