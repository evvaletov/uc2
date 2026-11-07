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

Turbo/method 80, multi-volume archives, and independently sourced historical
collections remain gaps. `tests/scripts/create_archives.sh` documents the
original fixture-generation procedure; regeneration changes archive timestamps
and requires reviewing and updating the pinned hashes.
