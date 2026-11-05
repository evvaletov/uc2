import gc
import io
from datetime import datetime
from pathlib import Path
import os
import subprocess

import pytest
from uc2 import UC2File, UC2Writer, ResourceLimitError
from uc2._cli import executable
from uc2 import _native


@pytest.mark.parametrize("level", range(2, 10))
def test_roundtrip(tmp_path, level):
    archive = tmp_path / "created.uc2"
    payload = bytes(range(256)) * 300
    date = datetime(2026, 11, 5, 12, 30, 17)
    source = io.BytesIO(payload)
    with UC2Writer(archive, level=level) as writer:
        writer.mkdir("folder/empty")
        writer.writestr("folder/café.bin", b"old")
        writer.write_stream("folder/café.bin", source, date_time=date)
        writer.writestr("empty", b"")
        assert not source.closed
        assert not archive.exists()
    assert writer.closed and writer.stats["files"] == 3
    writer.close()
    with UC2File(archive) as reader:
        assert reader.read("folder/café.bin") == payload
        assert reader.read("folder/café.bin", version=1) == b"old"
        assert reader.getinfo("folder/café.bin").date_time == date.replace(second=16)
        assert reader.getinfo("folder/empty").is_dir()
        assert reader.read("empty") == b""
        reader.verify()
    subprocess.run([str(executable()), "-qt", str(archive)], check=True)


def test_tree_and_empty_archive(tmp_path):
    source = tmp_path / "input"
    (source / "nested").mkdir(parents=True)
    (source / "nested" / "a").write_bytes(b"abc")
    archive = tmp_path / "empty.uc2"
    with UC2Writer(archive):
        pass
    with UC2File(archive) as reader:
        assert reader.namelist() == []
    with UC2Writer(archive) as writer:
        writer.write(source, "tree")
    with UC2File(archive) as reader:
        assert reader.read("tree/nested/a") == b"abc"
    output = tmp_path / "directory.uc2"
    with UC2Writer(output) as writer:
        writer.mkdir("only-empty-directory")
    with UC2File(output) as reader:
        assert reader.namelist() == ["only-empty-directory"]


@pytest.mark.parametrize("mode", ["context", "abort", "destruct", "bad-name", "bad-size", "read-error"])
def test_preserve_destination(tmp_path, mode):
    archive = tmp_path / "existing.uc2"
    archive.write_bytes(b"keep")
    writer = UC2Writer(archive)
    writer.writestr("good", b"new")
    if mode == "context":
        with pytest.raises(RuntimeError), writer:
            raise RuntimeError("cancel")
    elif mode == "abort":
        writer.abort()
    elif mode == "destruct":
        del writer
        gc.collect()
    elif mode == "bad-name":
        with pytest.raises(ValueError):
            writer.writestr("../bad", b"x")
    elif mode == "bad-size":
        with pytest.raises(OSError):
            writer.write_stream("short", io.BytesIO(b"x"), size=3)
    else:
        class Broken:
            def read(self, size):
                raise RuntimeError("input failed")
        with pytest.raises(RuntimeError, match="input failed"):
            writer.write_stream("bad", Broken())
    assert archive.read_bytes() == b"keep"
    assert not list(tmp_path.glob(".*.tmp"))


@pytest.mark.parametrize("name", ["", "/a", "a/", "a//b", "a/../b", "a\\b", "a:b", "nul\0x", "smile😀", "x" * 256, "é" * 150])
def test_names(tmp_path, name):
    writer = UC2Writer(tmp_path / "bad.uc2")
    with pytest.raises((ValueError, UnicodeError)):
        writer.writestr(name, b"bad")
    assert writer.closed and not (tmp_path / "bad.uc2").exists()


def test_long_name_and_depth(tmp_path):
    archive = tmp_path / "names.uc2"
    name = "x" * 255
    with UC2Writer(archive) as writer:
        writer.writestr(name, b"ok")
        writer.writestr("/".join(["d"] * 64 + ["f"]), b"deep")
    with UC2File(archive) as reader:
        assert reader.read(name) == b"ok"
        assert reader.read("/".join(["d"] * 64 + ["f"])) == b"deep"
    writer = UC2Writer(tmp_path / "too-deep.uc2")
    with pytest.raises(ValueError):
        writer.mkdir("/".join(["d"] * 65))


def test_limits_and_short_reads(tmp_path):
    class Short:
        def __init__(self):
            self.data = io.BytesIO(b"x" * 10000)
        def read(self, size):
            assert size <= 65536
            return self.data.read(min(size, 7))
    archive = tmp_path / "limits.uc2"
    with UC2Writer(archive, max_total_size=10000) as writer:
        writer.write_stream("x", Short(), size=10000)
    with UC2File(archive) as reader:
        assert reader.read("x") == b"x" * 10000
    for kwargs in ({"max_total_size": 2}, {"max_entries": 1}):
        writer = UC2Writer(archive, **kwargs)
        with pytest.raises(ResourceLimitError):
            writer.write_stream("a/b", io.BytesIO(b"xxx"))
        assert writer.closed
    with UC2File(archive) as reader:
        reader.verify()
    writer = UC2Writer(archive)
    with pytest.raises(ResourceLimitError):
        writer.write_stream("x", io.BytesIO(b"too long"), size=2)


def test_reentrant_and_invalid_stream(tmp_path):
    for stream in [io.StringIO("text"), type("Oversized", (), {"read": lambda self, n: b"x" * (n + 1)})()]:
        writer = UC2Writer(tmp_path / "bad.uc2")
        with pytest.raises(TypeError):
            writer.write_stream("x", stream)
        assert writer.closed
    writer = UC2Writer(tmp_path / "bad.uc2")
    class Reentrant:
        def read(self, n):
            writer.close()
    with pytest.raises(RuntimeError, match="already in use"):
        writer.write_stream("x", Reentrant())
    assert writer.closed


def test_atomic_output_failures(tmp_path, monkeypatch):
    archive = tmp_path / "existing.uc2"
    archive.write_bytes(b"keep")
    writer = UC2Writer(archive)
    writer.writestr("x", b"new")
    def failed(*args):
        raise OSError("replace failed")
    monkeypatch.setattr(os, "replace", failed)
    with pytest.raises(OSError, match="replace failed"):
        writer.close()
    assert writer.closed and archive.read_bytes() == b"keep"
    assert not list(tmp_path.glob(".*.tmp"))


def test_native_output_callback_and_state():
    writer = _native.Writer()
    writer.add_file(0, "a", 0, 0x20, 3, io.BytesIO(b"abc"))
    class ShortWrite:
        def write(self, data):
            return len(data) - 1
    with pytest.raises(OSError, match="full buffer"):
        writer.finish(ShortWrite())
    with pytest.raises(OSError):
        writer.finish(io.BytesIO())
    writer.abort()
    writer.abort()


@pytest.mark.skipif(os.name == "nt", reason="symlink creation requires Windows privileges")
def test_symlink_and_cli_preservation(tmp_path):
    source = tmp_path / "input"
    source.mkdir()
    (source / "loop").symlink_to(source, target_is_directory=True)
    archive = tmp_path / "old.uc2"
    archive.write_bytes(b"keep")
    with pytest.raises(ValueError, match="symlinks"), UC2Writer(archive) as writer:
        writer.write(source)
    result = subprocess.run([str(executable()), "-qw", str(archive), str(source)], capture_output=True)
    assert result.returncode != 0 and archive.read_bytes() == b"keep"
    assert not list(tmp_path.glob("*.tmp-*"))
