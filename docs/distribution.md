# Installing UC2 packages

UC2 is an alpha release. Keep original archives and try new builds on copies.
The package recipes pin their source archive and SHA-256 digest.

## Homebrew

The project tap provides the native CLI on macOS and Linux:

```sh
brew install evvaletov/tap/uc2
brew test evvaletov/tap/uc2
brew upgrade evvaletov/tap/uc2
brew uninstall evvaletov/tap/uc2
```

The formula builds from source with CMake. It installs the CLI, man page,
licences and optional Midnight Commander helper. The helper needs Python 3;
enabling it in Midnight Commander is described in `contrib/mc/README.md`.
The recipe is in `packaging/homebrew/uc2.rb`. This is a project tap; Homebrew
core inclusion has not been requested.

## Arch Linux

The source package recipe and generated metadata are in `packaging/aur/`.
AUR registration is temporarily closed, as reported on 21 November 2026.
The recipe is ready to build locally:

```sh
cd packaging/aur
makepkg -si
```

Run `makepkg` as an ordinary user. It downloads the pinned alpha13 source,
checks its digest, compiles it and tests classic and chunked archive round
trips. `pacman -R uc2` removes the installed package. User archives stay in
their chosen directories. The recipe targets x86_64 and declares Python as
an optional dependency for the Midnight Commander helper.

## Fedora RPM

The alpha13 release provides an x86_64 RPM built and tested on Fedora 45,
its source RPM, and `RPM-SHA256SUMS.txt`. Verify the downloads against that
checksum file before installation. The standalone RPM is unsigned; the
following command disables signature checking for this local package:

```sh
sha256sum -c --ignore-missing RPM-SHA256SUMS.txt && \
  sudo dnf --setopt=localpkg_gpgcheck=0 install ./uc2-3.0.0a13-1.fc45.x86_64.rpm
uc2 -h
```

To remove the package:

```sh
sudo dnf remove uc2
```

The package installs the CLI, man page, licences and Midnight Commander
helper. Python 3 is required for the helper. RPM version `3.0.0~alpha.13`
sorts before stable `3.0.0`; the CLI retains its upstream `3.0.0a13` label.
The native suite passed 35 tests, with the optional OpenTimestamps
cross-check skipped and DOSBox excluded. Installation, alpha12-to-alpha13
upgrade, package-file verification, archive round trips, removal and
reinstallation were tested in a Fedora 45 container.

The recipe and rebuild instructions are in `packaging/rpm/`. Copr publication
is pending. The release filenames use `3.0.0a13`; the RPM metadata retains
`3.0.0~alpha.13` for version ordering.
No inclusion in Fedora's official repositories is claimed.

## FreeDOS

Download `UC2.ZIP` from the alpha13 release and run:

```text
FDNPKG INSTALL UC2.ZIP
UC2 -h
FDNPKG REMOVE UC2
```

The optional package contains the DJGPP CLI, CWSDPMI, licence texts and
complete source archives. Its outer paths use DOS 8.3 names. FDNPKG maps
`PROGS/UC2` according to its configuration and creates a command launcher.
For manual installation, keep `UC2.EXE` and `CWSDPMI.EXE` together.

This DOS build is `3.0.0a13+dos1`. It adds a file-opening portability fix:
on the tested FreeDOS system, DJGPP assigns different inode numbers when
querying a pathname and its open descriptor. The DOS build checks that both
refer to regular files on the same device, without comparing those synthetic
inode numbers. Other POSIX builds retain the inode check. DOS files are opened
in binary mode. The source revision and binary digest are in `SOURCE.TXT`.
The existing alpha13 tag and other platform downloads retain their versions.
The CLI uses GPL/LGPL components. BSD-2-Clause covers optional integration
helpers included in the source; its text accompanies the distributions too.

FDNPKG's numeric version comparator cannot order the upstream alpha spelling.
The FreeDOS package therefore uses `2.99.13.1`: pre-3.0, alpha13,
DOS revision 1. Later alpha packages increase the final components; stable
`3.0.0` sorts above them. The LSM summary and CLI retain the upstream build label.

The package was tested with a Pentium CPU and 32 MB RAM under QEMU, using
FreeDOS 1.4 boot media (kernel 2043) and FDNPKG 0.99.7a. Larger archives may
need more memory. Extract into a fresh directory: DOS refuses to replace
existing output files, including when `-f` is supplied. Earlier successfully
extracted members can remain after a later failure.

Classic archives at levels 2-5 without `--chunked` are readable by original
DOS UC2. Levels 6-9 use rANS; chunked archives at every level need revival
version 3.0.0a11 or newer.
For archive creation on DOS, change to the input directory and pass relative
8.3 filenames.

FreeDOS repository acceptance is a separate maintainer decision. The
submission discloses AI assistance and asks about eligibility for an optional
third-party package under the [current FreeDOS policy](https://www.freedos.org/about/devel/).

To reproduce the ZIP, build the source revision named in
`scripts/package_freedos.py` with the DJGPP toolchain and version label
`3.0.0a13+dos1`. Supply that executable, the pinned source archive and the
pinned FreeDOS CWSDPMI package to the script. `--help` lists its arguments.
The embedded source archive is made with:

```sh
git archive --format=tar.gz --prefix=uc2-dos-src/ \
  c8f52a7b0543792d159545ab06563c37099d49b3 -o uc2-dos-source.tar.gz
```

## SDF

See [the native NetBSD guide](sdf.rst). SDF membership installed the CLI at
`/usr/local/bin/uc2`; the versioned alpha13 build is also shared under
`/sys/sdf/uc2-3.0.0a13`. Tutorial wiki publication awaits the membership team's
access response.
