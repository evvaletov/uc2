# Commander submission review — 14 November 2026

The Total Commander catalogue enquiry and Midnight Commander feature proposal
have completed QA, prose/AI-tell review and implementation-elegance review.
The owner authorised submission after those checks. The exact reviewed messages
are [the TC email](../../contrib/totalcommander/CATALOG-MESSAGE.txt) and
[the MC issue](../../contrib/mc/upstream/ISSUE-DRAFT.md).

Three independent reviewers completed the submission review: GPT-6 Astra,
Grok 4.7 Build Fast and Claude Opus 5.5. The implementation also retains the
two clean integration rounds recorded in the
[project audit](project-2026-11-13/reviews.json); the reviewed helper and WCX
source hashes are unchanged.

The submission panel's substantive findings were readiness gates: finish the
full upstream test suite, repeat parser/copyout checks using the public alpha12
CLI, and verify the public patch link against the reviewed bytes. All three
gates passed. Optional prose and fixture-order preferences were left unchanged;
there are no unresolved substantive or blocking findings. A Gemini review
attempt failed authentication and is not counted as a completed review.

Validation:

- The patch applies to MC master `3228c23fd57db28c20c35db8e0e1c6d3b087b3c7`.
- A clean Linux ncurses build with Check 0.15.2, `--enable-tests` and
  `--enable-werror` passes `make check`: **71/71**, with no skips or failures.
  Installation also passes.
- The installed helper and public alpha12 Linux CLI pass MC's listing parser
  and exact-byte copyout checks for literal wildcard/bracket/semicolon names
  and both stored versions. This submission check does not repeat every
  upstream platform configuration.
- The patch downloaded from the public alpha12 tag matches SHA256
  `f1e2088bed412c1cc233d4f8bd73c14231fd78d500c28ff0dac4c2e34303d6fe`.
- The public TC ZIP matches SHA256
  `187edd950d2afc411496268f96c380c7fc59ec1ab120d270c8ff71566ab1f554`.
  The native 32-/64-bit Total Commander validation claim matches the released
  integration workflow and its successful Windows jobs.

Reviewed message SHA256 hashes:

| Message | SHA256 |
| --- | --- |
| TC email | `29477402a4214840e07a8167aa0fa5a579b70628fb53173a8242573b383ac4a3` |
| MC issue | `219c39e3e9a2b8f82052690b2980391c3a11ce52fa5b5c28bcaa17b9c19fdda8` |

Submission state:

- **Total Commander:** ready; the owner selected a Gmail sender and authorised
  the sf01 browser route. That sender is neither signed in nor configured as a
  sending address in the inspected browser session. Account access is pending.
  No email has been sent.
- **Midnight Commander:** GitHub rejected issue creation with an account
  restriction. A subsequent issue search found no matching submission. The
  reviewed issue remains ready; no issue or PR has been submitted. Its patch
  link does not assert a contributor DCO sign-off. A later code commit must
  meet MC's sign-off and issue-number requirements.

Detailed review, build and attempted-submission receipts are retained in the
local Git administrative directory. Mailbox contents and authentication data
are excluded from the public repository.
