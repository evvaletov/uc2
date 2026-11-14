Roadmap
=======

Alpha12 includes the project audit repairs, CLI metadata indexes and expanded
snapshot scale/recovery validation. See the `release record
<https://github.com/evvaletov/uc2/blob/main/docs/releases/3.0.0a12-qa.md>`_
and `measurements
<https://github.com/evvaletov/uc2/blob/main/docs/benchmarks/2026-11-14-scale-recovery.md>`_.

The next priorities are:

1. Complete the authorised Total Commander and Midnight Commander submissions.
   QA, prose and elegance reviews are complete. TC awaits access to the selected
   sender; GitHub rejected MC issue creation with an account restriction.
   A later MC code commit needs the contributor's DCO sign-off.
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
work and tracking references. TC and MC submissions remain pending; their
`submission review record
<https://github.com/evvaletov/uc2/blob/main/docs/qa/commander-submissions-2026-11-14.md>`_
records completed checks and the remaining access blockers.
