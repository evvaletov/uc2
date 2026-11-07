# UC2 legacy shared reader

`libuc2read` builds the existing LGPL decoder as a separate shared library.
It reads the supported historical Huffman, delta and Turbo (method 80) streams.
UC2 v3 rANS (method 10) and archive creation are unavailable.
Use the full UC2 package when you need rANS or the compressor.

```sh
cmake -S contrib/libuc2read -B build-reader -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build-reader
cmake --install build-reader
```

Consumers can use `pkg-config --cflags --libs libuc2read`. The API is the
reader portion of `uc2/libuc2.h`; `UC2_HUFFMAN_ONLY` hides the compressor
declarations. Avoid linking this library and full libuc2 into one process:
they export the same reader API. The caller supplies allocation callbacks
and remains responsible for resource limits.

The compiled source is `lib/src/decompress.c` and its `list.h`, both
`LGPL-3.0-only`, plus the original SuperMaster data. `UC2_HUFFMAN_ONLY`
excludes the GPL rANS implementation and header. No source is relicensed.
The small build files use `GPL-3.0-or-later OR BSD-2-Clause`.

This target gives the proposed libarchive adapter an optional shared dependency
without linking the GPL compressor or rANS code. Distributions must retain
the LGPL notices and meet its source and replacement requirements. This is
an integration design for review; libarchive maintainers have not accepted it.
See the repository's `docs/license-audit.md` for provenance.
