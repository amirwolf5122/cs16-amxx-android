#!/usr/bin/env python3
"""Generate a minimal valid WAD3 file (gfx.wad / decals.wad stand-in for the CI test).

Every lump is a fully valid WAD3 miptex: name[16] + w + h + offsets[4] +
mip0..mip3 + short(256) + 768-byte palette — the layout the engine's
Image_LoadMIP expects.

NOTE: this FWGS master does NOT mount .wad files as search paths, so
the console/HUD font must exist as a real file. When 'conchars' is requested
and the output is gfx.wad, a standalone valve/gfx/conchars.mip (128x128
valid miptex) is written next to the wad — that is what
Con_LoadFixedWidthFont("gfx/conchars") actually resolves.
"""
import struct, sys, os

def miptex(name, w, h, fill=0xFF, pal255=(0, 0, 0)):
    m0 = bytes([fill]) * (w * h)
    m1 = bytes([fill]) * ((w // 2) * (h // 2))
    m2 = bytes([fill]) * ((w // 4) * (h // 4))
    m3 = bytes([fill]) * ((w // 8) * (h // 8))
    header_len = 40
    off0 = header_len
    off1 = off0 + len(m0)
    off2 = off1 + len(m1)
    off3 = off2 + len(m2)
    header = struct.pack('<16sII4I', name.encode()[:15] + b'\0', w, h,
                         off0, off1, off2, off3)
    palette = bytes([0x80, 0x80, 0x80]) * 255 + bytes(pal255)
    return header + m0 + m1 + m2 + m3 + struct.pack('<H', 256) + palette

def build_wad3(names):
    lumps = []
    for n in names:
        if n == 'conchars':
            lumps.append((n, miptex(n, 128, 128, fill=0xFF)))
        elif n.startswith('{'):
            # masked decals: blue last palette entry = classic decal base
            lumps.append((n, miptex(n, 8, 8, fill=0xFF, pal255=(0, 0, 255))))
        else:
            lumps.append((n, miptex(n, 8, 8)))
    body = b''
    direntries = []
    cur = 12
    for name, data in lumps:
        direntries.append(struct.pack('<IIIBBH16s', cur, len(data), len(data), 0x43, 0, 0, name.encode()[:15] + b'\0'))
        body += data
        cur += len(data)
    diroff = cur
    return b'WAD3' + struct.pack('<II', len(lumps), diroff) + body + b''.join(direntries)

if __name__ == '__main__':
    out = sys.argv[1]
    names = sys.argv[2:] or ['{CONS24', 'CONBACK', 'LAMBDA']
    open(out, 'wb').write(build_wad3(names))
    print('wrote %s' % out)
    # standalone font file: the engine loads "gfx/conchars" from a real
    # file (wads are not mounted as filesystems in FWGS master)
    if 'conchars' in names:
        conchars = miptex('conchars', 128, 128, fill=0xFF)
        gdir = os.path.join(os.path.dirname(out), 'gfx')
        os.makedirs(gdir, exist_ok=True)
        cpath = os.path.join(gdir, 'conchars.mip')
        open(cpath, 'wb').write(conchars)
        print('wrote %s' % cpath)
