# Midnight Commander

The `uc2` extfs helper supports browsing, viewing, and copying files out of UC2
archives. It preserves the CLI's `;N` version selectors and quotes names for
literal extraction. Archive modification is not supported.

Requirements: Python 3 and the current UC2 CLI on `PATH`. Set `UC2` to an
absolute executable path when testing another build.

Download `uc2-mc-<version>.tar.gz` from the
[release page](https://github.com/evvaletov/uc2/releases), unpack it, and run:

```sh
python3 install.py --uc2 /absolute/path/to/uc2
```

From a source checkout, run `python3 contrib/mc/install.py` instead. The
installer preserves the existing MC extension definitions, saves a backup
before changing the configuration, and supports the XDG data/config paths.
Running it again does not duplicate the UC2 section. Restart MC after installing.

For a user installation:

```sh
mkdir -p ~/.local/share/mc/extfs.d
install -m 755 contrib/mc/uc2 ~/.local/share/mc/extfs.d/uc2
```

Add the section in `mc.ext.ini` to your `~/.config/mc/mc.ext.ini`, before its
`[Default]` section. If you do not have a user extension file, start with the
system file (normally `/etc/mc/mc.ext.ini`). Keep its `[mc.ext.ini]` metadata
section. Restart MC, then press Enter on a `.uc2` file. An explicit VFS path
also works: `/absolute/path/archive.uc2/uc2://`.

The CMake installation puts the helper under the selected prefix's
`libexec/mc/extfs.d` and an extension-file snippet under `share/uc2/mc`.
An existing system MC normally uses its compiled-in libexec directory, so a
user installation should use the per-user extfs directory above.

The text extfs protocol cannot represent all possible control characters in
filenames. Unsupported or ambiguous names cause an error. Failed extraction
does not replace the destination with partial data. Ordinary names containing
spaces, wildcard characters, and semicolon/digit suffixes are covered by the
integration test.

This independently written helper is dual-licensed under
`GPL-3.0-or-later OR BSD-2-Clause`. The release archive includes the BSD text
under `LICENSES/`; UC2 itself keeps its existing licenses.
