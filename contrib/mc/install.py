#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Install the UC2 helper into the current user's Midnight Commander config."""
import argparse
import os
from pathlib import Path
import re
import shutil
import shlex
import tempfile

BEGIN = "# BEGIN UC2 integration (managed by uc2 installer)"
END = "# END UC2 integration"


def atomic_write(path, data, mode=0o644):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix=".uc2-install-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(data)
        os.chmod(name, mode)
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def install(root, *, data_home, config_home, cli):
    helper = data_home / "mc/extfs.d/uc2"
    config = config_home / "mc/mc.ext.ini"
    if config.exists():
        text = config.read_text()
    else:
        defaults = [Path("/etc/mc/mc.ext.ini"), Path("/usr/local/etc/mc/mc.ext.ini")]
        system = next((p for p in defaults if p.is_file()), None)
        if system is None:
            raise FileNotFoundError("MC extension configuration missing; install Midnight Commander first")
        text = system.read_text()
    if not re.search(r"(?m)^\[mc\.ext\.ini\][ \t]*$", text):
        raise ValueError("MC extension configuration lacks its [mc.ext.ini] metadata")
    if (BEGIN in text) != (END in text):
        raise ValueError("incomplete UC2 configuration block; repair it before installing")
    if BEGIN in text:
        start, finish = text.index(BEGIN), text.index(END) + len(END)
        text = text[:start] + text[finish:].lstrip("\n")
    snippet = (root / "mc.ext.ini").read_text().strip()
    # Adopt an exact manual copy; preserve customized sections for manual review.
    if snippet in text:
        text = text.replace(snippet, "", 1)
    if re.search(r"(?m)^\[UC2\][ \t]*$", text):
        raise ValueError("an existing customized [UC2] section requires manual reconciliation")
    command = shlex.quote(str(cli)).replace("%", "%%")
    snippet = snippet.replace(" uc2 -la -- %f", f" {command} -la -- %f")
    block = BEGIN + "\n" + snippet + "\n" + END + "\n\n"
    section = re.search(r"(?m)^\[Default\][ \t]*$", text)
    offset = section.start() if section else -1
    text = text[:offset] + block + text[offset:] if offset >= 0 else text.rstrip() + "\n\n" + block
    backup = None
    if config.exists() and config.read_text() != text:
        fd, name = tempfile.mkstemp(prefix="mc.ext.ini.before-uc2-", dir=config.parent)
        os.close(fd)
        backup = Path(name)
        shutil.copyfile(config, backup)
    # Embed a Python string literal, never shell-interpolate the executable path.
    code = (root / "uc2").read_text()
    code = code.replace("import os\n", "import os\nos.environ.setdefault('UC2', " + repr(str(cli)) + ")\n", 1)
    atomic_write(helper, code.encode(), 0o755)
    atomic_write(config, text.encode())
    print(f"Installed {helper}\nConfigured {config}")
    if backup:
        print(f"Previous configuration: {backup}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uc2", type=Path, help="UC2 executable; otherwise locate it on PATH")
    args = parser.parse_args()
    cli = args.uc2 or shutil.which("uc2")
    if not cli or not Path(cli).is_file() or not os.access(cli, os.X_OK):
        parser.error("install the UC2 CLI first, or pass --uc2 /absolute/path/to/uc2")
    try:
        install(Path(__file__).resolve().parent,
                data_home=Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share")),
                config_home=Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config")),
                cli=Path(cli).resolve())
    except (OSError, ValueError) as error:
        parser.exit(1, f"uc2 MC install: {error}\n")


if __name__ == "__main__":
    main()
