# Read-only MatrixOS info queries over raw HID (report 0xCB). GET commands only.
import hid, time
# Windows splits top-level collections; only the one owning report 0xCB accepts the write
h = None
for dev in [d for d in hid.enumerate(0x0203) if d['usage_page'] == 0xFF00]:
    c = hid.device(); c.open_path(dev['path'])
    try:
        if c.write([0xCB, 0x00] + [0] * 62) > 0:
            c.read(64, 1000); h = c; break
    except OSError:
        pass
    c.close()
if h is None: raise SystemExit('no MatrixOS command HID collection found')
def q(cmd):
    h.write([0xCB, cmd] + [0] * 62)
    end = time.time() + 1.5
    while time.time() < end:
        r = h.read(64, 500)
        if r and r[0] == 0xCB and r[1] == (cmd | 0x80): return bytes(r[2:])
        if r and r[0] == (cmd | 0x80): return bytes(r[1:])
    return None
s = lambda b: b.split(b'\0')[0].decode(errors='replace') if b else None
v = q(0x00); print('OS version bytes:', list(v[:4]) if v else None)
for c, n in [(1, 'name'), (2, 'model'), (3, 'serial'), (4, 'device id')]: print(n + ':', s(q(c)))
z = q(0x05); print('size:', list(z[:2]) if z else None); l = q(0x06); print('led count raw:', list(l[:4]) if l else None)
for c, n in [(0x11, 'app'), (0x12, 'app author'), (0x13, 'app version')]: print(n + ':', s(q(c)))
