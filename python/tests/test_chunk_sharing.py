"""Shared payloads preserve named files; malformed recipes fail explicitly."""
import hashlib
import io
import random
import struct
import subprocess
import tarfile

import pytest
from uc2 import UC2File, UC2Writer, BadArchiveError
from uc2._cli import executable

MAGIC = b"UC2CHN1\0"
HEADER, RECORD = 88, 48


@pytest.mark.parametrize("level", range(2, 10))
def test_named_chunks_versions_and_codecs(tmp_path, level):
    base = random.Random(7).randbytes(180000) + b"ABCD" * 22000
    changed = b"prefix change" + base[:160000] + b"X" + base[160000:]
    path = tmp_path / "shared.uc2"
    with UC2Writer(path, level=level, chunked=True) as writer:
        writer.writestr("folder/café.bin", base)
        writer.writestr("folder/café.bin", changed)
        writer.writestr("empty", b"")
        writer.writestr("copy", base)
    with UC2File(io.BytesIO(path.read_bytes())) as archive:
        files = [i for i in archive.infolist(all_versions=True) if not i.is_dir()]
        assert len(files) == 4
        assert all(i.method == 11 for i in files)
        assert archive.read("folder/café.bin", version=1) == base
        assert archive.read("folder/café.bin") == changed
        assert archive.read("copy") == base
        assert archive.read("empty") == b""
    assert writer.stats["chunk_references"] > writer.stats["unique_chunks"]
    assert writer.stats["shared_bytes"] > len(base)
    assert writer.stats["masters"] == 0


def test_partial_edits_share_beyond_dictionary_window(tmp_path):
    base = random.Random(341).randbytes(512 * 1024)
    changed = base[:200000] + b"an inserted line\n" + base[200000:]
    sizes = {}
    for chunked in (False, True):
        path = tmp_path / f"{chunked}.uc2"
        with UC2Writer(path, chunked=chunked) as writer:
            writer.writestr("old.bin", base)
            writer.writestr("new.bin", changed)
        sizes[chunked] = path.stat().st_size
        with UC2File(path) as archive:
            assert archive.namelist() == ["old.bin", "new.bin"]
            assert archive.read("old.bin") == base
            assert archive.read("new.bin") == changed
        if chunked:
            assert writer.stats["shared_bytes"] > 400000
        else:
            assert writer.stats["chunk_references"] == 0
    assert sizes[True] < sizes[False] * 0.75


def recipe_digest(data, start):
    count = struct.unpack_from("<I", data, start + 8)[0]
    data[start + 56:start + 88] = hashlib.sha256(
        data[start:start + 56] + data[start + HEADER:start + HEADER + count * RECORD]
    ).digest()


@pytest.mark.parametrize("fault", [
    "magic", "flags", "count", "size", "table_size", "table_digest",
    "offset_header", "offset_self", "offset_overflow", "zero_size", "large_size",
    "large_compressed", "method", "chunk_checksum", "chunk_digest", "file_digest",
    "payload_same_xor", "reordered",
])
def test_bad_chunk_recipes(tmp_path, fault):
    payload = random.Random(91).randbytes(70001)
    path = tmp_path / "bad.uc2"
    with UC2Writer(path, chunked=True) as writer:
        writer.writestr("data", payload)
    data = bytearray(path.read_bytes())
    start = data.index(MAGIC)
    row = start + HEADER
    value = {
        "flags": (start + 20, 1), "count": (start + 8, 1048577),
        "size": (start + 12, 70002), "table_size": (start + 16, 0),
        "offset_header": (row, 0), "offset_self": (row, start),
        "offset_overflow": (row, 0xFFFFFFFF), "zero_size": (row + 8, 0),
        "large_size": (row + 8, 32769), "large_compressed": (row + 4, 65537),
    }
    if fault in value:
        struct.pack_into("<I", data, *value[fault])
    elif fault == "method":
        struct.pack_into("<H", data, row + 12, 11)  # nested recipes are forbidden
    elif fault == "magic":
        data[start] ^= 1
    elif fault == "table_digest":
        data[start + 56] ^= 1
    elif fault == "chunk_checksum":
        data[row + 14] ^= 1
    elif fault == "chunk_digest":
        data[row + 16] ^= 1
    elif fault == "file_digest":
        data[start + 24] ^= 1
    elif fault == "payload_same_xor":
        assert struct.unpack_from("<H", data, row + 12)[0] == 0
        offset = struct.unpack_from("<I", data, row)[0]
        data[offset] ^= 1
        data[offset + 2] ^= 1  # preserve the weak historical XOR checksum
    elif fault == "reordered":
        first = bytes(data[row:row + RECORD])
        data[row:row + RECORD] = data[row + RECORD:row + 2 * RECORD]
        data[row + RECORD:row + 2 * RECORD] = first
    if fault not in {"magic", "count", "table_digest"}:
        recipe_digest(data, start)
    path.write_bytes(data)
    with pytest.raises(BadArchiveError):
        with UC2File(path) as archive:
            archive.read("data")


