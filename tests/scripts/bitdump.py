#!/usr/bin/env python3
"""Decode and annotate UC2 compressed bitstreams for comparison.

Reads a UC2 archive, locates either the cdir or a file data section,
and decodes the LZ77+Huffman bitstream symbol by symbol using the
exact same algorithm as the Bobrowski decompressor.

Usage:
    python3 bitdump.py archive.uc2 [--cdir | --file N] [--max-symbols 100]
"""

import struct
import sys
import argparse

# UC2 constants
MaxCodeBits = 13
NumByteSym = 256
NumDistSym = 60
NumLenSym = 28
NumSymbols = NumByteSym + NumDistSym + NumLenSym  # 344
NumDeltaCodes = 14
RepeatCode = 14
MinRepeat = 6
EOB_MARK = 64001

# Default tree lengths (BasePrev from TREEENC.CPP)
DEFAULT_LENGTHS = [0] * NumSymbols
# Symbols 0..31: code length 9 (except 10,12,32 = 7)
for i in range(32):
    DEFAULT_LENGTHS[i] = 9
DEFAULT_LENGTHS[10] = 7
DEFAULT_LENGTHS[12] = 7
DEFAULT_LENGTHS[32] = 7
# 33..127: 8 (except 46,58,92 = 7)
for i in range(33, 128):
    DEFAULT_LENGTHS[i] = 8
DEFAULT_LENGTHS[46] = 7
DEFAULT_LENGTHS[58] = 7
DEFAULT_LENGTHS[92] = 7
# 128..255: 10
for i in range(128, 256):
    DEFAULT_LENGTHS[i] = 10
# 256..271: 6
for i in range(256, 272):
    DEFAULT_LENGTHS[i] = 6
# 272..283: 7
for i in range(272, 284):
    DEFAULT_LENGTHS[i] = 7
# 284..289: 8
for i in range(284, 290):
    DEFAULT_LENGTHS[i] = 8
# 290..299: 9
for i in range(290, 300):
    DEFAULT_LENGTHS[i] = 9
# 300..315: 10
for i in range(300, 316):
    DEFAULT_LENGTHS[i] = 10
# 316..324: 4
for i in range(316, 325):
    DEFAULT_LENGTHS[i] = 4
# 325..333: 5
for i in range(325, 334):
    DEFAULT_LENGTHS[i] = 5
# 334..343: 6
for i in range(334, 344):
    DEFAULT_LENGTHS[i] = 6

# vval table (delta-to-absolute)
VVAL = [
    [0,13,12,11,10,9,8,7,6,5,4,3,2,1],
    [1,2,3,4,5,6,7,8,9,10,11,12,13,0],
    [2,1,3,4,5,6,7,8,9,10,11,12,13,0],
    [3,2,4,1,5,6,7,8,9,10,11,12,13,0],
    [4,3,5,2,6,1,7,8,9,10,11,12,13,0],
    [5,4,6,3,7,2,8,1,9,10,11,12,13,0],
    [6,5,7,4,8,3,9,2,10,1,11,12,13,0],
    [7,6,8,5,9,4,10,3,11,2,12,1,13,0],
    [8,7,9,6,10,5,11,4,12,3,13,2,0,1],
    [9,8,10,7,11,6,12,5,13,4,0,3,2,1],
    [10,9,11,8,12,7,13,6,0,5,4,3,2,1],
    [11,10,12,9,13,8,0,7,6,5,4,3,2,1],
    [12,11,13,10,0,9,8,7,6,5,4,3,2,1],
    [13,12,0,11,10,9,8,7,6,5,4,3,2,1],
]

# Distance decoding tables
DIST_CODES = []
for i in range(15):
    DIST_CODES.append((i + 1, 0))        # dist 1-15, 0 extra
for i in range(15):
    DIST_CODES.append((16 + i * 16, 4))  # dist 16-240, 4 extra
for i in range(15):
    DIST_CODES.append((256 + i * 256, 8))  # dist 256-3840, 8 extra
for i in range(15):
    DIST_CODES.append((4096 + i * 4096, 12))  # dist 4096-61440, 12 extra

# Length decoding tables
LEN_CODES = []
for i in range(8):
    LEN_CODES.append((3 + i, 0))
for i in range(8):
    LEN_CODES.append((11 + i * 2, 1))
for i in range(8):
    LEN_CODES.append((27 + i * 8, 3))
LEN_CODES.append((91, 6))
LEN_CODES.append((155, 9))
LEN_CODES.append((667, 11))
LEN_CODES.append((2715, 15))


