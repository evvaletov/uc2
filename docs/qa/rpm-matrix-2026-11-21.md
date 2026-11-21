# RPM target expansion, 21 November 2026

RPM revision 2 covers Fedora 44/45 and AlmaLinux 9/10 on x86_64 and ARM64.
The [native build matrix](https://github.com/evvaletov/uc2/actions/runs/44935426121)
passed all eight RPM jobs at commit `14d6186`. The tested AlmaLinux container
versions are 9.9 and 10.3. The EL10 x86_64 build uses `x86-64-v3` compiler
flags. Other Enterprise Linux distributions were not tested.

Each target builds as an unprivileged user from the pinned alpha13 archive.
Its native suite registers 36 cases: 35 pass and the optional OpenTimestamps
cross-check skips. DOSBox is excluded. The alpha12 upgrade fixture registers
35 cases: 34 pass and the same optional check skips. The spec rejects an
unexpected test count or skip. AlmaLinux 9 installs Python 3.11 for tests;
the shipped MC helper retains the system Python 3 runtime.

Fresh installation, alpha12 upgrade, removal and reinstallation pass on every
target. Each installed package passes `rpm -V`, all six fixture-file comparisons,
MC list and copyout checks, and classic L4 and chunked L9 round trips. Package
removal preserves the input file and the SHA-256 digests of user-created archives.
The EL9 checks also confirm that Python 3.11 did not become a runtime dependency.

A separate Fedora 45 x86_64 check installed the previously published revision 1
RPM, created an archive, upgraded to revision 2, verified package files and
extracted identical bytes. The archive digest survived the upgrade and removal.
The run completed with `PUBLISHED_REVISION_UPGRADE_PASS`.

All eight source RPMs contain the reviewed spec and the identical pinned source
archive, verified after extraction. The four published source RPMs are from the
x86_64 jobs; the ARM64 jobs produce equivalent payloads with different package
metadata and whole-file hashes. The comparison completed with
`ALL_SOURCE_RPM_CONTENTS_MATCH_REVIEWED_SPEC_AND_SOURCE`.

The checksum command was tested with valid, corrupt, missing and duplicate
entries, plus a stale manifest that still matches an older package. Only the
valid selected RPM reaches installation. Separate negative checks confirm that
a reduced 28-test inventory and an unexpected skipped test are rejected.

The source tag and CLI version remain alpha13. Twelve RPM downloads are added:
eight binaries and one source RPM per distribution. Existing revision 1 downloads
are retained, and the checksum manifest contains both revisions. These packages
are unsigned standalone downloads. Copr remains deferred at the owner's request;
signed repository delivery can be revisited separately.

See the [installation guide](../distribution.md#fedora-rpm),
[validation record](rpm-matrix-2026-11-21/validation.json),
[review record](rpm-matrix-2026-11-21/reviews.json) and
[publication receipt](rpm-matrix-2026-11-21/publication.json).
