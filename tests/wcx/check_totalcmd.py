#!/usr/bin/env python3
"""Drive an isolated native Total Commander trial through list and extraction."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import time

from pywinauto import Application
from pywinauto.controls.hwndwrapper import InvalidWindowHandle
from PIL import ImageGrab

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--app", type=Path, required=True)
parser.add_argument("--plugin", type=Path, required=True)
parser.add_argument("--uc2", type=Path, required=True)
parser.add_argument("--work", type=Path, required=True)
args = parser.parse_args()
root = args.work.resolve()
root.mkdir(parents=True, exist_ok=False)
source, output = root / "source", root / "output"
(source / "nested").mkdir(parents=True)
output.mkdir()
files = {"hello.txt": b"Total Commander native application check\r\n" * 200,
         "nested/data.bin": bytes(range(256)) * 400, "empty": b""}
for name, data in files.items():
    (source / name).write_bytes(data)
archive = root / "fixture.uc2"
subprocess.run([str(args.uc2.resolve()), "-qw", str(archive), "hello.txt", "nested", "empty"],
               cwd=source, check=True)
ini = root / "wincmd.ini"
ini.write_text("[Configuration]\nUseIniInProgramDir=0\nFirstTime=0\nUseNewDefFont=1\n"
               "DirBrackets=0\nStartupScreen=0\n[PackerPlugins]\n"
               f"uc2=68,{args.plugin.resolve()}\n", encoding="utf-8")
command = [str(args.app.resolve()), "/N", f"/I={ini}", f"/F={root / 'wcx_ftp.ini'}",
           f"/L={archive}", f"/R={output}"]
process = subprocess.Popen(command)
app = Application(backend="win32").connect(process=process.pid, timeout=20)
observed = []


def windows():
    # TC destroys its trial window between EnumWindows and wrapper creation.
    for _ in range(20):
        try:
            return app.windows()
        except InvalidWindowHandle:
            time.sleep(0.05)
    raise RuntimeError("Total Commander window enumeration did not settle")


def snapshot():
    rows = []
    for window in windows():
        controls = []
        for child in window.descendants():
            try:
                controls.append(dict(handle=child.handle, cls=child.class_name(), text=child.window_text()))
            except Exception:
                pass
        rows.append(dict(handle=window.handle, cls=window.class_name(), text=window.window_text(), controls=controls))
    observed.append(rows)
    (root / "windows.json").write_text(json.dumps(observed, indent=2))
    return rows


def press_trial_prompt():
    # Use the visible trial instruction, without altering registration state.
    for window in windows():
        if window.class_name() == "TICONLIBCHANGEDLG":
            for child in window.descendants():
                if child.window_text() == "Cancel":
                    child.send_message(0xF5, 0, 0)
                    return True
        if window.class_name() == "TNASTYNAGSCREEN":
            # TC exposes the large visible choice digit in a separate panel.
            digits = [c.window_text().strip() for c in window.descendants()
                      if c.window_text().strip() in ("1", "2", "3")]
            if len(digits) == 1:
                for child in window.descendants():
                    if child.window_text() == "&" + digits[0]:
                        child.send_message(0xF5, 0, 0)  # BM_CLICK
                        return True
        text = " ".join([window.window_text(), *[c.window_text() for c in window.descendants()]])
        match = re.search(r"(?:button|press)\D{0,30}([123])", text, re.I)
        if match:
            wanted = match.group(1)
            for child in window.descendants():
                if child.window_text().replace("&", "").strip() == wanted:
                    child.click()
                    return True
    return False


try:
    deadline = time.monotonic() + 35
    main = None
    while time.monotonic() < deadline:
        snapshot()
        press_trial_prompt()
        candidates = [w for w in windows() if w.class_name().startswith("TTOTAL_CMD")]
        if candidates:
            main = candidates[0]
            # A modal trial window may still prevent interaction.
            if main.is_enabled():
                break
        time.sleep(0.5)
    if main is None or not main.is_enabled():
        raise RuntimeError("Total Commander did not finish its visible startup prompt")
    time.sleep(1)
    rows = snapshot()
    # Some TC builds unload a WCX after listing. Record module observation,
    # then require actual application extraction and exact bytes below.
    modules = subprocess.check_output([
        "powershell", "-NoProfile", "-Command",
        f"(Get-Process -Id {process.pid}).Modules | ForEach-Object {{$_.FileName}}"], text=True)
    observed_module = args.plugin.name.lower() in modules.lower()
    if not any("fixture.uc2" in c.window_text().lower() for c in main.descendants()):
        raise RuntimeError("Total Commander did not enter the UC2 archive")
    (root / "loaded-modules.txt").write_text(modules)
    # TC's documented WM_USER+51 interface; IDs come from TOTALCMD.INC.
    main.send_message(1075, 3301, 0)  # cm_SelectAllBoth
    main.post_message(1075, 905, 0)  # cm_Copy opens a modal confirmation
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        if all((output / name).is_file() and (output / name).read_bytes() == data
               for name, data in files.items()):
            break
        snapshot()
        for window in windows():
            if window.handle == main.handle:
                continue
            for child in window.descendants():
                if child.window_text().replace("&", "").strip().lower() in ("ok", "unpack", "copy"):
                    child.send_message(0xF5, 0, 0)
                    break
        time.sleep(0.3)
    else:
        raise RuntimeError("native Total Commander extraction did not produce the expected files")
    evidence = dict(application=str(args.app.name), plugin=args.plugin.name,
                    application_sha256=hashlib.sha256(args.app.read_bytes()).hexdigest(),
                    plugin_sha256=hashlib.sha256(args.plugin.read_bytes()).hexdigest(),
                    files={name: hashlib.sha256(data).hexdigest() for name, data in files.items()},
                    native_application=True, plugin_module_observed=observed_module,
                    archive_opened=True, extracted_bytes_verified=True)
    (root / "result.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps(evidence))
finally:
    snapshot()
    ImageGrab.grab().save(root / "desktop.png")
    if process.poll() is None:
        process.terminate()
        process.wait(timeout=10)