class BitReader:
    def __init__(self, data, offset):
        self.data = data
        self.byte_pos = offset
        self.bits = 0
        self.have = 0
        self.total_bits_read = 0
        self.exhausted = False

    def _fill(self):
        if self.byte_pos + 1 < len(self.data):
            lo = self.data[self.byte_pos]
            hi = self.data[self.byte_pos + 1]
            self.bits = (self.bits << 16) | lo | (hi << 8)
            self.have += 16
            self.byte_pos += 2
        else:
            self.exhausted = True

    def peek(self, n):
        while self.have < n:
            if self.exhausted:
                return (self.bits << (n - self.have)) & ((1 << n) - 1)
            self._fill()
        return (self.bits >> (self.have - n)) & ((1 << n) - 1)

    def get(self, n):
        v = self.peek(n)
        self.have -= n
        self.total_bits_read += n
        return v


def build_decode_table(lengths, nsym):
    """Build 13-bit lookup table from code lengths."""
    table = [None] * (1 << MaxCodeBits)
    code = 0
    for bit_len in range(1, MaxCodeBits + 1):
        for sym in range(nsym):
            if lengths[sym] == bit_len:
                prefix = code << (MaxCodeBits - bit_len)
                count = 1 << (MaxCodeBits - bit_len)
                for j in range(count):
                    table[prefix + j] = (sym, bit_len)
                code += 1
        code <<= 1
    return table


def huff_decode(br, table):
    """Decode one Huffman symbol."""
    idx = br.peek(MaxCodeBits)
    entry = table[idx]
    if entry is None:
        return None, 0
    sym, bits = entry
    br.get(bits)
    return sym, bits


def decode_tree(br, symprev):
    """Decode Huffman tree from bitstream."""
    tree_changed = br.get(1)
    if not tree_changed:
        lengths = list(DEFAULT_LENGTHS)
        for i in range(NumSymbols):
            symprev[i] = lengths[i]
        return lengths, "default"

    t = br.get(2)
    has_lo = t & 1
    has_hi = (t >> 1) & 1

    # Read tree-encoding tree (15 x 3 bits)
    tlengths = [br.get(3) for _ in range(15)]

    # Build meta-tree decode table
    meta_table = build_decode_table(tlengths, 15)

    # Compute stream size
    stream_size = NumSymbols
    if not has_lo:
        stream_size -= 28  # skip symbols 4..31
    if not has_hi:
        stream_size -= 128  # skip symbols 128..255

    # Decode delta stream with RLE
    stream = []
    val = 0
    while len(stream) < stream_size:
        sym, _ = huff_decode(br, meta_table)
        if sym == RepeatCode:
            c, _ = huff_decode(br, meta_table)
            count = c + MinRepeat - 1
            stream.extend([val] * count)
        else:
            val = sym
            stream.append(sym)

    # Convert delta to absolute lengths
    lengths = [0] * NumSymbols
    si = 0
    if has_lo:
        for i in range(32):
            lengths[i] = VVAL[symprev[i]][stream[si]]
            si += 1
    else:
        lengths[9] = VVAL[symprev[9]][stream[si]]; si += 1
        lengths[10] = VVAL[symprev[10]][stream[si]]; si += 1
        lengths[12] = VVAL[symprev[12]][stream[si]]; si += 1
        lengths[13] = VVAL[symprev[13]][stream[si]]; si += 1

    for i in range(32, 128):
        lengths[i] = VVAL[symprev[i]][stream[si]]
        si += 1

    if has_hi:
        for i in range(128, 256):
            lengths[i] = VVAL[symprev[i]][stream[si]]
            si += 1

    for i in range(256, 344):
        lengths[i] = VVAL[symprev[i]][stream[si]]
        si += 1

    for i in range(NumSymbols):
        symprev[i] = lengths[i]

    return lengths, f"custom(t={t})"


def decode_block(br, bd_table, l_table, max_symbols):
    """Decode LZ77 symbols from one block."""
    symbols = []
    max_bits = (len(br.data) - br.byte_pos + br.have) * 8 + 1000
    start_bits = br.total_bits_read
    while len(symbols) < max_symbols:
        if br.total_bits_read - start_bits > max_bits:
            symbols.append(("ERROR", "exceeded bit limit"))
            break
        sym, _ = huff_decode(br, bd_table)
        if sym is None:
            symbols.append(("ERROR", f"invalid Huffman code at bit {br.total_bits_read}"))
            break

        if sym < NumByteSym:
            symbols.append(("LIT", sym))
        else:
            dsym = sym - NumByteSym
            if dsym >= NumDistSym:
                symbols.append(("ERROR", f"dist sym {dsym} out of range"))
                break
            base, extra_bits = DIST_CODES[dsym]
            dist = base
            if extra_bits:
                dist += br.get(extra_bits)

            if dist == EOB_MARK:
                # Read length (should be 3)
                lsym, _ = huff_decode(br, l_table)
                lbase, lextra = LEN_CODES[lsym]
                length = lbase + (br.get(lextra) if lextra else 0)
                symbols.append(("EOB", f"dist={dist} len={length}"))
                break

            lsym, _ = huff_decode(br, l_table)
            if lsym is None:
                symbols.append(("ERROR", "invalid length Huffman code"))
                break
            lbase, lextra = LEN_CODES[lsym]
            length = lbase + (br.get(lextra) if lextra else 0)
            symbols.append(("MATCH", f"dist={dist} len={length}"))

    return symbols


