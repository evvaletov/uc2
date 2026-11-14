UC2: UltraCompressor II
=======================

A cross-platform revival of UltraCompressor II, the DOS-era archiver by
Nico de Vries (1992-1996). UC2 used shared compression dictionaries
("master blocks") and file versioning.

UC2 v3 is a portable C99 tool with both
decompression and compression, targeting Linux, macOS, Windows, and DOS.

`Open an archive in your browser <reader/>`_ to inspect stored versions,
verify checksums and download selected file versions as a ZIP. Save the reader
for offline use while connected. Archive data stays on your device.
For collections, ``uc2-preserve`` inventories and migrates UC2 archives to
verified ZIP/tar exports. See the `preservation guide
<https://github.com/evvaletov/uc2/blob/main/docs/preservation.md>`_.

``uc2 snapshot`` in the Python package creates incremental directory versions
with shared chunks, verified restores, explicit retention and replica repair. See the `snapshot guide
<https://github.com/evvaletov/uc2/blob/main/docs/snapshots.md>`_.

The optional ``--chunked`` writer shares payload chunks across named files and
stored versions; see the `chunk-sharing guide
<https://github.com/evvaletov/uc2/blob/main/docs/chunk-sharing.md>`_ for compatibility.

The native CLI also supports tar ingestion with separate entries and data-only
SAS/UC2SEA reads. See the `tar guide
<https://github.com/evvaletov/uc2/blob/main/docs/tar-ingestion.md>`_ and
`volume guide <https://github.com/evvaletov/uc2/blob/main/docs/volumes.md>`_.

.. toctree::
   :maxdepth: 2
   :caption: Contents

   quickstart
   usage
   library
   format
   building
   history
   roadmap
