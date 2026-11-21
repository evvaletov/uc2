## Additional packages (21 November 2026)

- [FreeDOS UC2.ZIP](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/UC2.ZIP)
  contains the `3.0.0a13+dos1` CLI, CWSDPMI, licences and complete source.
  FDNPKG identifies it as `2.99.13.1` so numeric version comparisons work.
- [Separate DOS source archive](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/uc2-3.0.0a13-dos1-source.tar.gz)
  and [DOS download checksums](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/DOS-SHA256SUMS.txt).
- [Homebrew tap](https://github.com/evvaletov/homebrew-tap):
  `brew install evvaletov/tap/uc2` on macOS or Linux.
- [Arch package recipe](https://github.com/evvaletov/uc2/tree/main/packaging/aur),
  ready to build locally; AUR submission is pending.
- RPM revision 2 covers Fedora 44/45 and AlmaLinux 9/10 on x86_64 and ARM64.
  The eight binary packages and four source RPMs are listed below.

| Tested distribution | x86_64 | ARM64 (aarch64) | Source |
| --- | --- | --- | --- |
| Fedora 44 | [x86_64](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/uc2-3.0.0a13-2.fc44.x86_64.rpm) | [ARM64](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/uc2-3.0.0a13-2.fc44.aarch64.rpm) | [SRPM](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/uc2-3.0.0a13-2.fc44.src.rpm) |
| Fedora 45 | [x86_64](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/uc2-3.0.0a13-2.fc45.x86_64.rpm) | [ARM64](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/uc2-3.0.0a13-2.fc45.aarch64.rpm) | [SRPM](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/uc2-3.0.0a13-2.fc45.src.rpm) |
| AlmaLinux 9 | [x86_64](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/uc2-3.0.0a13-2.el9.x86_64.rpm) | [ARM64](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/uc2-3.0.0a13-2.el9.aarch64.rpm) | [SRPM](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/uc2-3.0.0a13-2.el9.src.rpm) |
| AlmaLinux 10 | [x86_64](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/uc2-3.0.0a13-2.el10.x86_64.rpm) | [ARM64](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/uc2-3.0.0a13-2.el10.aarch64.rpm) | [SRPM](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/uc2-3.0.0a13-2.el10.src.rpm) |

[Download checksums](https://github.com/evvaletov/uc2/releases/download/v3.0.0-alpha.13/RPM-SHA256SUMS.txt).
The AlmaLinux 10 x86_64 RPM requires an `x86-64-v3` CPU.
These standalone RPMs are unsigned. See the
[installation guide](https://github.com/evvaletov/uc2/blob/main/docs/distribution.md#fedora-rpm)
for checksum verification, installation and removal. Revision 1 downloads
remain available. Copr publication is deferred.

The DOS revision fixes file opening when DJGPP supplies synthetic inode
numbers. Its source is commit `c8f52a7b0543792d159545ab06563c37099d49b3`.
The alpha13 tag and existing platform downloads retain their original source
and versions. [Installation and test details](https://github.com/evvaletov/uc2/blob/main/docs/distribution.md).

The native NetBSD amd64 CLI is also shared on SDF at
`/sys/sdf/uc2-3.0.0a13/bin/uc2`.
[SDF guide](https://git.sdf.org/evv/uc2/src/branch/main/docs/sdf.rst).
