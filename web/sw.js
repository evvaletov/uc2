// SPDX-License-Identifier: GPL-3.0-or-later
'use strict';
// Replaced by build.py. Only this immutable, digest-checked shell is cached.
const VERSION = '__UC2_VERSION__';
const ASSETS = __UC2_ASSETS__;
const PREFIX = 'uc2-reader:' + self.registration.scope + ':';
const CACHE = PREFIX + VERSION;
const assetURLs = new Map(Object.entries(ASSETS).map(([name, digest]) => [new URL(name, self.registration.scope).href, digest]));
let preparation = null;

async function checkedResponse(url, expected) {
  const response = await fetch(url, {cache: 'reload', redirect: 'error'});
  if (!response.ok) throw new Error('Could not save the offline reader.');
  const digest = Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', await response.clone().arrayBuffer())), b => b.toString(16).padStart(2, '0')).join('');
  if (digest !== expected) throw new Error('The reader changed during download. Reload and try again.');
  return response;
}

async function ready() {
  if (!(await caches.has(CACHE))) return false;
  const cache = await caches.open(CACHE);
  for (const url of assetURLs.keys()) if (!(await cache.match(url))) return false;
  return true;
}
function prepare() {
  if (preparation) return preparation;
  preparation = (async () => {
    const cache = await caches.open(CACHE);
    for (const [url, expected] of assetURLs) {
      if (await cache.match(url)) continue;
      await cache.put(url, await checkedResponse(url, expected));
    }
  })().finally(() => { preparation = null; });
  return preparation;
}
self.addEventListener('install', event => {
  event.waitUntil(prepare().catch(async error => { await caches.delete(CACHE); throw error; }));
});
self.addEventListener('activate', event => {
  // Do not skip waiting or claim existing pages: old tabs keep their matching
  // worker and decoder. A new navigation uses the complete new shell.
  event.waitUntil(caches.keys().then(keys => Promise.all(keys.filter(key => key.startsWith(PREFIX) && key !== CACHE).map(key => caches.delete(key)))));
});
self.addEventListener('fetch', event => {
  if (event.request.method !== 'GET') return;
  const url = new URL(event.request.url);
  url.search = ''; url.hash = '';
  if (url.href === self.registration.scope) url.pathname += 'index.html';
  if (!assetURLs.has(url.href)) return;
  event.respondWith(caches.open(CACHE).then(async cache => (await cache.match(url.href)) || checkedResponse(url.href, assetURLs.get(url.href))));
});
self.addEventListener('message', event => {
  if (!event.ports[0] || !['status', 'prepare'].includes(event.data?.action)) return;
  event.waitUntil((async () => {
    try {
      if (event.data.action === 'prepare') await prepare();
      event.ports[0].postMessage({version: VERSION, ready: await ready()});
    } catch (error) { event.ports[0].postMessage({error: error.message}); }
  })());
});
