# UC2 RPM

`uc2.spec` builds the alpha13 source as RPM version `3.0.0~alpha.13`, which
sorts before stable `3.0.0`. Its source URL and SHA-256 are pinned. The
binary package includes the CLI, man page, licence texts and optional
Midnight Commander helper. Python 3 is a runtime dependency for the helper.

On Fedora, install the build tools and build as an ordinary user:

```sh
sudo dnf install rpm-build redhat-rpm-config cmake gcc make python3 diffutils curl
mkdir -p "$HOME/rpmbuild/SOURCES"
curl -fL https://github.com/evvaletov/uc2/archive/refs/tags/v3.0.0-alpha.13.tar.gz \
  -o "$HOME/rpmbuild/SOURCES/v3.0.0-alpha.13.tar.gz"
rpmbuild -ba packaging/rpm/uc2.spec
```

The spec verifies the source checksum before unpacking. It runs the native
test suite, excluding DOSBox, then checks classic and chunked archive round
trips. The optional OpenTimestamps reference check skips when its Python
dependency is absent. Binary and source packages appear in `~/rpmbuild/RPMS`
and `~/rpmbuild/SRPMS`. The source RPM is also suitable for a Copr upload.

See [installation and test details](../../docs/distribution.md#fedora-rpm).
