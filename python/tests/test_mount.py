from concurrent.futures import ThreadPoolExecutor
import errno
import os
import random
import subprocess
import sys

import pytest
from uc2 import BadArchiveError, ResourceLimitError
from uc2._cli import executable
from uc2._mount import ArchiveFS

pytestmark = pytest.mark.skipif(os.name != "posix", reason="POSIX mount backend")


@pytest.fixture(params=[4, 6])
def mounted_archive(tmp_path, request):
    source = tmp_path / "source"
    (source / "nested").mkdir(parents=True)
    files = {"first": random.Random(5).randbytes(40_000),
             "nested/second": random.Random(6).randbytes(40_000),
             "first;1": b"literal suffix", "empty": b""}
    for name, data in files.items():
        (source / name).write_bytes(data)
    archive = tmp_path / "fixture.uc2"
    subprocess.run([str(executable()), "-qw", "-L", str(request.param), str(archive),
                    "first", "first", "first;1", "nested", "empty"],
                   cwd=source, check=True, capture_output=True)
    return archive, files


def test_reads_versions_and_eviction(mounted_archive):
    path, files = mounted_archive
    fs = ArchiveFS(path, cache_bytes=50_000, cache_entries=2)
    try:
        assert fs.cached_bytes == 0
        assert fs.readdir("/") == [".", "..", "files", "versions"]
        for name, data in files.items():
            assert fs.read("/files/" + name, len(data) + 10, 0) == data
            assert fs.cached_bytes <= 50_000
            assert len(fs.cache) <= 2
        assert fs.read("/versions/1/first", 300, 101) == files["first"][101:401]
        assert fs.read("/files/first;1", 100, 0) == files["first;1"]
        assert fs.read("/files/first", 100, len(files["first"])) == b""
        assert fs.getattr("/files/first")["st_size"] == len(files["first"])
        with pytest.raises(OSError) as error:
            fs.open("/files/first", os.O_WRONLY)
        assert error.value.errno == errno.EROFS
        with pytest.raises(OSError) as error:
            fs.read("/missing", 1, 0)
        assert error.value.errno == errno.ENOENT
    finally:
        temporary = fs.temporary.name
        fs.close()
    assert not os.path.exists(temporary)


def test_concurrent_reads_bounded_cache(mounted_archive):
    path, files = mounted_archive
    fs = ArchiveFS(path, cache_bytes=40_000, cache_entries=1)
    try:
        def read(i):
            name = "first" if i % 2 else "nested/second"
            return fs.read("/files/" + name, 1000, i), files[name][i:i+1000]
        with ThreadPoolExecutor(max_workers=4) as pool:
            assert all(actual == expected for actual, expected in pool.map(read, range(20)))
        assert fs.cached_bytes == 40_000
        assert len(fs.cache) == 1
    finally:
        fs.close()


def test_limits_and_source_changes(mounted_archive):
    path, _ = mounted_archive
    fs = ArchiveFS(path, cache_bytes=10)
    try:
        with pytest.raises(OSError) as error:
            fs.open("/files/first", os.O_RDONLY)
        assert error.value.errno == errno.EFBIG
        assert fs.cached_bytes == 0
        with path.open("ab") as output:
            output.write(b"changed")
        with pytest.raises(OSError) as error:
            fs.getattr("/")
        assert error.value.errno == errno.ESTALE
    finally:
        fs.close()
    with pytest.raises(ResourceLimitError):
        ArchiveFS(path, max_nodes=3)
    with pytest.raises(ResourceLimitError):
        ArchiveFS(path, decoder_bytes=1)


def test_corrupt_payload_never_cached(mounted_archive):
    path, _ = mounted_archive
    data = bytearray(path.read_bytes())
    # The first compressed master follows the fixed file and extended headers.
    data[29:40] = b"\xff" * 11
    path.write_bytes(data)
    fs = ArchiveFS(path)
    try:
        with pytest.raises(BadArchiveError):
            fs.read("/files/first", 100, 0)
        assert fs.cached_bytes == 0
        assert not fs.cache
        assert not os.listdir(fs.temporary.name)
    finally:
        fs.close()
