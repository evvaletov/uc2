# UC2 Homebrew tap

Install the UC2 archive reader and writer on macOS or Linux:

```sh
brew install evvaletov/tap/uc2
brew test evvaletov/tap/uc2
```

UC2 is an alpha release. Keep original archives and test on copies.
The formula builds version 3.0.0a13 from its pinned source archive.

```sh
brew upgrade evvaletov/tap/uc2
brew uninstall evvaletov/tap/uc2
```

Installation, alpha12-to-alpha13 upgrade, archive round trips and removal
were checked on Linux x86_64 and macOS Intel/Apple Silicon.
[Package details and validation](https://github.com/evvaletov/uc2/blob/main/docs/distribution.md).

The CLI, man page, licence texts and optional Midnight Commander helper are
installed. The helper requires Python 3 and separate Midnight Commander
configuration. See the
[integration guide](https://github.com/evvaletov/uc2/blob/main/contrib/mc/README.md).

Report problems to the [UC2 issue tracker](https://github.com/evvaletov/uc2/issues).
The maintained recipe is also kept in the UC2 repository under
`packaging/homebrew/uc2.rb`.
