import struct, sys, hashlib
src, out = sys.argv[1], sys.argv[2]
data = open(src, 'rb').read()
blocks = {}
fams = set(); flags = set()
for off in range(0, len(data), 512):
    b = data[off:off+512]
    m0, m1, fl, addr, size, bno, nblk, fam = struct.unpack('<8I', b[:32])
    if m0 != 0x0A324655 or m1 != 0x9E5D5157: continue
    fams.add(hex(fam)); flags.add(hex(fl))
    blocks[addr] = b[32:32+size]
lo, hi = min(blocks), max(blocks) + len(blocks[max(blocks)])
img = bytearray(b'\xff' * (hi - lo))
for a, p in blocks.items(): img[a-lo:a-lo+len(p)] = p
open(out, 'wb').write(img)
print(f'blocks={len(blocks)} family={fams} flags={flags} range=0x{lo:x}-0x{hi:x} ({(hi-lo)//1024} KiB)')
# esp_app_desc_t sits after the 24-byte image header + 8-byte segment header
d = img[0x20:0x20+256]
magic, sver = struct.unpack('<II', d[:8])
print('image magic', hex(img[0]), 'app_desc magic', hex(magic))
s = lambda a, n: d[a:a+n].split(b'\0')[0].decode(errors='replace')
print('version:', s(16, 32), '| project:', s(48, 32), '| built:', s(96, 16), s(80, 16), '| idf:', s(112, 32))
