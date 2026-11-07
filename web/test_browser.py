#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the compiled reader and its UI in an isolated headless browser."""
import argparse
from functools import partial
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import tempfile
from threading import Thread

from playwright.sync_api import sync_playwright, expect
from uc2 import UC2File, UC2Writer

ROOT = Path(__file__).resolve().parents[1]


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dist', type=Path, default=ROOT / 'web/dist')
    parser.add_argument('--browser')
    parser.add_argument('--screenshot', type=Path)
    args = parser.parse_args()
    server = ThreadingHTTPServer(('127.0.0.1', 0), partial(QuietHandler, directory=str(args.dist)))
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = f'http://127.0.0.1:{server.server_port}/'
    try:
        with tempfile.TemporaryDirectory(prefix='uc2-browser-test-') as temporary, sync_playwright() as pw:
            scratch = Path(temporary)
            browser = pw.chromium.launch(headless=True, executable_path=args.browser)
            context = browser.new_context(accept_downloads=True)
            page = context.new_page()
            errors, requests = [], []
            page.on('pageerror', lambda error: errors.append(str(error)))
            context.on('request', lambda request: requests.append((request.method, request.url)))
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
            page.locator('#search').fill('café')
            expect(page.locator('#entries tr')).to_have_count(2)
            page.set_viewport_size({'width': 390, 'height': 844})
            assert page.evaluate('document.documentElement.scrollWidth <= innerWidth')

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
            assert not errors, errors
            assert all(method == 'GET' and target.startswith(url) for method, target in requests), requests
            browser.close()
            print('Browser checks passed: original fixtures, rANS, versions, downloads, inventory, unsafe names, corruption, cancellation, timeout recovery, mobile layout, and no uploads.')
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    main()
