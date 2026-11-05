import gc
import io
from pathlib import Path
import random
import subprocess
import sys
import weakref

import pytest
import uc2
from uc2._cli import executable

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(params=[4, 6])
def archive(tmp_path, request):
    source = tmp_path / "source"
    (source / "folder").mkdir(parents=True)
    files = {"notes.txt": b"hello from UC2\n" * 1500,
             "notes.txt;1": b"literal semicolon name",
             "folder/long filename.bin": random.Random(30).randbytes(110000),
             "folder/empty": b""}
    for name, value in files.items():
        (source / name).write_bytes(value)
    path = tmp_path / "archive.uc2"
    subprocess.run([str(executable()), "-qw", "-L", str(request.param), str(path),
                    "notes.txt", "notes.txt", "notes.txt;1", "folder"], cwd=source,
                   check=True, capture_output=True)
    return path, files


def test_read_versions_and_roundtrip(archive, tmp_path):
    path, files = archive
    with uc2.UC2File(path) as reader:
        assert set(files) <= set(reader.namelist())
        assert len(reader.infolist(all_versions=True)) == len(reader.infolist()) + 1
        assert reader.read("notes.txt", version=1) == files["notes.txt"]
        for name, value in files.items():
            info = reader.getinfo(name)
            assert info.file_size == len(value)
            assert reader.read(info) == value
            out = io.BytesIO()
            assert reader.copy_to(info, out) == len(value)
            assert out.getvalue() == value
        reader.verify()
        destination = tmp_path / "restored"
        reader.extractall(destination)
        for name, value in files.items():
            assert (destination / name).read_bytes() == value
        with pytest.raises(FileExistsError):
            reader.extract("notes.txt", destination)
        (destination / "notes.txt").write_bytes(b"old")
        reader.extract("notes.txt", destination, overwrite=True)
        assert (destination / "notes.txt").read_bytes() == files["notes.txt"]
    assert reader.closed
    reader.close()
    with pytest.raises(ValueError, match="closed"):
        reader.read("notes.txt")


def test_reference_archives():
    for path in (ROOT / "tests/archives").glob("*.uc2"):
        with uc2.UC2File(path) as reader:
            reader.verify()
            for info in reader.infolist():
                expected = ROOT / "tests/corpus" / info.filename.lower()
                assert reader.read(info) == expected.read_bytes()


def test_file_objects_and_unicode_archive_path(archive, tmp_path):
    path, files = archive
    renamed = tmp_path / "архив.uc2"
    renamed.write_bytes(path.read_bytes())
    with uc2.UC2File(renamed) as reader:
        assert reader.read("notes.txt") == files["notes.txt"]
    source = io.BytesIO(path.read_bytes())
    with uc2.UC2File(source) as reader:
        assert reader.read("notes.txt") == files["notes.txt"]
    assert not source.closed


def test_limits(archive, tmp_path):
    path, _ = archive
    with pytest.raises(uc2.ResourceLimitError, match="entry limit"):
        uc2.UC2File(path, max_entries=1)
    with pytest.raises(uc2.ResourceLimitError, match="memory"):
        uc2.UC2File(path, max_decoder_memory=1)
    with uc2.UC2File(path, max_read_size=10, max_total_size=10) as reader:
        with pytest.raises(uc2.ResourceLimitError):
            reader.read("notes.txt")
        with pytest.raises(uc2.ResourceLimitError):
            reader.extractall(tmp_path / "limited")
        assert not (tmp_path / "limited").exists()
        with pytest.raises(uc2.ResourceLimitError):
            reader.verify()
    with pytest.raises(ValueError):
        uc2.UC2File(path, max_read_size=-1)
    with pytest.raises(ValueError):
        uc2.UC2File(path, "w")


def test_errors_and_foreign_metadata(archive):
    path, _ = archive
    with uc2.UC2File(path) as first, uc2.UC2File(path) as second:
        with pytest.raises(ValueError, match="another archive"):
            first.read(second.getinfo("notes.txt"))
        with pytest.raises(KeyError):
            first.read("missing")
        with pytest.raises(IsADirectoryError):
            first.read("folder")
        with pytest.raises(ValueError):
            first.getinfo("notes.txt", version=-1)
    for data in (b"", b"UC2\x1a", path.read_bytes()[:24], b"not an archive"):
        with pytest.raises((uc2.BadArchiveError, OSError)):
            uc2.UC2File(io.BytesIO(data))
    with pytest.raises(TypeError):
        uc2.UC2File(io.StringIO("this is text"))


def test_callbacks_and_reentrancy(archive):
    path, files = archive
    with uc2.UC2File(path) as reader:
        class ShortWriter:
            def write(self, data):
                return len(data) - 1
        with pytest.raises(OSError, match="full block"):
            reader.copy_to("notes.txt", ShortWriter())

        class ClosingWriter:
            def write(self, data):
                reader.close()
                return len(data)
        with pytest.raises(RuntimeError, match="already in use"):
            reader.copy_to("notes.txt", ClosingWriter())
        assert reader.read("notes.txt") == files["notes.txt"]
    gc.collect()


