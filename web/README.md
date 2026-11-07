# UC2 browser reader

The static reader lists archive paths and stored versions, verifies UC2
checksums, and downloads individual decoded files. Downloaded files also get
a SHA-256 hash in the session inventory. Archive data stays on the device.
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

Legacy Huffman, Turbo/method 80 and v3 rANS are supported. Filenames are rendered as text, and download names are reduced
to safe leaf names. The browser inventory includes raw timestamps and DOS
attributes, but does not contain arbitrary tags or original compressed data.
Unverified entries are explicitly marked.

## Build and test

CI pins Emscripten 4.0.15, from emsdk commit
`389a68bc35dcff7ebae4614e1615099dafda00d1`.

```sh
python web/build.py --emcc /path/to/emsdk/upstream/emscripten/emcc
python -m pip install . playwright==1.64.1
python -m playwright install chromium
python web/test_browser.py --dist web/dist
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
