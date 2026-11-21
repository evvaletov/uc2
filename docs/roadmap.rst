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

The next priorities are:

1. Resolve libarchive's optional dependency, format ID and decoded-entry spool
   decisions with the maintainers.
2. Design snapshot growth beyond fixed reference and manifest caps; assess
   concurrent readers separately from the exclusive lock.
3. Prepare and installation-test a FreeDOS ZIP/LSM package, including the DOS
   replacement limitation, then confirm current submission requirements.
4. Prepare Homebrew tap and AUR packages with installation, upgrade and removal
   checks.
5. Add a reproducible Linux/DOS round-trip demo, with commands, a recording and
   a text alternative.

See `ROADMAP.md on GitHub
<https://github.com/evvaletov/uc2/blob/main/ROADMAP.md>`_ for evidence, completed
work and tracking references. The TC catalogue enquiry and `MC issue #5225
<https://github.com/MidnightCommander/mc/issues/5225>`_ were submitted on
19 November (EST) and verified on 20 November. Their `submission receipts
<https://github.com/evvaletov/uc2/blob/main/docs/qa/commander-submissions-2026-11-20.md>`_
record the completed checks. Catalogue acceptance and an MC merge remain pending.
