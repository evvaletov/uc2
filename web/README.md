# UC2 browser reader

The static reader lists archive paths and stored versions, verifies UC2
checksums, and downloads individual decoded files or a ZIP of selected file
versions. Downloaded files also get a SHA-256 hash in the session inventory.
Archive data stays on the device.
There are no analytics or upload requests.

The C decoder runs in a dedicated worker. Browser `Blob` reads use Emscripten
WORKERFS, so opening an archive does not copy the whole input into WebAssembly
memory. SuperMaster data is compiled from generated C.

Limits are 256 MiB of input, 10,000 entries, 64 MiB of decoder allocation,
64 MiB per downloaded/verified file, and 1 GiB total decoded content in a
verification operation. WebAssembly memory is capped at 256 MiB. Metadata and
download buffers also use browser memory. The worker stops after 30 seconds
without progress or five minutes in one operation. Completed verification
results remain available after a timeout. Cancel terminates the worker and
clears the archive. Reopen after cancellation or a timeout. Use `uc2-preserve`
for larger collections.

Legacy Huffman, Turbo/method 80, v3 rANS and a11 shared chunks are supported. Filenames are rendered as text, and download names are reduced
to safe leaf names. The browser inventory includes raw timestamps and DOS
attributes, but does not contain arbitrary tags or original compressed data.
Unverified entries are explicitly marked.

## Selected ZIP export

Check individual file versions or use **Select matching files** to include all
files matching the current search and version filter. Changing the filter keeps
the selection; **Clear selection** resets it. **Save selected ZIP** decodes and
checks every selected file before offering a download. Failure, cancellation
or timeout produces no partial ZIP. Completed checks remain in the session
inventory after a failure or timeout.

The ZIP uses uncompressed STORE entries with CRC32 and portable numbered names.
Its `inventory.json` maps each output name to the original path, version, DOS
timestamp, attributes, method, size and SHA-256. Empty files are included;
directories are not selectable. Keep the original UC2 archive for metadata and
compressed data outside this export contract. A ZIP holds at most 1,000 file
versions and 128 MiB including its inventory and headers; the inventory is
limited to 16 MiB. These are output limits, not total browser RAM limits.

## Offline use

Open the hosted reader while connected, choose **Save reader offline**, and
wait for confirmation. Reopen that URL to use it without a connection. Where
the browser offers an installation prompt, **Install reader** adds a standalone
shortcut; otherwise bookmark the page. Installation UI varies by browser and OS.

Only the reader, sample, build information and license files are cached. User
archives, decoded files and inventories are not stored in the offline cache.
Each cached asset is checked against the build's SHA-256 digest. Updates wait
until all reader tabs close, so an open page keeps its matching decoder. A
partial deployment cannot replace the working cache. If browser storage is
cleared or evicted, save the reader again while connected. Offline storage is
not an archive backup. HTTPS or localhost is required.

## Build and test

CI pins Emscripten 4.0.15, from emsdk commit
`389a68bc35dcff7ebae4614e1615099dafda00d1`.

```sh
python web/build.py --emcc /path/to/emsdk/upstream/emscripten/emcc
python -m pip install . playwright==1.64.1
python -m playwright install --with-deps chromium firefox webkit
python web/test_browser.py --dist web/dist --engine chromium
python web/test_browser.py --dist web/dist --engine firefox
python web/test_browser.py --dist web/dist --engine webkit
python -m http.server 8000 --directory web/dist
```

Open `http://localhost:8000`. Serve over HTTPS when hosted. The build is static;
no server-side archive processing is required. The Docs workflow deploys the
tested reader at `https://evvaletov.github.io/uc2/reader/` alongside Sphinx.
`BUILD-INFO.json` records the source commit and compiler version. Source is in
this repository, including all build instructions and the original licenses.

The UI tests use original-encoder archives and modern rANS fixtures, check
downloaded bytes, stored versions, checksums, inventory hashes, malformed
inputs, cancellation, mobile layout and the absence of upload requests.
All three engines also check ZIP bytes/CRCs/hashes, selection limits, offline
decoding and export, cache repair, waiting updates and failed integrity checks.
