# UC2 RPM

`uc2.spec` builds the alpha13 source as RPM version `3.0.0~alpha.13`, which
sorts before stable `3.0.0`. Its source URL and SHA-256 are pinned. The
binary package includes the CLI, man page, licence texts and optional
Midnight Commander helper. Python 3 is a runtime dependency for the helper.

On Fedora or AlmaLinux, install the build tools and build as an ordinary user:

```sh
sudo dnf install rpm-build redhat-rpm-config cmake gcc make python3 diffutils /usr/bin/curl
mkdir -p "$HOME/rpmbuild/SOURCES"
curl -fL https://github.com/evvaletov/uc2/archive/refs/tags/v3.0.0-alpha.13.tar.gz \
  -o "$HOME/rpmbuild/SOURCES/v3.0.0-alpha.13.tar.gz"
rpmbuild -ba packaging/rpm/uc2.spec
```

On AlmaLinux 9, also install `python3.11` before building. The test suite
requires Python 3.11 or newer; the installed MC helper uses the system Python 3.

The spec verifies the source checksum before unpacking. It runs the native
test suite, excluding DOSBox, then checks classic and chunked archive round
trips. It requires all 36 native cases. Only the OpenTimestamps reference
check may skip, when its optional Python dependency is absent. Binary and source packages appear in
`~/rpmbuild/RPMS`
and `~/rpmbuild/SRPMS`. The source RPM contains the pinned upstream archive and spec.

## Build and test all targets

The package workflow covers Fedora 44/45 and AlmaLinux 9/10 on x86_64 and
ARM64. RPM revision 2 uses the same alpha13 source as revision 1. ARM64 jobs
run on native `ubuntu-24.04-arm` runners.

To reproduce a complete build and package lifecycle check, start a fresh
disposable container on a host of the intended architecture:

```sh
docker run --rm -v "$PWD:/repo:ro" -w /repo fedora:45 \
  packaging/rpm/build-test.sh /tmp/uc2-rpm-artifacts
```

Replace `fedora:45` with `fedora:44`, `almalinux:9` or `almalinux:10` as needed.
The script installs build dependencies, builds as an unprivileged user, then
checks fresh installation, upgrade from alpha12, removal and reinstallation.
It refuses to replace an existing UC2 installation. The output directory must
be new; mount an output directory if you want to retain the packages and JSON
validation report after the container exits. For example, create `rpm-output`
and add `-v "$PWD/rpm-output:/out"`, then use `/out/artifacts` as the argument.
CI retains those outputs as per-target workflow artefacts.

See [installation and test details](../../docs/distribution.md#fedora-rpm).
