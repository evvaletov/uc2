# Commander submissions verified — 20 November 2026

Both proposals were already submitted when checked on 20 November. No duplicate
email or issue was created. This supersedes the access blockers recorded in the
[14 November review](commander-submissions-2026-11-14.md).

| Proposal | Submission | Verification |
| --- | --- | --- |
| Total Commander catalogue enquiry | Email to `support@ghisler.com`, 19 November 2026 at 21:49 EST | The owner's Gmail Sent copy on asteroid01 has the intended sender, recipient and subject; its body matches the reviewed email. |
| Midnight Commander extfs helper | [Issue #5225](https://github.com/MidnightCommander/mc/issues/5225), created 20 November 2026 at 02:44:57 UTC (19 November, 21:44:57 EST) | Open issue by `evvaletov`; the posted text differs from the reviewed issue only in its updated upstream-revision sentence. |

The email subject is **UC2 read-only WCX plugin for 32-bit and 64-bit Total
Commander**. Its [reviewed source](../../contrib/totalcommander/CATALOG-MESSAGE.txt)
is unchanged, SHA256
`29477402a4214840e07a8167aa0fa5a579b70628fb53173a8242573b383ac4a3`.
The Sent body comparison normalises CRLF to LF and removes terminal newlines;
all other text matches. The displayed recipient details show the intended
support address, and no attachment is visible.

The MC issue retains the public alpha12 patch link. It now distinguishes the
original patch base, `3228c23fd57db28c20c35db8e0e1c6d3b087b3c7`, from the newer
revision `fc236523376ef3e66f597759ea36380df6ef121e` named in the posted text.
On 20 November, `git apply --cached --check` against that newer revision passed
using a separate temporary index. The earlier **71/71** full-suite result
remains evidence for the original base; the full suite was not rerun for the
newer revision. The patch and implementation bytes are unchanged.

QA, prose/AI-tell and elegance reviews were completed on 14 November, as recorded
in the linked review. This verification reconciles existing submissions and
checks the one factual update in the posted MC text.

At verification, the MC issue had zero comments. A scoped Gmail search for UC2
messages from `ghisler.com` or `ghisler.ch` found no matching reply. This is a
status check, not a catalogue acceptance, merge or recipient delivery
acknowledgement. Follow maintainer feedback under tracker `4b7fbd1`.

Detailed mailbox and API receipts remain in the local Git administrative
directory. Private mailbox URLs, account data and session credentials are not
included in the public repository.
