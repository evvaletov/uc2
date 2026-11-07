// SPDX-License-Identifier: GPL-3.0-or-later
'use strict';
importScripts('uc2-core.js');
let core;
let entries = [];
let mounted = false;
let opened = false;
const MAX_INPUT = 256 * 1024 * 1024;
const MAX_VERIFY = 1024 * 1024 * 1024;

function check(result) {
  if (result < 0) throw new Error(core.UTF8ToString(core._uc2_web_error()));
  return result;
}

function readIndex(count) {
  const rows = Array.from({length: count}, (_, index) => {
    const field = n => core._uc2_web_field(index, n) >>> 0;
    const name = core.UTF8ToString(core._uc2_web_name(index));
    if (!name || name.includes('/') || name.includes('\\')) throw new Error('Invalid name component');
    return {index, name, parent: field(0), id: field(1), directory: !!field(2),
      size: field(3), compressedSize: field(4), dosTime: field(5), attributes: field(6), method: field(7)};
  });
  const directories = new Map();
  for (const row of rows) if (row.directory) {
    if (!row.id || directories.has(row.id)) throw new Error('Invalid directory identifier');
    directories.set(row.id, row);
  }
  const kinds = new Map();
  for (const row of rows) {
    const parts = [row.name];
    const seen = new Set();
    let parent = row.parent;
    while (parent) {
      if (seen.has(parent) || seen.size >= 64) throw new Error('Cyclic or excessively deep directory');
      seen.add(parent);
      const directory = directories.get(parent);
      if (!directory) throw new Error('Missing parent directory');
      parts.unshift(directory.name);
      parent = directory.parent;
    }
    row.path = parts.join('/');
    if (row.path.length > 4096) throw new Error('Archive path is too long');
    if (kinds.has(row.path) && (row.directory || kinds.get(row.path))) throw new Error('Conflicting paths');
    kinds.set(row.path, row.directory);
  }
  const versions = new Map();
  for (const row of [...rows].reverse()) {
    row.version = versions.get(row.path) || 0;
    versions.set(row.path, row.version + 1);
  }
  return rows;
}

self.onmessage = async ({data}) => {
  const {id, action} = data;
  try {
    if (!core) core = await createUC2({print: () => {}, printErr: () => {}});
    if (action === 'open') {
      opened = false;
      const file = data.file;
      if (!(file instanceof Blob) || file.size > MAX_INPUT) throw new Error('Archive exceeds the 256 MiB browser limit');
      core._uc2_web_close();
      entries = [];
      if (mounted) core.FS.unmount('/input');
      else core.FS.mkdir('/input');
      mounted = false;
      core.FS.mount(core.WORKERFS, {blobs: [{name: 'archive.uc2', data: file}]}, '/input');
      mounted = true;
      entries = readIndex(check(core._uc2_web_open()));
      opened = true;
      self.postMessage({id, entries});
    } else if (action === 'read') {
      if (!opened) throw new Error('Open an archive first');
      const row = entries[data.index];
      if (!row || row.directory) throw new Error('Select a file');
      check(core._uc2_web_extract(row.index, 1));
      const start = core._uc2_web_output();
      const bytes = core.HEAPU8.slice(start, start + row.size);
      core._uc2_web_free_output();
      const digest = await crypto.subtle.digest('SHA-256', bytes);
      const sha256 = Array.from(new Uint8Array(digest), b => b.toString(16).padStart(2, '0')).join('');
      self.postMessage({id, bytes, sha256}, [bytes.buffer]);
    } else if (action === 'verify') {
      if (!opened) throw new Error('Open an archive first');
      const files = entries.filter(row => !row.directory);
      if (files.reduce((total, row) => total + row.size, 0) > MAX_VERIFY) throw new Error('Verification exceeds the 1 GiB output limit');
      const results = [];
      for (const row of files) {
        const result = core._uc2_web_extract(row.index, 0);
        results.push({index: row.index, verified: result >= 0,
          error: result < 0 ? core.UTF8ToString(core._uc2_web_error()) : null});
        self.postMessage({id, progress: results.length, total: files.length, result: results[results.length - 1]});
      }
      self.postMessage({id, results});
    } else {
      throw new Error('Unknown operation');
    }
  } catch (error) {
    if (action === 'open' && core) { core._uc2_web_close(); entries = []; opened = false; }
    self.postMessage({id, error: error.message || String(error)});
  }
};
