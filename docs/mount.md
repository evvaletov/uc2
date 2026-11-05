# Mount a UC2 archive on Linux

Install the Python package with its optional FUSE binding and your Linux
distribution's libfuse2 runtime. You also need access to `/dev/fuse` and a
working `fusermount` helper.

```sh
python -m pip install --pre 'uc2[mount]'
mkdir archived
uc2-mount backup.uc2 archived
```

The command stays in the foreground. In another terminal:

```sh
ls archived/files
cat archived/files/notes.txt
diff archived/versions/1/notes.txt archived/versions/0/notes.txt
fusermount -u archived
```

`files/` contains the newest version of every member. `versions/0/` contains
the same newest versions, `versions/1/` contains the preceding versions where
present, and so on. Separate directories preserve literal names such as
`notes.txt;1`. Archive names cannot collide with these two mount directories.

Files have mode 0444 and directories 0555. Writes, execution, devices, and
setuid behavior are disabled. Mounting does not alter the archive or extract
every entry. Metadata is indexed at startup; the first open of a file decodes
that entire file into a private temporary file and verifies its UC2 checksum.
Subsequent reads seek within that cached file. Checksums detect accidental
corruption; they do not authenticate the archive.

| Option | Default | Limit |
| --- | --- | --- |
| `--cache-mib` | 512 | Total decoded bytes on disk, including the file being decoded |
| `--cache-entries` | 128 | Number of decoded temporary files |
| `--decoder-mib` | 64 | Native decoder allocations, including master dictionaries |
| `--max-entries` | 100000 | Archive directory entries |
| `--max-nodes` | 200000 | Filesystem nodes across both views and their parent directories |
| `--cache-dir` | System temporary directory | Location of private decoded files |

Least recently used files are evicted before another file is decoded. An entry
larger than the cache budget fails with `EFBIG`; raise `--cache-mib` to read it.
Corrupt entries fail with `EIO` and never enter the cache. A decoder allocation
limit returns `ENOMEM`. Reads are serialized and each decode uses a fresh
reader, so master caches cannot accumulate across files. The kernel file cache
is disabled, and each userspace read buffer is at most 1 MiB.

The decoder budget excludes Python metadata, interpreter overhead, and the
operating system's temporary-file cache. Node and entry limits bound index
growth; this is not a whole-process memory limit or a CPU-time sandbox. Keep
the source unchanged. A detected size or timestamp change returns `ESTALE`.
Replacing the archive pathname does not switch the open mount to the new file.

The mountpoint must be an existing empty directory. Keep the temporary-file
directory outside it. Unmount before removing the mountpoint. Normal unmount
closes the cache and removes its private directory; a forced process kill can
leave an empty `uc2-mount-*` directory behind.

The filesystem currently targets Linux. The optional
[fusepy binding](https://github.com/fusepy/fusepy) uses libfuse2. Installing a
libfuse3-only runtime does not satisfy that dependency. The Python archive API
and CLI remain available on the other supported platforms.
