Roadmap
=======

Alpha12 includes the project audit repairs, CLI metadata indexes and expanded
snapshot scale/recovery validation. See the `release record
<https://github.com/evvaletov/uc2/blob/main/docs/releases/3.0.0a12-qa.md>`_
and `measurements
<https://github.com/evvaletov/uc2/blob/main/docs/benchmarks/2026-11-14-scale-recovery.md>`_.

Alpha13 adds automatic layout selection and staged CLI extraction/OTS output.
See the `release checks
<https://github.com/evvaletov/uc2/blob/main/docs/releases/3.0.0a13-qa.md>`_.

SDF membership installed the native CLI at ``/usr/local/bin/uc2``. A versioned
alpha13 build is also shared at ``/sys/sdf/uc2-3.0.0a13/bin/uc2``. The release
entry, archive-recovery guide, RETRO introduction and REQUESTS installation
update are complete. Wiki publication awaits the tutorial-access response.
See the `SDF evidence
<https://github.com/evvaletov/uc2/blob/main/docs/qa/sdf-2026-11-21.md>`_.

The next priorities are:

1. Publish a signed Copr repository from the tested RPM, then assess further
   Fedora and Enterprise Linux targets. Resume AUR upload when registration opens.
2. Fix drive-qualified DOS input paths and add a FreeDOS regression check.
3. Publish the reviewed SDF tutorial when wiki access is granted.
4. Resolve the FreeDOS eligibility enquiry and libarchive's optional dependency,
   format ID and decoded-entry spool decisions; track MC and TC separately.
5. Design snapshot growth beyond fixed reference and manifest caps, preserving
   v1/v2 readability and verified publication. Assess concurrent readers separately.

The FreeDOS ZIP, Homebrew tap and Fedora RPMs are published, with installation,
upgrade, archive round-trip and removal checks. The Arch recipe is tested and
available for local builds. See the `package guide
<https://github.com/evvaletov/uc2/blob/main/docs/distribution.md>`_. The FreeDOS
submission discloses AI assistance; repository acceptance is pending.

See `ROADMAP.md on GitHub
<https://github.com/evvaletov/uc2/blob/main/ROADMAP.md>`_ for evidence, completed
work and tracking references. The TC catalogue enquiry and `MC issue #5225
<https://github.com/MidnightCommander/mc/issues/5225>`_ were submitted on
19 November (EST) and verified on 20 November. Their `submission receipts
<https://github.com/evvaletov/uc2/blob/main/docs/qa/commander-submissions-2026-11-20.md>`_
record the completed checks. Catalogue acceptance and an MC merge remain pending.
