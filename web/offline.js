// SPDX-License-Identifier: GPL-3.0-or-later
'use strict';
(() => {
  const button = document.getElementById('offline-save');
  const label = document.getElementById('offline-status');
  const install = document.getElementById('install-reader');
  const version = document.querySelector('meta[name="uc2-build"]').content;
  const scope = new URL('./', location.href).href;
  let installPrompt = null;
  function ask(worker, action) {
    return new Promise((resolve, reject) => {
      const channel = new MessageChannel();
      const timer = setTimeout(() => { channel.port1.close(); reject(new Error('Offline download timed out. Try again while connected.')); }, 120000);
      channel.port1.onmessage = ({data}) => {
        clearTimeout(timer); channel.port1.close();
        data.error ? reject(new Error(data.error)) : resolve(data);
      };
      worker.postMessage({action}, [channel.port2]);
    });
  }
  async function report(registration, repair = false) {
    const worker = registration.waiting || registration.active;
    if (!worker) throw new Error('The offline reader is not ready. Try again while connected.');
    let result = await ask(worker, 'status');
    if (repair && !result.ready) result = await ask(worker, 'prepare');
    if (!result.ready) label.textContent = 'Offline reader is incomplete. Save it again while connected.';
    else if (registration.waiting === worker) label.textContent = 'Offline update saved. Close all reader tabs and reopen to use it.';
    else if (result.version !== version) label.textContent = 'A different offline version is saved. Close all reader tabs and reopen to update.';
    else label.textContent = 'Reader saved for offline use. Reopen this page when offline.';
  }
  function installed(worker, updating) {
    const complete = () => worker.state === 'activated' || (updating && worker.state === 'installed');
    if (!worker || complete()) return Promise.resolve();
    return new Promise((resolve, reject) => {
      const finish = error => { clearTimeout(timer); worker.removeEventListener('statechange', changed); error ? reject(error) : resolve(); };
      const changed = () => {
        if (complete()) finish();
        if (worker.state === 'redundant') finish(new Error('Offline download failed. Reload and try again.'));
      };
      const timer = setTimeout(() => finish(new Error('Offline download timed out. Try again while connected.')), 120000);
      worker.addEventListener('statechange', changed); changed();
    });
  }
  if (!('serviceWorker' in navigator) || !isSecureContext) {
    button.disabled = true; label.textContent = 'Offline saving requires HTTPS or localhost.';
    return;
  }
  button.addEventListener('click', async () => {
    button.disabled = true; label.textContent = 'Saving the reader for offline use…';
    try {
      const registration = await navigator.serviceWorker.register('sw.js', {scope: './', updateViaCache: 'none'});
      const updating = !!registration.active;
      if (!registration.installing) await registration.update();
      await installed(registration.installing || registration.waiting || registration.active, updating);
      await report(registration, true);
    } catch (error) { label.textContent = error.message; }
    finally { button.disabled = false; }
  });
  navigator.serviceWorker.getRegistration(scope).then(registration => {
    if (registration?.scope === scope && registration.active) return report(registration);
  }).catch(() => { label.textContent = 'Offline status unavailable. Save the reader again while connected.'; });
  window.addEventListener('beforeinstallprompt', event => {
    event.preventDefault(); installPrompt = event; install.hidden = false;
  });
  install.addEventListener('click', async () => {
    if (!installPrompt) return;
    const prompt = installPrompt; installPrompt = null; install.hidden = true;
    await prompt.prompt(); await prompt.userChoice;
  });
})();
