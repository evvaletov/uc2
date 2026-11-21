UC2 3.0.0a13 is an alpha release of the portable C99 revival of
UltraCompressor II.

- [Standalone downloads and SHA256SUMS](https://github.com/evvaletov/uc2/releases/tag/v3.0.0-alpha.13)
- [Python package 3.0.0a13](https://pypi.org/project/uc2/3.0.0a13/)
- [Release changes and validation](https://git.sdf.org/evv/uc2/src/branch/main/docs/releases/3.0.0a13-qa.md)
- [Native SDF build and archive-recovery demonstration](https://git.sdf.org/evv/uc2/src/branch/main/docs/sdf.rst)

Alpha13 adds automatic archive layout selection. CLI extraction and
OpenTimestamps operations now stage their output. Use classic mode at levels 2-5 for original DOS
UC2 compatibility. Levels 6-9 and chunked output require the revival reader;
chunked archives require 3.0.0a11 or newer.

The source tag points to `b297757b7509b3b46be9002979eff5e3e20e6ea8`.
A native build of that source passed 35 tests on SDF's NetBSD 9.3 amd64
host otaku after two Python test scripts were made portable. One optional
OpenTimestamps cross-check was skipped, and DOSBox testing was excluded on
SDF. Those test repairs leave the compiled release source unchanged.

GitHub hosts the binary downloads and PyPI hosts the Python distributions.
This SDF release entry provides the matching source tag and download links.
