Roadmap
=======

Alpha12 includes the project audit repairs, CLI metadata indexes and expanded
snapshot scale/recovery validation. See the `release record
<https://github.com/evvaletov/uc2/blob/main/docs/releases/3.0.0a12-qa.md>`_
and `measurements
<https://github.com/evvaletov/uc2/blob/main/docs/benchmarks/2026-11-14-scale-recovery.md>`_.

The next priorities are:

1. Tune shared-chunk compression against the documented public-source regression
   while preserving edited-binary gains and compatibility.
2. Resolve libarchive's optional dependency, format ID and decoded-entry spool
   decisions with the maintainers.
3. Add transactional CLI extraction and OTS attachment, preserving existing
   outputs on failure.
4. Design snapshot growth beyond the fixed reference and manifest caps; assess
   read-only/concurrent access separately from the current exclusive lock.
5. Prepare and installation-test a FreeDOS ZIP/LSM package, then confirm current
   repository submission requirements.

See `ROADMAP.md on GitHub
<https://github.com/evvaletov/uc2/blob/main/ROADMAP.md>`_ for evidence, completed
work and tracking references. The TC catalogue enquiry and `MC issue #5225
<https://github.com/MidnightCommander/mc/issues/5225>`_ were submitted on
19 November (EST) and verified on 20 November. Their `submission receipts
<https://github.com/evvaletov/uc2/blob/main/docs/qa/commander-submissions-2026-11-20.md>`_
record the completed checks. Catalogue acceptance and an MC merge remain pending.
