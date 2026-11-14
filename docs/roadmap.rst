Roadmap
=======

Alpha12 includes the project audit repairs, CLI metadata indexes and expanded
snapshot scale/recovery validation. See the `release record
<https://github.com/evvaletov/uc2/blob/main/docs/releases/3.0.0a12-qa.md>`_
and `measurements
<https://github.com/evvaletov/uc2/blob/main/docs/benchmarks/2026-11-14-scale-recovery.md>`_.

The next priorities are:

1. Submit the prepared Total Commander and Midnight Commander proposals once
   the user has authorised those contacts; confirm MC's DCO sign-off.
2. Tune shared-chunk compression against the documented public-source regression
   while preserving edited-binary gains and compatibility.
3. Resolve libarchive's optional dependency, format ID and decoded-entry spool
   decisions with the maintainers.
4. Add transactional CLI extraction and OTS attachment, preserving existing
   outputs on failure.
5. Design snapshot growth beyond the fixed reference and manifest caps; assess
   read-only/concurrent access separately from the current exclusive lock.

See `ROADMAP.md on GitHub
<https://github.com/evvaletov/uc2/blob/main/ROADMAP.md>`_ for evidence, completed
work and tracking references. TC and MC submissions remain pending.
