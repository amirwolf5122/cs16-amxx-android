#!/usr/bin/env python3
"""Generate a minimal valid GoldSrc studio model (.mdl, version 10).

1 bone, 1 sequence ("idle", 1 frame, no animation data - all channels use
bone defaults), 1 texture, 1 bodypart with 1 mesh (0 triangles), 1 hitbox.
Used by the CI arm64 test as stand-in player/weapon models so the server
can precache and the client can connect without shipping game assets.
"""
import struct, sys

def build_model(model_name='minimal.mdl', tex_name='minimal.bmp'):
    S = 32   # MAXSTUDIONAME

    parts = []          # (bytes) appended in order; offsets computed after
    # --- header (vanilla GoldSrc studiohdr_t, 112 + ... computed) ---
    def hdr_layout():
        # returns list of (field, size) ints
        f = []
        f += [('id', 4), ('version', 4)]
        f += [('name', 64), ('length', 4)]
        f += [('eyeposition', 12), ('min', 12), ('max', 12), ('bbmin', 12), ('bbmax', 12)]
        f += [('flags', 4)]
        for k in ['numbones','boneindex','numbonecontrollers','bonecontrollerindex',
                  'numhitboxes','hitboxindex','numseq','seqindex','numseqgroups',
                  'seqgroupindex','numtextures','textureindex','texturedataindex',
                  'numskinref','numskinfamilies','skinindex','numbodyparts',
                  'bodypartindex','numattachments','attachmentindex','soundtable',
                  'soundindex','soundgroups','soundgroupindex','numtransitions',
                  'transitionindex']:
            f.append((k, 4))
        return f
    HDR_INT_FIELDS = [k for k, s in hdr_layout() if s == 4]
    HDR_SIZE = sum(s for _, s in hdr_layout())
    # ^ vanilla studiohdr_t = 4+4+64+4+60+4+26*4 = 236? compute at runtime

    bone = struct.pack('<%dsii6i12f' % S,
                       b'root\0', -1, 0,
                       *([-1] * 6),                 # bonecontroller
                       0, 0, 0, 0, 0, 0,            # value[6]
                       0, 0, 0, 0, 0, 0)            # scale[6]

    # sequence desc (vanilla): label[32] fps flags activity actweight numevents
    # eventindex numframes numpivots pivotindex motiontype motionbone
    # linearmovement[3] automoveposindex automoveangleindex bbmin[3] bbmax[3]
    # numblends animindex blendtype[2] blendstart[2] blendend[2] blendparent
    # seqgroup entrynode exitnode nodeflags nextseq
    anim_index_placeholder = 0
    _i = struct.pack  # shorthand
    seqdesc = _i('<32sf', b'idle\0', 1.0)
    seqdesc += _i('<5i', 0, 0, 0, 0, 0)          # flags, activity, actweight, numevents, eventindex
    seqdesc += _i('<i', 1)                       # numframes
    seqdesc += _i('<2i', 0, 0)                   # numpivots, pivotindex
    seqdesc += _i('<2i', 0, 0)                   # motiontype, motionbone
    seqdesc += _i('<3f', 0, 0, 0)                # linearmovement
    seqdesc += _i('<2i', 0, 0)                   # automoveposindex, automoveangleindex
    seqdesc += _i('<6f', -16, -16, -16, 16, 16, 16)  # bbmin/bbmax
    seqdesc += _i('<i', 1)                       # numblends
    seqdesc += _i('<i', 0)                       # animindex (patched later)
    seqdesc += _i('<2i', 0, 0)                   # blendtype[2]
    seqdesc += _i('<2f', 0.0, 0.0)               # blendstart[2]
    seqdesc += _i('<2f', 0.0, 0.0)               # blendend[2]
    seqdesc += _i('<i', 0)                       # blendparent
    seqdesc += _i('<i', 0)                       # seqgroup (0 = in-file)
    seqdesc += _i('<4i', 0, 0, 0, 0)             # entrynode, exitnode, nodeflags, nextseq

    anim = struct.pack('<6H', 0, 0, 0, 0, 0, 0)  # all channels: use bone defaults

    seqgroup = struct.pack('<32s64si', b'default\0', b'', 0)

    texdata = bytes([0x30]) * (16 * 16 + 8 * 8 + 4 * 4 + 2 * 2 + 1) + bytes([0x80, 0x80, 0x80]) * 256
    texture = struct.pack('<64sIii i', tex_name.encode(), 0, 16, 16, 0)  # index patched later

    skin = struct.pack('<h', 0)

    bodypart = struct.pack('<64s3i', b'studio\0', 1, 0, 0)  # modelindex patched
    model = struct.pack('<64sif10i',
        model_name.encode()[:63] + b'\0',
        0, 0.0,
        1, 0,      # nummesh, meshindex (patched)
        0, 0, 0,   # numverts, vertinfoindex, vertindex
        0, 0, 0,   # numnorms, norminfoindex, normindex
        0, 0)      # blendvertinfoindex, blendnorminfoindex
    mesh = struct.pack('<5i', 0, 0, 0, 0, 0)  # numtris=0, triindex, skinref=0, numnorms, normindex
    hitbox = struct.pack('<2i6f', 0, 0, -8, -8, -8, 8, 8, 8)

    # ---- assemble with patching ----
    # order: header | bone | hitbox | seqdesc | seqgroup | texture | skin |
    #        bodypart | model | mesh | anim
    off = HDR_SIZE
    bone_i = off;          off += len(bone)
    hitbox_i = off;        off += len(hitbox)
    seq_i = off;           off += len(seqdesc)
    seqgroup_i = off;      off += len(seqgroup)
    texture_i = off;       off += len(texture)
    skin_i = off;          off += len(skin)
    bodypart_i = off;      off += len(bodypart)
    model_i = off;         off += len(model)
    mesh_i = off;          off += len(mesh)
    anim_i = off;          off += len(anim)
    texdata_i = off;       off += len(texdata)

    def patch(buf, at, *vals):
        b = bytearray(buf)
        struct.pack_into('<%di' % len(vals), b, at, *vals)
        return bytes(b)

    texture = patch(texture, texture.index(0) if False else 64 + 4 + 4 + 4, texdata_i)
    bodypart = patch(bodypart, 64 + 8, model_i)
    model = patch(model, 64 + 8, 1, mesh_i)                 # nummesh=1, meshindex
    seqdesc = patch(seqdesc, 124, anim_i)                   # animindex field

    fields = {
        'id': b'IDST', 'version': 10, 'name': model_name.encode()[:63] + b'\0',
        'length': 0, 'eyeposition': struct.pack('<3f', 0, 0, 64),
        'min': None, 'max': None,
        'bbmin': struct.pack('<3f', -16, -16, 0), 'bbmax': struct.pack('<3f', 16, 16, 72),
        'flags': 0,
        'numbones': 1, 'boneindex': bone_i,
        'numbonecontrollers': 0, 'bonecontrollerindex': 0,
        'numhitboxes': 1, 'hitboxindex': hitbox_i,
        'numseq': 1, 'seqindex': seq_i,
        'numseqgroups': 1, 'seqgroupindex': seqgroup_i,
        'numtextures': 1, 'textureindex': texture_i, 'texturedataindex': texdata_i,
        'numskinref': 1, 'numskinfamilies': 1, 'skinindex': skin_i,
        'numbodyparts': 1, 'bodypartindex': bodypart_i,
        'numattachments': 0, 'attachmentindex': 0,
        'soundtable': 0, 'soundindex': 0, 'soundgroups': 0, 'soundgroupindex': 0,
        'numtransitions': 0, 'transitionindex': 0,
    }
    header = struct.pack('<4si', b'IDST', 10)
    header += struct.pack('<64s', fields['name'])
    header += struct.pack('<i', 0)                      # length (patched)
    header += fields['eyeposition']
    header += struct.pack('<3f', -16, -16, 0)
    header += struct.pack('<3f', 16, 16, 72)
    header += fields['bbmin'] + fields['bbmax']
    header += struct.pack('<i', fields['flags'])
    for k in HDR_INT_FIELDS:
        if k in ('id', 'version', 'length', 'flags'):
            continue  # already packed manually
        header += struct.pack('<i', fields[k])
    assert len(header) == HDR_SIZE, (len(header), HDR_SIZE)

    total = off
    header = patch(header, 64 + 8, total)   # length field at offset 4+4+64
    data = header + bone + hitbox + seqdesc + seqgroup + texture + skin + \
           bodypart + model + mesh + anim + texdata
    return data

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'minimal.mdl'
    d = build_model()
    open(out, 'wb').write(d)
    print('wrote %s: %d bytes' % (out, len(d)))
