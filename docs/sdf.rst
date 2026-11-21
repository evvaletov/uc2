UC2 on SDF: recover a DOS archive
=================================

UC2 3.0.0a13 was built and tested on SDF's otaku host on 21 November 2026,
using NetBSD 9.3 amd64, GCC 7.5.0, CMake 4.1.2 and Python 3.12.7.
The resulting CLI links to ``libc.so.12``. This is an alpha release; keep your
original archives and work on copies in a private directory.

Shared MetaARPA installation
----------------------------

The shared alpha13 CLI for NetBSD amd64 is installed at
``/sys/sdf/uc2-3.0.0a13/bin/uc2``. Its versioned directory includes the man
page, licence texts and exact release source archive. This follows the
REQUESTS charter's MetaARPA custom-software route. Use the full path; the
shared ``bin`` directory is unchanged::

   /sys/sdf/uc2-3.0.0a13/bin/uc2 -h

The binary SHA-256 is
``d5cdb68811a74924efbf725a9cc19ba8cb7769df70d3a421228e30ea6f8e7ee8``.
It was built and tested on otaku. For another operating system or architecture,
build from source using the instructions below. Tutorial wiki publication
awaits access from the membership team; the source for that page is in
``docs/outreach/sdf/wiki.txt``.

Build in local scratch space
----------------------------

SDF's make wrapper requires builds under ``/tmp``. A build in the NFS home
directory can print a refusal and still return success. Check that the binary
exists before testing or installing. Stay on the same shell host while using
its local scratch directory. Run these commands in ``sh``::

   set -eu
   umask 077
   uc2_work=$(mktemp -d /tmp/uc2-build.XXXXXX)
   cd "$uc2_work"
   git clone https://git.sdf.org/evv/uc2.git source
   cd source
   git checkout v3.0.0-alpha.13
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
     -DUC2_VERSION_LABEL=3.0.0a13 \
     -DPython3_EXECUTABLE=/usr/pkg/bin/python3.12
   nice -n 10 cmake --build build --parallel 2
   test -x build/cli/uc2

The release source is commit
``b297757b7509b3b46be9002979eff5e3e20e6ea8``. Two tests in that tag assume
that reading a directory always fails. NetBSD can return directory bytes.
Use the portable test scripts from the following pinned revision; this
changes two Python tests and leaves the compiled release source intact::

   uc2_test_revision=a76e66fa4b745395bbacaf3ed6810ccbdde5ad9a
   git checkout "$uc2_test_revision" -- \
     tests/scripts/check_cli_io.py tests/scripts/check_cli_staging.py &&
   ctest --test-dir build --output-on-failure --timeout 180 -j 1 \
     -E '^roundtrip_dosbox$'

On otaku, 35 tests passed and the optional OpenTimestamps cross-check was
skipped because its Python dependency was absent. DOSBox interoperability
was excluded on SDF; the release's Linux/DOS checks are recorded separately.
The benchmark's directory read-error check runs only where the OS rejects
such reads. The staging test uses a write-only descriptor to force an actual
read error on NetBSD too.

Recover an archive made by the original DOS encoder
---------------------------------------------------

The bundled ``basic.uc2`` fixture was made with UC2 Pro 2.3 in DOSBox-X from
project-owned payloads. Its SHA-256 is
``ea16565545abde72798e9cc9423c23149a050bfb4d769b6281f98593e3ee721b``.
The fixture's provenance and expected payload hashes are in
``tests/archives/manifest.json``. From the source directory::

   uc2_demo=$(mktemp -d /tmp/uc2-demo.XXXXXX)
   build/cli/uc2 -l tests/archives/basic.uc2
   build/cli/uc2 -t tests/archives/basic.uc2
   mkdir "$uc2_demo/restored"
   build/cli/uc2 -d "$uc2_demo/restored" tests/archives/basic.uc2
   for file in tests/corpus/*; do
     cmp "$file" "$uc2_demo/restored/${file##*/}" || exit 1
   done

All six files compare byte for byte, including the empty file and 65,536-byte
zero-filled file. ``-t`` reports ``Everything went OK``. This example uses a
fixture generated for this project; its DOS timestamps do not establish
historical provenance. The `recorded SDF demonstration
<https://github.com/evvaletov/uc2/blob/main/docs/qa/sdf-2026-11-21/demo.txt>`_
contains the actual output.

Create a classic-format archive and check its restored bytes::

   build/cli/uc2 -qw -L4 "$uc2_demo/new.uc2" tests/corpus/hello.txt
   build/cli/uc2 -t "$uc2_demo/new.uc2"
   build/cli/uc2 -p "$uc2_demo/new.uc2" > "$uc2_demo/new-hello.txt"
   cmp tests/corpus/hello.txt "$uc2_demo/new-hello.txt"

For original DOS UC2 compatibility, use levels 2-5 and leave ``--chunked``
unset. Levels 6-9 use the new rANS coding; chunked archives at every level,
including those selected by ``--chunked=auto``, need a revival reader version
3.0.0a11 or newer. This SDF example checks native restoration. The separate
``roundtrip_dosbox`` test checks both directions with the original DOS tool.

Preserve your own collection
----------------------------

Keep a separate copy of each original archive and all parts of a split set.
Record its source and a SHA-256 digest before extracting into a fresh private
directory. List and test the archive first; treat checksum failures as a
reason to investigate before replacing any earlier copy. UC2 checksums detect
corruption; establishing authenticity needs independent provenance.

Stored versions can share a pathname. Use the `collection preservation guide
<https://github.com/evvaletov/uc2/blob/main/docs/preservation.md>`_ when you need
an inventory and verified ZIP/tar exports of every stored version. That Python
workflow was tested separately; this SDF check covers the native CLI.

Private installation and reporting
----------------------------------

A private-prefix installation of the CLI and man page was tested. Choose an
unused prefix for your own installation::

   cmake --install build --prefix "$HOME/opt/uc2-3.0.0a13"
   "$HOME/opt/uc2-3.0.0a13/bin/uc2" -t tests/archives/basic.uc2

The source includes GPL-3.0-or-later and LGPL-3.0-only components; see
``docs/license-audit.md`` and the distributed licence texts. Keep those texts
and the corresponding source with any redistributed build.

Send compatibility reports through the `project issue tracker
<https://github.com/evvaletov/uc2/issues>`_ or the UC2 thread on SDF's ``RETRO``
board. Include the UC2 version, host OS, command and error. Share an archive
only if you have permission and have checked its contents for private data.
Software installation requests belong on ``REQUESTS``; member tutorial wiki
access is arranged through ``membership@sdf.org``. See SDF's
`tutorial contribution instructions <https://sdf.org/tutorials/>`_.
