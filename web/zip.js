// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
'use strict';
// Small ZIP STORE writer. Payloads have already passed UC2 verification.
// Classic ZIP only: bounds below stay well below ZIP64's size/count thresholds.
const ZIP_LIMIT = 128 * 1024 * 1024;
const ZIP_FILES = 1000;
const zipCRC = new Uint32Array(256);
for (let i = 0; i < 256; i++) {
  let crc = i;
  for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ (crc & 1 ? 0xedb88320 : 0);
  zipCRC[i] = crc >>> 0;
}
function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) crc = (crc >>> 8) ^ zipCRC[(crc ^ byte) & 255];
  return (crc ^ 0xffffffff) >>> 0;
}
function zipPath(row) {
  const leaf = row.name.replace(/[^a-zA-Z0-9._-]/g, '_').replace(/^[.]+|[.]+$/g, '').slice(0, 80) || 'entry';
  return `files/${String(row.index).padStart(5, '0')}-v${row.version}-${leaf}`;
}
function zipDate(value) {
  const y = 1980 + (value >>> 25), m = (value >>> 21) & 15, d = (value >>> 16) & 31;
  const h = (value >>> 11) & 31, min = (value >>> 5) & 63, sec = (value & 31) * 2;
  const date = new Date(Date.UTC(y, m - 1, d, h, min, sec));
  return date.getUTCFullYear() === y && date.getUTCMonth() === m - 1 && date.getUTCDate() === d && h < 24 && min < 60 && sec < 60 ? value >>> 0 : 0x00210000;
}
class UC2Zip {
  constructor() { this.parts = []; this.directory = []; this.offset = 0; this.directorySize = 0; }
  add(name, bytes, timestamp = 0) {
    if (!(bytes instanceof Uint8Array) || !/^[A-Za-z0-9._/-]+$/.test(name) ||
        name.split('/').some(part => !part || part === '.' || part === '..')) throw new Error('Invalid ZIP entry');
    const encoded = new TextEncoder().encode(name);
    if (encoded.length > 240 || this.directory.length >= ZIP_FILES + 1) throw new Error('ZIP entry limit exceeded');
    if (this.offset + 30 + encoded.length + bytes.length + this.directorySize + 46 + encoded.length + 22 > ZIP_LIMIT)
      throw new Error('ZIP exceeds the 128 MiB download limit');
    const crc = crc32(bytes), date = zipDate(timestamp);
    const local = new Uint8Array(30 + encoded.length), lv = new DataView(local.buffer);
    lv.setUint32(0, 0x04034b50, true); lv.setUint16(4, 20, true); lv.setUint16(6, 0x800, true);
    lv.setUint32(10, date, true); lv.setUint32(14, crc, true);
    lv.setUint32(18, bytes.length, true); lv.setUint32(22, bytes.length, true);
    lv.setUint16(26, encoded.length, true); local.set(encoded, 30);
    const central = new Uint8Array(46 + encoded.length), cv = new DataView(central.buffer);
    cv.setUint32(0, 0x02014b50, true); cv.setUint16(4, 0x314, true); cv.setUint16(6, 20, true);
    cv.setUint16(8, 0x800, true); cv.setUint32(12, date, true); cv.setUint32(16, crc, true);
    cv.setUint32(20, bytes.length, true); cv.setUint32(24, bytes.length, true);
    cv.setUint16(28, encoded.length, true); cv.setUint32(38, 0x81a40000, true);
    cv.setUint32(42, this.offset, true); central.set(encoded, 46);
    this.parts.push(local, bytes); this.directory.push(central);
    this.offset += local.length + bytes.length; this.directorySize += central.length;
  }
  finish() {
    const end = new Uint8Array(22), view = new DataView(end.buffer);
    view.setUint32(0, 0x06054b50, true);
    view.setUint16(8, this.directory.length, true); view.setUint16(10, this.directory.length, true);
    view.setUint32(12, this.directorySize, true); view.setUint32(16, this.offset, true);
    return new Blob([...this.parts, ...this.directory, end], {type: 'application/zip'});
  }
}