def test_native_cli(archive):
    path, _ = archive
    result = subprocess.run([sys.executable, "-m", "uc2", "-qt", str(path)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert subprocess.check_output([str(executable()), "-h"]).startswith(b"UC2 3.0.0a4")


def test_native_cli_binary_streams(tmp_path):
    payload = bytes(range(256)) * 256 + b"\r\n\x1a\n" * 31
    ingested = tmp_path / "stream.uc2"
    subprocess.run([str(executable()), "--ingest", str(ingested)], input=payload,
                   check=True, capture_output=True)
    assert subprocess.check_output([str(executable()), "--ingest-restore", str(ingested)]) == payload
    (tmp_path / "bytes.bin").write_bytes(payload)
    archive = tmp_path / "binary.uc2"
    subprocess.run([str(executable()), "-qw", str(archive), "bytes.bin"], cwd=tmp_path,
                   check=True, capture_output=True)
    assert subprocess.check_output([str(executable()), "-qp", str(archive)]) == payload


def test_damaged_payload_preserves_destination(tmp_path):
    (tmp_path / "checksum.bin").write_bytes(bytes(range(256)) * 100)
    path = tmp_path / "damaged.uc2"
    subprocess.run([str(executable()), "-qw", str(path), "checksum.bin"], cwd=tmp_path,
                   check=True, capture_output=True)
    data = bytearray(path.read_bytes())
    directory = int.from_bytes(data[17:21], "little")
    data[directory - 16] ^= 0xFF
    path.write_bytes(data)
    destination = tmp_path / "out"
    destination.mkdir()
    target = destination / "checksum.bin"
    target.write_bytes(b"preserve this")
    with uc2.UC2File(path) as reader:
        with pytest.raises(uc2.BadArchiveError):
            reader.extract("checksum.bin", destination, overwrite=True)
    assert target.read_bytes() == b"preserve this"
    assert not list(destination.glob(".uc2-*.tmp"))


def test_links_are_not_followed(archive, tmp_path):
    path, _ = archive
    outside = tmp_path / "outside"
    outside.mkdir()
    out = tmp_path / "out"
    out.mkdir()
    try:
        (out / "folder").symlink_to(outside, target_is_directory=True)
    except OSError:
        pytest.skip("symlink creation is unavailable on this host")
    with uc2.UC2File(path) as reader:
        with pytest.raises(uc2.UnsafePathError):
            reader.extract("folder/empty", out)
        (out / "notes.txt").symlink_to(outside / "target")
        with pytest.raises(uc2.UnsafePathError):
            reader.extract("notes.txt", out, overwrite=True)
    assert not list(outside.iterdir())


@pytest.mark.parametrize("name", ["../escape", "/absolute", "C:/drive", "folder/../escape",
                                  "folder\\escape", "NUL.txt", "COM¹", "name.", "bad\x00name"])
def test_unsafe_names(name):
    from uc2._extract import validate_members
    info = uc2.UC2Info(name, 0, 0, None, 0, 0, 0, False, 0)
    with pytest.raises(uc2.UnsafePathError):
        validate_members([info])


def test_colliding_names():
    from uc2._extract import validate_members
    def info(name):
        return uc2.UC2Info(name, 0, 0, None, 0, 0, 0, False, 0)
    for names in (("A", "a"), ("a", "a/b"), ("same", "same")):
        with pytest.raises(uc2.UnsafePathError):
            validate_members([info(n) for n in names])


def test_reader_replays_damaged_inputs():
    candidates = list((ROOT / "tests/fuzz/regressions").glob("*"))
    basic = (ROOT / "tests/archives/basic.uc2").read_bytes()
    inputs = [p.read_bytes() for p in candidates if p.is_file()]
    inputs += [basic[:cut] for cut in range(0, len(basic), 73)]
    for data in inputs:
        try:
            with uc2.UC2File(io.BytesIO(data), max_entries=1000,
                             max_decoder_memory=8*1024**2, max_total_size=1024**2) as reader:
                reader.verify()
        except (OSError, ValueError):
            pass


def test_file_read_errors_are_preserved(archive):
    path, _ = archive
    class BrokenFile(io.BytesIO):
        def read(self, size=-1):
            raise OSError("simulated source read failure")
    with pytest.raises(OSError, match="simulated source read failure"):
        uc2.UC2File(BrokenFile(path.read_bytes()))


def test_negative_limits_cannot_disable_bounds(archive):
    path, _ = archive
    with uc2.UC2File(path) as reader:
        reader.max_read_size = -1
        with pytest.raises(ValueError, match="positive"):
            reader.read("notes.txt")
        reader.max_extract_size = -1
        with pytest.raises(ValueError, match="positive"):
            reader.copy_to("notes.txt", io.BytesIO())
    with pytest.raises(ValueError, match="positive"):
        uc2._native.Reader(io.BytesIO(path.read_bytes()), max_decoder_memory=-1)


def test_ambiguous_directory_records_are_rejected():
    # Rows are the decoder's metadata boundary: parent, id, directory, name,
    # size, compressed size, date, attributes, method.
    def row(name, *, parent=0, identifier=0, directory=False):
        return (parent, identifier, directory, name, 0, 0, 0, 0, 0)
    malformed = [[row(name)] for name in ("", "a/b", "a\\b")]
    malformed += [[row("a", identifier=1, directory=True), row("a")],
                  [row("a", identifier=1, directory=True),
                   row("a", identifier=2, directory=True)]]
    for rows in malformed:
        reader = uc2.UC2File.__new__(uc2.UC2File)
        with pytest.raises(uc2.BadArchiveError):
            reader._build_index(rows)


def test_gc_during_callbacks_and_unreachable_cycle(archive):
    path, files = archive
    class CollectingFile(io.BytesIO):
        def read(self, size=-1):
            gc.collect()
            return super().read(size)
    source = CollectingFile(path.read_bytes())
    reader = uc2.UC2File(source)
    source.owner = reader
    assert reader.read("notes.txt") == files["notes.txt"]
    reference = weakref.ref(source)
    del source, reader
    gc.collect()
    assert reference() is None
