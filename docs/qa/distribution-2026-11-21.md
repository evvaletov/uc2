# Distribution publication, 21 November 2026

UC2 now has a public [Homebrew tap](https://github.com/evvaletov/homebrew-tap),
a tested FreeDOS ZIP, and Fedora 45 binary and source RPMs. The Arch recipe
passes its local build and package lifecycle checks; AUR registration is closed
according to the owner's report. Copr publication awaits a normal OIDC login.
The standalone RPMs are unsigned.

The [alpha13 GitHub release](https://github.com/evvaletov/uc2/releases/tag/v3.0.0-alpha.13)
retains its original 31 assets and adds six package/source/checksum downloads.
All six public downloads matched their reviewed sizes and SHA-256 digests.
The [SDF release entry](https://git.sdf.org/evv/uc2/releases/tag/v3.0.0-alpha.13)
links to the same downloads. Both saved release bodies were read back.

The DOS build is `3.0.0a13+dos1`, source revision `c8f52a7`. It fixes DJGPP's
synthetic-inode mismatch on FreeDOS and opens files in binary mode. FDNPKG
uses numeric version `2.99.13.1`. Fresh installation and alpha12 upgrade,
removal, bundled CWSDPMI, six byte-for-byte fixture checks, refusal to replace
existing output, and DOS-created L4/L9 archive recovery passed on FreeDOS 1.4
boot media under QEMU. Drive-qualified input paths remain a known issue;
the package guide documents relative 8.3 input paths.

Homebrew installation, upgrade, extraction, removal and reinstallation passed
on Linux x86_64 and both Mac architectures. Arch's corresponding checks passed,
including `pacman -Qkk` with no altered files. Fedora 45 non-root RPM builds
passed 35 native tests, with one optional OpenTimestamps skip and DOSBox
excluded. Its upgrade, file verification, round trips, removal and reinstallation
passed. A clean container also installed the public RPM and restored all six
fixture files exactly. A deliberately mismatched checksum prevented installation.

SDF membership installed UC2 at `/usr/local/bin/uc2`. On otaku, that binary
passed archive integrity and all six extraction comparisons. Its banner is
`UC2 3.0.0`; its exact source revision is unconfirmed. The separately installed
`/sys/sdf/uc2-3.0.0a13` tree contains the pinned alpha13 binary, man page,
licence texts and exact source. All eight files and public permissions were
checked, and the published recovery example passed. The reviewed acknowledgement
and shared-install follow-up was posted on REQUESTS and read back; the board
records 17:45:35. Wiki access remains pending.

The FreeDOS eligibility enquiry was sent from the owner's chosen Gmail account
to freedos-devel and read back in Sent with an exact body match. It discloses
AI assistance and asks whether this optional third-party application is eligible.
Repository acceptance is pending.

Correctness, prose and elegance reviews used Grok 4.7, Fable 5.2 and GPT-6.2
Astra. Astra replaced unavailable Gemini Argon subscription access. Implementation
and elegance checks had two consecutive rounds with no required findings.
Later documentation findings were repaired and checked, including checksum-gated
RPM installation, exact SDF download links, GitHub Markdown rendering and RST
heading lengths. A focused two-model review covered the subsequent SDF reply
and installation update. Sphinx built successfully; the final prose scan found
no flagged patterns. Reviewer scope and repairs are in the [review record](distribution-2026-11-21/reviews.json).

All five GitHub workflows passed for code/package commit `d8f0b44`: Build,
Distributions, Integration releases, Docs and Package recipes. The
[validation record](distribution-2026-11-21/validation.json) and
[publication receipt](distribution-2026-11-21/publication.json) give the test
scope, live URLs, exact asset hashes and CI run links.
