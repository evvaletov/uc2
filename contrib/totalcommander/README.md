# Total Commander UC2 reader

This WCX plugin lists, tests, and extracts UC2 archives, including custom master
blocks and UC2 v3 rANS entries. It supports the Unicode WCX API, older ANSI
entry points, version suffixes, cancellation, and content-based recognition.
Extraction stages each file before replacing its destination. The host controls
the extraction destination and metadata application. Packing is not supported.

Open `uc2-wcx-<version>.zip` from the
[release page](https://github.com/evvaletov/uc2/releases) inside Total Commander
to install it. The archive contains both 32-bit and 64-bit plugins, their
installation metadata, license notices, and a corresponding-source reference.
Both plugins link the C runtime statically.

Total Commander 11.58.1 has different behavior in its two architectures:

- In 64-bit TC, install the plugin for `uc2` and open the archive normally.
- In 32-bit TC, register the same plugin for the alias extension `u2r` under
  Configuration, Options, Packer, Configure packer extensions. Make a copy of
  the archive with extension `.u2r`, then open that copy and extract from it.
  The file contents stay unchanged. This avoids TC's historical `.uc2` path,
  which requests DOS `UC.EXE` during extraction even when the WCX plugin was
  selected for listing. The native application test uses this workaround.

For a manual `wincmd.ini` configuration, add the following to its existing
section with the correct plugin location:

```ini
[PackerPlugins]
uc2=196,C:\path\to\uc2.wcx
u2r=196,C:\path\to\uc2.wcx
```

Use `uc2.wcx64` for a 64-bit-only installation. An internal association such as
`**u2r` can select the plugin for listing `.uc2` files, but did not avoid the
32-bit extraction behavior in this release's native tests. Keep the original
archive when making the `.u2r` copy; it remains an ordinary UC2 archive.

Download the official [WCX SDK](https://github.com/ghisler/WCX-SDK) and point
`UC2_WCX_SDK_DIR` at the directory containing `wcxhead.h`. The SDK is a build
dependency and is not vendored or relicensed here.

On Windows with a C compiler:

```sh
cmake -B build-wcx -DUC2_BUILD_WCX=ON -DUC2_WCX_SDK_DIR=C:/src/WCX-SDK/src
cmake --build build-wcx --config Release --target uc2_wcx test_wcx
```

On Linux with 64-bit MinGW:

```sh
env -u CPATH -u C_INCLUDE_PATH -u CPLUS_INCLUDE_PATH cmake -B build-wcx \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
  -DCMAKE_ASM_COMPILER=x86_64-w64-mingw32-gcc \
  -DCMAKE_BUILD_TYPE=Release -DUC2_BUILD_TESTS=OFF \
  -DUC2_BUILD_WCX=ON -DUC2_WCX_SDK_DIR=/absolute/path/WCX-SDK/src
env -u CPATH -u C_INCLUDE_PATH -u CPLUS_INCLUDE_PATH \
  cmake --build build-wcx --target uc2_wcx test_wcx
```

The 64-bit output is `contrib/totalcommander/uc2.wcx64`; a 32-bit toolchain
produces `uc2.wcx`. In Total Commander, use Configuration, Options, Packer,
Configure packer extensions, choose the `uc2` extension, and select the matching
plugin. `pluginst.inf` is provided for ZIP-based plugin installation.

To reproduce the Linux runtime checks:

```sh
docker build -t uc2-wcx-test -f tests/wcx/Dockerfile .
python3 tests/wcx/run_fixture.py --uc2 build/cli/uc2 \
  --plugin build-wcx/contrib/totalcommander/uc2.wcx64 \
  --host build-wcx/contrib/totalcommander/test_wcx.exe \
  --work /tmp/uc2-wcx-test --wine-image uc2-wcx-test
```

The container has no network access during tests. The fixture directory must be
new and needs about 700 MiB for Wine's profile. On Windows, omit `--wine-image`
and use the corresponding native executable paths.

The test host `test_wcx` loads the actual DLL and exercises recognition, listing,
skip/test/extract, byte comparisons, and cancellation without replacing an
existing file. Its arguments are DLL, archive, original-files directory, and
an existing empty output directory. It can run on Windows or in the isolated
Wine container described by `tests/wcx/Dockerfile`.

The `Integration releases` workflow also runs Total Commander 11.58.1 itself
on isolated native Windows runners. It checks that the application loads the
plugin, opens an archive, and extracts selected files with exact byte matches.
The official trial executable is checked against its pinned SHA-256 digest and
Ghisler Authenticode signature. The application is used for testing and is not
included in UC2 downloads.

Archive names use UC2's existing CP850/long-name decoding. Unicode filesystem
paths to the archive are supported. Windows-unsafe entry components and ambiguous names after version suffixing
or case folding are rejected.
This does not expand the archive writer's filename encoding support.

The adapter derives from Jan Bobrowski's 2021 prototype and retains its
`LGPL-3.0-only` source license. The linked plugin includes GPL UC2 components;
the limited dual-license choice for independently written glue does not apply
to Bobrowski's code. See `docs/license-audit.md`.
