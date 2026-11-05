Roadmap
=======

UC2 3.0.0a4 provides a Python reader, twenty platform wheels, and four
standalone CLI downloads. The next priorities are:

1. A browser-only reader and inspector, with archive bytes kept on the device.
2. A preservation and migration toolkit with version-aware inventories and
   verified ZIP/tar exports.
3. Compression improvements guided by measured gaps against 7z and tar+zstd.
4. Packaged file-manager integrations and a compatible libarchive contribution.
5. A read-only archive filesystem, starting with a Linux FUSE prototype.

The project-wide task-qa + task-at audit and the unused standalone BLAKE3
module's official-vector failures remain open. The Python release received
scoped tests and review; it does not establish correctness of every library
module or support for every historical UC2 method.

See `ROADMAP.md on GitHub
<https://github.com/evvaletov/uc2/blob/main/ROADMAP.md>`_ for the full inventory,
completed work, benchmark evidence, and tracking references.
