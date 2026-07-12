#!/bin/sh
# Generate the SDF bboard <REQUESTS> post requesting a system-wide uc2
# install, filling in the current release tag and test-suite size from the
# repository so the announcement never drifts out of sync with the code.
#
# Usage:
#   scripts/gen-bboard-post.sh [-v VERSION] [-t TESTCOUNT] > bboard-post.draft.txt
#
# VERSION   defaults to the latest git tag (git describe --tags --abbrev=0).
# TESTCOUNT defaults to the number of registered ctest cases in the first
#           configured build directory (ctest -N).
set -eu

repo_root=$(cd "$(dirname "$0")/.." && pwd)
version=""
tests=""

while [ $# -gt 0 ]; do
	case "$1" in
	-v) version=$2; shift 2 ;;
	-t) tests=$2; shift 2 ;;
	-h|--help)
		sed -n '2,12p' "$0"; exit 0 ;;
	*) echo "usage: $0 [-v VERSION] [-t TESTCOUNT]" >&2; exit 2 ;;
	esac
done

if [ -z "$version" ]; then
	version=$(git -C "$repo_root" describe --tags --abbrev=0 2>/dev/null || echo "v3.0.0")
fi

if [ -z "$tests" ]; then
	for bd in build-release build build-linux build-asan cmake-build-release; do
		if [ -f "$repo_root/$bd/CTestTestfile.cmake" ]; then
			tests=$(cd "$repo_root/$bd" && ctest -N 2>/dev/null \
				| sed -n 's/^Total Tests: *//p')
			[ -n "$tests" ] && break
		fi
	done
	[ -n "$tests" ] || tests="?"
fi

cat <<EOF
DRAFT -- not posted. Target: bboard <REQUESTS> on sdf.org.
Post with: bboard -> post to REQUESTS. Keep lines under 72 cols.
Regenerate with: scripts/gen-bboard-post.sh > bboard-post.draft.txt
----------------------------------------------------------------------
Subject: uc2 - UltraCompressor II archiver for system-wide install

Hello,

I would like to request a system-wide install of uc2, a C99 revival
of UltraCompressor II, the MS-DOS archiver by Nico de Vries (AIP-NL,
1992-1996). It reads and writes the original UC2 format, so .UC2
archives from the DOS era open fine on SDF, and archives made here
at the compatible levels extract in period-correct DOS software.

Why it might suit SDF: it is a small UNIX tool with no runtime
dependencies beyond libc, it has a retro-computing pedigree, and SDF
members with old DOS archives currently have no way to open them here.

Technical summary:

  - single binary, links against libc only (no pkgsrc deps)
  - builds with the stock toolchain: cmake + gmake, C99
  - its ${tests}-test suite passes on Linux/macOS/NetBSD; verified on
    NetBSD 10.0 amd64 (rie).  The one DOSBox interop test runs on the
    Linux side against the original uc2pro.exe
  - man page included; "gmake -C build install" installs both
  - license: GPL-3.0-or-later

  Source:  https://git.sdf.org/evv/uc2  (mirror: github.com/evvaletov/uc2)
  Release: ${version}
  Build:   cmake -B build -DCMAKE_BUILD_TYPE=Release
           gmake -C build && gmake -C build test

I can provide a prebuilt NetBSD binary for inspection on request,
and I am happy to adjust the build to fit local conventions.

Thanks for considering it,

evv
EOF