def analyze_archive(path, section, max_symbols):
    with open(path, "rb") as f:
        data = f.read()

    magic = data[0:4]
    if magic != b'UC2\x1a':
        print(f"Not a UC2 archive: {magic}")
        return

    complen = struct.unpack_from('<I', data, 4)[0]
    cdir_off = struct.unpack_from('<I', data, 17)[0]
    fletch = struct.unpack_from('<H', data, 21)[0]
    ver_made = struct.unpack_from('<H', data, 24)[0]
    ver_need = struct.unpack_from('<H', data, 26)[0]

    print(f"Archive: {path} ({len(data)} bytes)")
    print(f"  complen={complen}, total={complen+13}")
    print(f"  cdir_offset={cdir_off}, fletcher={fletch:#06x}")
    print(f"  versionMade={ver_made}, versionNeeded={ver_need}")

    if section == 'cdir':
        crec = data[cdir_off:cdir_off + 10]
        csize, method, master = struct.unpack_from('<IHI', crec)
        print(f"  cdir COMPRESS: csize={csize}, method={method}, master={master}")
        stream_start = cdir_off + 10
    else:
        stream_start = 29
        print(f"  File data starts at offset {stream_start}")

    print()

    br = BitReader(data, stream_start)
    symprev = list(DEFAULT_LENGTHS)
    total_decoded = 0

    block_num = 0
    while total_decoded < max_symbols:
        bit_pos = br.total_bits_read
        block_present = br.get(1)
        print(f"Block {block_num} at bit {bit_pos}: present={block_present}")
        if not block_present:
            print("  End of stream")
            break

        lengths, tree_desc = decode_tree(br, symprev)
        tree_bits = br.total_bits_read - bit_pos - 1
        print(f"  Tree: {tree_desc} ({tree_bits} bits)")

        nonzero = sum(1 for l in lengths if l > 0)
        print(f"  Non-zero lengths: {nonzero}/{NumSymbols}")

        bd_table = build_decode_table(lengths[:NumByteSym + NumDistSym],
                                      NumByteSym + NumDistSym)
        l_table = build_decode_table(lengths[NumByteSym + NumDistSym:],
                                     NumLenSym)

        bd_none = sum(1 for x in bd_table if x is None)
        l_none = sum(1 for x in l_table if x is None)
        if bd_none:
            print(f"  WARNING: {bd_none}/{len(bd_table)} BD table entries are None")
        if l_none:
            print(f"  WARNING: {l_none}/{len(l_table)} LEN table entries are None")

        # Decode until EOB or error (no per-block symbol limit)
        remaining = max_symbols - total_decoded
        symbols = decode_block(br, bd_table, l_table, remaining)
        total_decoded += len(symbols)

        truncated = len(symbols) >= remaining and symbols[-1][0] not in ("EOB", "ERROR")
        print(f"  Decoded {len(symbols)} symbols{' (truncated)' if truncated else ''}:")
        for i, (kind, val) in enumerate(symbols):
            if kind == "LIT":
                ch = chr(val) if 32 <= val < 127 else f"\\x{val:02x}"
                print(f"    [{i:3d}] LIT {val:3d} '{ch}'")
            elif kind == "MATCH":
                print(f"    [{i:3d}] {val}")
            elif kind == "EOB":
                print(f"    [{i:3d}] EOB ({val})")
            elif kind == "ERROR":
                print(f"    [{i:3d}] ERROR: {val}")

        data_bits = br.total_bits_read - bit_pos - 1 - tree_bits
        print(f"  Data: {data_bits} bits")
        print()

        if truncated or (symbols and symbols[-1][0] in ("ERROR",)):
            break
        block_num += 1


def main():
    parser = argparse.ArgumentParser(description='UC2 bitstream analyzer')
    parser.add_argument('archive', help='UC2 archive file')
    parser.add_argument('--cdir', action='store_true', help='Analyze cdir section')
    parser.add_argument('--file', action='store_true', help='Analyze file data section')
    parser.add_argument('--max-symbols', type=int, default=200,
                        help='Max symbols to decode per block')
    args = parser.parse_args()

    section = 'cdir' if args.cdir else 'file'
    analyze_archive(args.archive, section, args.max_symbols)


if __name__ == '__main__':
    main()
