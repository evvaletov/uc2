#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the compiled reader and its UI in an isolated headless browser."""
import argparse
from functools import partial
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import re
import tempfile
from threading import Thread
from urllib.parse import urlsplit
import zipfile

from playwright.sync_api import sync_playwright, expect
from uc2 import UC2File, UC2Writer

ROOT = Path(__file__).resolve().parents[1]


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        override = getattr(self.server, 'overrides', {}).get(urlsplit(self.path).path)
        if override is not None:
            self.send_response(200)
            self.send_header('Content-Type', 'application/javascript' if self.path.endswith('.js') else 'text/html')
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(override)
        else:
            super().do_GET()


def check_zip(path, archive, wanted=None):
    """Check ZIP CRCs, portable unique names, metadata and decoded bytes independently."""
    with zipfile.ZipFile(path) as exported, UC2File(archive) as reference:
        assert exported.testzip() is None
        report = json.loads(exported.read('inventory.json'))
        assert report['schema'] == 'uc2-browser-export-1'
        files = report['files']
        expected = [info for info in reference.infolist(all_versions=True) if not info.is_dir()]
        if wanted is not None:
            expected = [info for info in expected if (info.filename, info.version) in wanted]
        assert len(files) == len(expected), (files, expected)
        assert len(exported.namelist()) == len(files) + 1
        assert len(set(name.lower() for name in exported.namelist())) == len(files) + 1
        for row, info in zip(files, expected):
            assert (row['path'], row['version']) == (info.filename, info.version)
            assert re.fullmatch(r'files/\d+-v\d+-[A-Za-z0-9._-]+', row['output'])
            data = exported.read(row['output'])
            assert data == reference.read(info), row['path']
            assert row['size'] == len(data) and row['verified'] is True
            assert row['sha256'] == hashlib.sha256(data).hexdigest()
            assert exported.getinfo(row['output']).compress_type == zipfile.ZIP_STORED


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dist', type=Path, default=ROOT / 'web/dist')
    parser.add_argument('--browser')
    parser.add_argument('--engine', choices=('chromium', 'firefox', 'webkit'), default='chromium')
    parser.add_argument('--screenshot', type=Path)
    args = parser.parse_args()
    server = ThreadingHTTPServer(('127.0.0.1', 0), partial(QuietHandler, directory=str(args.dist)))
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = f'http://127.0.0.1:{server.server_port}/'
    try:
        with tempfile.TemporaryDirectory(prefix='uc2-browser-test-') as temporary, sync_playwright() as pw:
            scratch = Path(temporary)
            browser = getattr(pw, args.engine).launch(headless=True, executable_path=args.browser)
            context = browser.new_context(accept_downloads=True)
            page = context.new_page()
            errors, requests, downloads = [], [], []
            page.on('pageerror', lambda error: errors.append(str(error)))
            context.on('request', lambda request: requests.append((request.method, request.url)))
            page.on('download', lambda download: downloads.append(download))
            page.goto(url)
            page.get_by_role('button', name='Try the sample archive').click()
            expect(page.locator('#workspace')).to_be_visible(timeout=30000)
            expect(page.locator('#archive-name')).to_have_text('sample.uc2')
            page.get_by_role('button', name='Verify all versions').click()
            expect(page.locator('#status')).to_contain_text('passed UC2 checksum', timeout=30000)
            if args.screenshot:
                page.screenshot(path=str(args.screenshot), full_page=True)

            for fixture in sorted((ROOT / 'tests/archives').glob('*.uc2')):
                page.locator('#archive').set_input_files(fixture)
                expect(page.locator('#archive-name')).to_have_text(fixture.name, timeout=30000)
                page.get_by_role('button', name='Verify all versions').click()
                expect(page.locator('#status')).to_contain_text('passed UC2 checksum', timeout=30000)
                with UC2File(fixture) as reference:
                    for info in reference.infolist():
                        page.locator('#entries').get_by_role('button', name=info.filename, exact=True).click()
                        with page.expect_download() as downloaded:
                            page.get_by_role('button', name='Save verified file').click()
                        target = scratch / 'download'
                        downloaded.value.save_as(target)
                        assert target.read_bytes() == reference.read(info), info.filename

            modern = scratch / 'versions.uc2'
            with UC2Writer(modern, level=8) as writer:
                writer.writestr('folder/café.txt', b'old version')
                writer.writestr('folder/café.txt', b'new version')
                writer.writestr('<script>alert(1).txt', b'plain text filename')
                writer.mkdir('empty-directory')
                writer.writestr('empty', b'')
                writer.writestr('CON.txt', b'reserved filename')
            page.locator('#archive').set_input_files(modern)
            expect(page.locator('#archive-name')).to_have_text(modern.name, timeout=30000)
            page.locator('#versions').check()
            expect(page.locator('#entries button').filter(has_text='folder/café.txt')).to_have_count(2)
            page.locator('#entries button').filter(has_text='folder/café.txt').first.click()
            with page.expect_download() as downloaded:
                page.get_by_role('button', name='Save verified file').click()
            assert downloaded.value.suggested_filename == 'v1-café.txt'
            downloaded.value.save_as(scratch / 'old')
            assert (scratch / 'old').read_bytes() == b'old version'
            page.get_by_role('button', name='Verify all versions').click()
            expect(page.locator('#status')).to_contain_text('All 5 stored files passed', timeout=30000)
            with page.expect_download() as downloaded:
                page.get_by_role('button', name='Save inventory').click()
            downloaded.value.save_as(scratch / 'inventory.json')
            inventory = json.loads((scratch / 'inventory.json').read_text())
            old = next(e for e in inventory['entries'] if e['path'] == 'folder/café.txt' and e['version'] == 1)
            assert old['sha256'] == hashlib.sha256(b'old version').hexdigest()
            page.get_by_role('button', name='CON.txt', exact=True).click()
            with page.expect_download() as downloaded:
                page.get_by_role('button', name='Save verified file').click()
            assert downloaded.value.suggested_filename == 'entry-CON.txt'
            # Selection survives filtering. Older versions have separate ZIP
            # paths, and the exported inventory maps those paths back exactly.
            page.get_by_role('button', name='Select matching files').click()
            expect(page.locator('#selection-count')).to_contain_text('5 file versions')
            page.locator('#versions').uncheck()
            page.locator('#search').fill('café')
            expect(page.locator('#entries tr')).to_have_count(1)
            expect(page.locator('#selection-count')).to_contain_text('5 file versions')
            with page.expect_download() as downloaded:
                page.get_by_role('button', name='Save selected ZIP').click()
            downloaded.value.save_as(scratch / 'all.zip')
            check_zip(scratch / 'all.zip', modern)
            page.get_by_role('button', name='Clear selection').click()
            expect(page.locator('#download-zip')).to_be_disabled()
            page.get_by_role('button', name='Select matching files').click()
            with page.expect_download() as downloaded:
                page.get_by_role('button', name='Save selected ZIP').click()
            downloaded.value.save_as(scratch / 'latest.zip')
            check_zip(scratch / 'latest.zip', modern, {('folder/café.txt', 0)})
            page.locator('#versions').check()
            page.locator('#search').fill('café')
            expect(page.locator('#entries tr')).to_have_count(2)
            for font in ('system-ui', 'Arial', 'monospace'):
                page.evaluate('(font) => document.documentElement.style.fontFamily = font', font)
                for width in (320, 390, 800):
                    page.set_viewport_size({'width': width, 'height': 844})
                    assert page.evaluate('document.documentElement.scrollWidth <= innerWidth'), (font, width)
            page.evaluate("document.documentElement.style.fontFamily = ''")

            # A stalled worker times out without a false checksum failure or a
            # fresh empty worker claiming successful verification of stale rows.
            page.clock.install()
            page.evaluate('worker.terminate()')
            page.get_by_role('button', name='Verify all versions').click()
            page.clock.fast_forward(30001)
            expect(page.locator('#status')).to_contain_text('Time limit reached')
            expect(page.locator('#verify')).to_be_disabled()
            expect(page.locator('#download')).to_be_disabled()
            with page.expect_download() as downloaded:
                page.get_by_role('button', name='Save inventory').click()
            downloaded.value.save_as(scratch / 'after-timeout.json')
            after = json.loads((scratch / 'after-timeout.json').read_text())
            assert all(e['check']['verified'] for e in after['entries'] if not e['directory'])
            page.clock.resume()
            page.locator('#archive').set_input_files(modern)
            expect(page.locator('#verify')).to_be_enabled(timeout=30000)

            # A bad checksum never produces a partial ZIP or a false success.
            page.locator('#archive').set_input_files(ROOT / 'tests/fuzz/regressions/turbo-empty-checksum.uc2')
            expect(page.locator('#workspace')).to_be_visible(timeout=30000)
            page.get_by_role('button', name='Select matching files').click()
            before = len(downloads)
            page.get_by_role('button', name='Save selected ZIP').click()
            expect(page.locator('#status')).to_contain_text('No ZIP was saved', timeout=30000)
            assert len(downloads) == before
            page.locator('#archive').set_input_files(modern)
            expect(page.locator('#archive-name')).to_have_text(modern.name, timeout=30000)
            page.get_by_role('button', name='Select matching files').click()
            # Terminate in the same event-loop turn, before any reply can save.
            page.evaluate("document.querySelector('#download-zip').click(); document.querySelector('#cancel').click()")
            expect(page.locator('#status')).to_contain_text('Cancelled')
            assert len(downloads) == before

            many = scratch / 'many.uc2'
            with UC2Writer(many, level=8) as writer:
                for i in range(1001):
                    writer.writestr(f'file-{i:04d}', b'')
            page.locator('#archive').set_input_files(many)
            expect(page.locator('#archive-name')).to_have_text(many.name, timeout=30000)
            page.get_by_role('button', name='Select matching files').click()
            page.get_by_role('button', name='Save selected ZIP').click()
            expect(page.locator('#status')).to_contain_text('between 1 and 1,000')
            assert len(downloads) == before

            broken = scratch / 'broken.uc2'
            broken.write_bytes(b'not an archive')
            page.locator('#archive').set_input_files(broken)
            expect(page.locator('#status')).to_have_class('error', timeout=30000)
            expect(page.locator('#workspace')).to_be_hidden()
            page.locator('#archive').set_input_files(ROOT / 'tests/fuzz/regressions/len0-match-underflow.uc2')
            expect(page.locator('#cancel')).to_be_hidden(timeout=35000)
            if page.locator('#workspace').is_visible():
                page.get_by_role('button', name='Verify all versions').click()
                expect(page.locator('#cancel')).to_be_hidden(timeout=35000)
            # Cancel terminates the worker; reopening uses a clean decoder.
            page.evaluate("openArchive(new File([new Uint8Array([0])], 'cancel.uc2')); document.querySelector('#cancel').click()")
            expect(page.locator('#status')).to_contain_text('Cancelled')
            page.locator('#archive').set_input_files(ROOT / 'tests/archives/single.uc2')
            expect(page.locator('#workspace')).to_be_visible(timeout=30000)

            # Explicit offline installation and cache repair; archive contents
            # never enter CacheStorage. Navigate offline to acquire control.
            page.get_by_role('button', name='Save reader offline').click()
            expect(page.locator('#offline-status')).to_contain_text('Reader saved for offline use', timeout=120000)
            context.set_offline(True)
            page.reload()
            expect(page.locator('#offline-status')).to_contain_text('Reader saved for offline use', timeout=30000)
            page.get_by_role('button', name='Try the sample archive').click()
            expect(page.locator('#workspace')).to_be_visible(timeout=30000)
            for fixture in (modern, ROOT / 'tests/archives/turbo0.uc2', ROOT / 'tests/archives/turbo1.uc2'):
                page.locator('#archive').set_input_files(fixture)
                expect(page.locator('#archive-name')).to_have_text(fixture.name, timeout=30000)
                page.locator('#versions').check()
                page.get_by_role('button', name='Select matching files').click()
                with page.expect_download() as downloaded:
                    page.get_by_role('button', name='Save selected ZIP').click()
                downloaded.value.save_as(scratch / 'offline.zip')
                check_zip(scratch / 'offline.zip', fixture)
            cached = page.evaluate('''async () => {
                const names = await caches.keys();
                return await Promise.all(names.map(async name => ({name, urls: (await (await caches.open(name)).keys()).map(r => r.url)})));
            }''')
            assert len(cached) == 1
            sw = (args.dist / 'sw.js').read_text()
            assets = json.loads(re.search(r'const ASSETS = (.*);', sw)[1])
            assert set(cached[0]['urls']) == {url + name for name in assets}
            assert not any(fixture.name in target for target in cached[0]['urls'])
            context.set_offline(False)
            page.evaluate("async () => { const c = await caches.open((await caches.keys())[0]); await c.delete(new URL('icon-192.png', location.href).href); }")
            server.overrides = {'/icon-192.png': b'wrong build'}
            assert page.evaluate("fetch('icon-192.png').then(() => false, () => true)")
            server.overrides = {}
            page.reload()
            expect(page.locator('#offline-status')).to_contain_text('incomplete')
            page.get_by_role('button', name='Save reader offline').click()
            expect(page.locator('#offline-status')).to_contain_text('Reader saved for offline use', timeout=120000)

            # A partial/inconsistent deployment fails integrity checking and
            # leaves the working offline version intact. A complete update
            # waits for old tabs to close; no mixed decoder/app version.
            old_version = page.locator('meta[name="uc2-build"]').get_attribute('content')
            new_version = 'test-' + old_version
            index = (args.dist / 'index.html').read_text()
            next_index = index.replace(old_version, new_version).encode()
            next_sw = sw.replace(old_version, new_version).replace(assets['index.html'], hashlib.sha256(next_index).hexdigest())
            server.overrides = {'/sw.js': next_sw.encode()}
            result = page.evaluate('''async () => {
                const r = await navigator.serviceWorker.getRegistration();
                const failed = new Promise(resolve => r.addEventListener('updatefound', () => {
                    const w = r.installing;
                    w.addEventListener('statechange', () => { if (w.state === 'redundant') resolve(true); });
                }, {once:true}));
                await r.update(); return await Promise.race([failed, new Promise((_, reject) => setTimeout(() => reject(new Error('Update did not fail integrity checking')), 30000))]);
            }''')
            assert result is True
            assert page.evaluate('navigator.serviceWorker.controller !== null')
            assert page.evaluate('caches.keys()') == [cached[0]['name']]
            server.overrides['/'] = server.overrides['/index.html'] = next_index
            page.get_by_role('button', name='Save reader offline').click()
            expect(page.locator('#offline-status')).to_contain_text('Offline update saved', timeout=120000)
            assert page.locator('meta[name="uc2-build"]').get_attribute('content') == old_version
            page.close()
            page = context.new_page()
            # Waiting-worker activation may finish just after the old tab closes.
            page.goto(url)
            page.wait_for_function('''async version => {
                const r = await navigator.serviceWorker.getRegistration();
                return r.active && !r.waiting && (await caches.keys()).every(k => k.endsWith(version));
            }''', arg=new_version)
            context.set_offline(True)
            page.reload()
            expect(page.locator('meta[name="uc2-build"]')).to_have_attribute('content', new_version)
            expect(page.locator('#offline-status')).to_contain_text('Reader saved for offline use')
            page.get_by_role('button', name='Try the sample archive').click()
            expect(page.locator('#workspace')).to_be_visible(timeout=30000)
            server.overrides = {}
            assert not errors, errors
            assert all(method == 'GET' and target.startswith(url) for method, target in requests), requests
            browser.close()
            print(f'{args.engine} checks passed: original fixtures, rANS, versions, verified ZIP/CRC/hash/paths, selection, limits, corruption, cancellation, timeout recovery, offline use/repair/update/integrity, mobile layout, and no uploads.')
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    main()
