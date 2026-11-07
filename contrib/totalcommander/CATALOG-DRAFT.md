# Total Commander catalog proposal

Prepared November 7, 2026. No message has been sent.

The [official plugin catalog](https://www.ghisler.com/plugins.htm) asks authors
to contact the project before a catalog listing to avoid duplicates. Its current
page has no UC2 entry. Use the contact route on the
[official support page](https://www.ghisler.com/faq.htm).

Proposed subject: **UC2 read-only WCX plugin for 32-bit and 64-bit Total Commander**

Proposed message:

> I maintain the cross-platform UltraCompressor II revival and would like to
> ask whether its read-only WCX plugin would be useful in the plugin catalog.
>
> The plugin lists, tests and extracts UC2 archives without a DOS runtime. It
> supports original Huffman and Turbo archives, stored file versions, shared
> dictionaries and the revival's rANS extension. One installable ZIP contains
> both 32-bit and 64-bit builds, license notices and a source reference.
>
> Automated tests run Total Commander 11.58.1 itself in both architectures,
> opening `.uc2` through ordinary plugin registration and comparing extracted
> bytes. The plugin does not pack archives or resolve SAS/UC2SEA volume sets.
>
> Source: https://github.com/evvaletov/uc2/tree/main/contrib/totalcommander
>
> Downloads: https://github.com/evvaletov/uc2/releases
>
> Please let me know whether this fits the catalog and which listing details
> or additional checks you would need.

Suggested short listing:

**UC2** — Read-only UltraCompressor II packer plugin. Lists, tests and extracts
original UC2 archives and stored versions; supports the revival's rANS extension.
Native 32-bit and 64-bit builds, Unicode WCX interface, no DOS runtime required.
Open source; GPL/LGPL components. Alpha release.

The catalog request should point to the verified release package, its SHA256SUMS
and corresponding-source link when sent. The [plugin README](README.md) contains
installation, supported formats and native-application validation details.
