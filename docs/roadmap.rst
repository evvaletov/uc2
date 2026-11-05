Roadmap
=======

UC2 alpha6 adds C and Python writer APIs to the reader, native CLI, Linux mount,
and integration packages. The next priorities are:

1. A browser-only reader and inspector, keeping archive bytes on the device.
2. A preservation and migration toolkit with version-aware inventories and
   verified ZIP/tar exports.
3. Incremental snapshots with measured chunk reuse and verified restores.
4. Upstream adoption of the prepared optional libarchive reader.
5. Historical-format and desktop compatibility, starting with Turbo/method 80
   and Total Commander 32-bit extraction routing.

The local project audit repaired BLAKE3, LZ4, delta decoding, and stale prose.
The independent task-qa + task-at panel is still incomplete because reviewer
services were unavailable. These limits and the release checks are recorded in
``docs/releases/3.0.0a6-qa.md``.

See `ROADMAP.md on GitHub
<https://github.com/evvaletov/uc2/blob/main/ROADMAP.md>`_ for the inventory,
completed work, benchmark evidence, and tracking references.