@pytest.mark.parametrize("level", [4, 8])
def test_damaged_compressed_chunks_preserve_destination(tmp_path, level):
    path = tmp_path / "compressed.uc2"
    payload = b"compressible data\n" * 5000
    with UC2Writer(path, level=level, chunked=True) as writer:
        writer.writestr("data", payload)
    data = bytearray(path.read_bytes())
    start = data.index(MAGIC)
    row = start + HEADER
    assert struct.unpack_from("<H", data, row + 12)[0] != 0
    offset = struct.unpack_from("<I", data, row)[0]
    data[offset] ^= 0xFF
    path.write_bytes(data)
    target = tmp_path / "output"
    target.mkdir()
    (target / "data").write_bytes(b"keep existing")
    with UC2File(path) as archive:
        with pytest.raises(BadArchiveError):
            archive.extractall(target, overwrite=True)
    assert (target / "data").read_bytes() == b"keep existing"


def test_chunked_cli_tar_and_stream(tmp_path):
    base = random.Random(71).randbytes(180000)
    changed = base[:70000] + b"new" + base[70000:]
    tar = io.BytesIO()
    with tarfile.open(fileobj=tar, mode="w") as archive:
        for name, data in [("old", base), ("new", changed)]:
            info = tarfile.TarInfo(name)
            info.size = len(data)
            archive.addfile(info, io.BytesIO(data))
    path = tmp_path / "tar.uc2"
    subprocess.run([executable(), "--chunked", "--ingest-tar", str(path)],
                   input=tar.getvalue(), check=True, capture_output=True)
    with UC2File(path) as archive:
        assert archive.namelist() == ["old", "new"]
        assert archive.read("old") == base
        assert archive.read("new") == changed
    stream = base + base + changed
    path = tmp_path / "stream.uc2"
    subprocess.run([executable(), "--chunked", "--ingest", str(path)],
                   input=stream, check=True, capture_output=True)
    with UC2File(path) as archive:
        assert archive.namelist() == ["data"]
        assert archive.read("data") == stream
    result = subprocess.run([executable(), "--ingest-restore", str(path)],
                            check=True, capture_output=True)
    assert result.stdout == stream


def test_chunked_option_validation(tmp_path):
    with pytest.raises(TypeError, match="chunked"):
        UC2Writer(tmp_path / "invalid.uc2", chunked="yes")
    with UC2Writer(tmp_path / "empty.uc2", chunked=True):
        pass
    with UC2File(tmp_path / "empty.uc2") as archive:
        assert archive.namelist() == []


@pytest.mark.parametrize("level", [4, 8])
@pytest.mark.parametrize("delta", [-2, 2])
def test_compressed_chunk_must_consume_exact_word_range(tmp_path, level, delta):
    path = tmp_path / "word-bound.uc2"
    with UC2Writer(path, level=level, chunked=True) as writer:
        writer.writestr("data", b"compressed payload\n" * 12000)
    data = bytearray(path.read_bytes())
    start = data.index(MAGIC)
    row = start + HEADER
    assert struct.unpack_from("<H", data, row + 12)[0] != 0
    size = struct.unpack_from("<I", data, row + 4)[0]
    struct.pack_into("<I", data, row + 4, size + delta)
    recipe_digest(data, start)
    path.write_bytes(data)
    with pytest.raises(BadArchiveError):
        with UC2File(path) as archive:
            archive.read("data")


@pytest.mark.parametrize("chunked", [False, True])
def test_many_repeated_version_names_are_not_a_ratio_bomb(tmp_path, chunked):
    path = tmp_path / "many-versions.uc2"
    name = "a" * 240
    with UC2Writer(path, chunked=chunked) as writer:
        for _ in range(4000):
            writer.writestr(name, b"a")
    with UC2File(path) as archive:
        assert len(archive.infolist(all_versions=True)) == 4000
        assert archive.read(name) == archive.read(name, version=3999) == b"a"
