// SPDX-License-Identifier: GPL-3.0-or-later
'use strict';
const $ = id => document.getElementById(id);
let worker = null, pending = null, serial = 0, epoch = 0;
let entries = [], selected = null, currentFile = null, busy = false;
const checks = new Map();
const hashes = new Map();

function status(text, error = false) {
  $('status').textContent = text;
  $('status').classList.toggle('error', error);
}
function size(value) {
  if (value < 1024) return value + ' B';
  const units = ['KiB', 'MiB', 'GiB'];
  let power = -1;
  do { value /= 1024; power++; } while (value >= 1024 && power < 2);
  return value.toFixed(value >= 10 ? 1 : 2) + ' ' + units[power];
}
function setBusy(value) {
  busy = value;
  for (const id of ['archive', 'sample', 'verify', 'inventory']) $(id).disabled = value;
  $('verify').disabled = value || !worker || !currentFile;
  $('cancel').hidden = !value;
  $('download').disabled = value || !worker || !selected || selected.directory || selected.size > 64 * 1024 * 1024;
}
function resetWorker() {
  if (worker) worker.terminate();
  worker = null;
  if (pending) {
    clearTimeout(pending.timer);
    pending.reject(new Error('Operation cancelled'));
    pending = null;
  }
}
function request(action, data = {}) {
  if (pending) return Promise.reject(new Error('Another operation is running'));
  if (!worker && action !== 'open') return Promise.reject(new Error('Reopen the archive to continue.'));
  if (!worker) {
    worker = new Worker('worker.js');
    worker.onmessage = ({data: reply}) => {
      if (!pending || reply.id !== pending.id) return;
      if (reply.progress !== undefined) {
        pending.resetTimer();
        if (reply.result) checks.set(reply.result.index, reply.result);
        status(`Checking ${reply.progress} of ${reply.total} stored files…`);
        return;
      }
      const job = pending;
      pending = null;
      clearTimeout(job.timer);
      reply.error ? job.reject(new Error(reply.error)) : job.resolve(reply);
    };
    worker.onerror = event => {
      if (pending) {
        const job = pending;
        pending = null;
        clearTimeout(job.timer);
        job.reject(new Error(event.message || 'Browser decoder could not start'));
      }
      if (worker) worker.terminate();
      worker = null;
    };
  }
  return new Promise((resolve, reject) => {
    const id = ++serial;
    const deadline = performance.now() + 300000;
    const job = {id, resolve, reject, timer: null, resetTimer() {
      clearTimeout(job.timer);
      job.timer = setTimeout(() => {
        if (worker) worker.terminate();
        worker = null; pending = null;
        reject(new Error('Time limit reached (30 seconds without progress or 5 minutes total). Reopen the archive or use uc2-preserve. Completed checks remain in the inventory.'));
      }, Math.max(0, Math.min(30000, deadline - performance.now())));
    }};
    pending = job; job.resetTimer();
    worker.postMessage({id, action, ...data});
  });
}
function dosDate(value) {
  const year = 1980 + (value >>> 25), month = (value >>> 21) & 15, day = (value >>> 16) & 31;
  const hour = (value >>> 11) & 31, minute = (value >>> 5) & 63, second = (value & 31) * 2;
  const date = new Date(Date.UTC(year, month - 1, day, hour, minute, second));
  if (date.getUTCFullYear() !== year || date.getUTCMonth() !== month - 1 || date.getUTCDate() !== day || hour > 23 || minute > 59 || second > 59) return 'Invalid DOS timestamp';
  return date.toISOString().slice(0,19).replace('T', ' ') + ' (timezone unspecified)';
}
function select(row) {
  selected = row;
  $('selected-name').textContent = row.path;
  const pairs = [
    ['Kind', row.directory ? 'Directory' : 'File'], ['Version', row.version === 0 ? '0 (newest)' : String(row.version)],
    ['Size', size(row.size)], ['Compressed', size(row.compressedSize)],
    ['Method', row.directory ? 'n/a' : String(row.method) + (row.method === 80 ? ' (Turbo; unsupported)' : '')],
    ['Timestamp', dosDate(row.dosTime)], ['DOS attributes', '0x' + row.attributes.toString(16).padStart(2, '0')],
    ['Check', checks.get(row.index)?.error || (checks.get(row.index)?.verified ? 'UC2 checksum verified' : 'Unchecked')],
  ];
  if (hashes.has(row.index)) pairs.push(['SHA-256', hashes.get(row.index)]);
  const fragment = document.createDocumentFragment();
  for (const [key, value] of pairs) {
    const dt = document.createElement('dt'), dd = document.createElement('dd');
    dt.textContent = key; dd.textContent = value; fragment.append(dt, dd);
  }
  $('metadata').replaceChildren(fragment);
  setBusy(busy);
  for (const tr of $('entries').children) tr.classList.toggle('selected', Number(tr.dataset.index) === row.index);
}
function render() {
  const query = $('search').value.toLocaleLowerCase();
  const rows = entries.filter(row => ($('versions').checked || row.version === 0) && row.path.toLocaleLowerCase().includes(query));
  const fragment = document.createDocumentFragment();
  for (const row of rows.slice(0, 1000)) {
    const tr = document.createElement('tr'); tr.dataset.index = row.index;
    tr.classList.toggle('selected', selected?.index === row.index);
    const path = document.createElement('td'), button = document.createElement('button');
    button.textContent = row.path + (row.directory ? '/' : '');
    button.addEventListener('click', () => select(row)); path.append(button); tr.append(path);
    for (const text of [row.version === 0 ? 'Latest' : 'Older ' + row.version, row.directory ? 'Folder' : size(row.size)]) {
      const td = document.createElement('td'); td.textContent = text; tr.append(td);
    }
    const td = document.createElement('td'), mark = document.createElement('span');
    const check = checks.get(row.index);
    mark.className = 'badge' + (check ? check.verified ? ' verified' : ' failed' : '');
    mark.textContent = row.directory ? '—' : check?.verified ? 'Verified' : row.method === 80 ? 'Unsupported' : check ? row.size > 64 * 1024 * 1024 ? 'Limit' : 'Failed' : 'Unchecked';
    td.append(mark); tr.append(td); fragment.append(tr);
  }
  $('entries').replaceChildren(fragment);
  $('shown').textContent = rows.length > 1000 ? `Showing 1,000 of ${rows.length} entries. Narrow the search to see the rest.` : `${rows.length} entries shown`;
}
async function openArchive(file) {
  if (!file) return;
  const token = ++epoch;
  resetWorker(); entries = []; selected = null; currentFile = null; checks.clear(); hashes.clear();
  $('workspace').hidden = true; document.body.classList.remove('has-archive'); setBusy(true); status('Reading archive directory…');
  try {
    const reply = await request('open', {file});
    if (token !== epoch) return;
    entries = reply.entries; currentFile = file;
    $('archive-name').textContent = file.name;
    $('file-count').textContent = entries.filter(row => !row.directory && !row.version).length;
    $('version-count').textContent = entries.filter(row => !row.directory).length;
    $('total-size').textContent = size(entries.filter(row => !row.directory).reduce((sum, row) => sum + row.size, 0));
    $('search').value = ''; $('versions').checked = false;
    $('workspace').hidden = false; document.body.classList.add('has-archive'); render();
    if (entries.length) select(entries.find(row => !row.directory && !row.version) || entries[0]);
    else { $('selected-name').textContent = 'Empty archive'; $('metadata').replaceChildren(); }
    status(`${entries.length} entries loaded. Choose a file or verify all stored versions.`);
  } catch (error) { if (token === epoch) status(error.message, true); }
  finally { if (token === epoch) setBusy(false); }
}
function save(bytes, name, type = 'application/octet-stream') {
  const url = URL.createObjectURL(new Blob([bytes], {type}));
  const anchor = document.createElement('a'); anchor.href = url; anchor.download = name;
  document.body.append(anchor); anchor.click(); anchor.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
$('archive').addEventListener('change', event => {
  const file = event.target.files[0]; event.target.value = ''; openArchive(file);
});
$('search').addEventListener('input', render); $('versions').addEventListener('change', render);
$('drop').addEventListener('dragover', event => {event.preventDefault(); $('drop').classList.add('drag');});
$('drop').addEventListener('dragleave', () => $('drop').classList.remove('drag'));
$('drop').addEventListener('drop', event => {event.preventDefault(); $('drop').classList.remove('drag'); if (!busy) openArchive(event.dataTransfer.files[0]);});
$('cancel').addEventListener('click', () => {
  ++epoch; resetWorker(); entries = []; selected = null; currentFile = null;
  document.body.classList.remove('has-archive');
  $('workspace').hidden = true; $('archive').value = ''; setBusy(false); status('Cancelled. Choose an archive to start again.');
});
$('sample').addEventListener('click', async () => {
  const token = epoch;
  setBusy(true);
  try {
    const response = await fetch('examples/basic.uc2');
    if (!response.ok) throw new Error('The sample archive could not be loaded');
    const data = await response.blob();
    if (token === epoch) await openArchive(new File([data], 'sample.uc2'));
  } catch (error) { if (token === epoch) {status(error.message, true); setBusy(false);} }
});
$('verify').addEventListener('click', async () => {
  const token = epoch; setBusy(true); status('Verifying stored files…');
  try {
    const {results} = await request('verify');
    if (token !== epoch) return;
    for (const result of results) checks.set(result.index, result);
    const failed = results.filter(result => !result.verified).length;
    status(!results.length ? 'This archive contains no stored files to verify.' : failed ? `${results.length - failed} verified; ${failed} failed, unsupported, or over the size limit. Select a file for details.` : `All ${results.length} stored files passed UC2 checksum verification.`, !!failed);
    render(); if (selected) select(selected);
  } catch (error) { if (token === epoch) { status(error.message, true); render(); if (selected) select(selected); } }
  finally { if (token === epoch) setBusy(false); }
});
$('download').addEventListener('click', async () => {
  if (!selected || selected.directory) return;
  const token = epoch, row = selected; setBusy(true); status('Decoding and checking ' + row.path + '…');
  try {
    const {bytes, sha256} = await request('read', {index: row.index});
    if (token !== epoch) return;
    checks.set(row.index, {verified: true}); hashes.set(row.index, sha256);
    let name = row.name.replace(/[\\/:*?"<>|\x00-\x1f\x7f]/g, '_').replace(/[. ]+$/, '') || 'entry';
    if (/^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)/i.test(name)) name = 'entry-' + name;
    save(bytes, (row.version ? `v${row.version}-` : '') + name);
    status('Verified and saved ' + row.path + '.'); render(); if (selected) select(selected);
  } catch (error) {
    if (token === epoch) { if (worker) checks.set(row.index, {verified: false, error: error.message}); status(error.message, true); render(); if (selected) select(selected); }
  } finally { if (token === epoch) setBusy(false); }
});
$('inventory').addEventListener('click', () => {
  if (!currentFile) return;
  const report = {schema: 'uc2-browser-inventory-1', source: {name: currentFile.name, size: currentFile.size},
    note: 'Checks describe UC2 checksum verification. SHA-256 is included only for files downloaded in this session. Keep the original archive.',
    entries: entries.map(row => ({...row, check: checks.get(row.index) || null, sha256: hashes.get(row.index) || null}))};
  save(JSON.stringify(report, null, 2) + '\n', 'uc2-inventory.json', 'application/json');
});
