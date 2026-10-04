#!/usr/bin/env python3
"""Generate a minimal valid GoldSrc BSP30 map (amxx_test.bsp).

A tiny box room: 6 faces, 1 embedded texture (AAATEST), no WAD
dependencies, worldspawn + info_player_start + light entities.
The point is to make the SERVER spawn and the CLIENT connect so the
AMXX/metamod chain runs end-to-end on the device.
"""
import struct, sys

# GoldSrc BSP30 on-disk lump order (matches xash3d-fwgs LUMP_ enum)
L_ENTITIES, L_PLANES, L_TEXTURES, L_VERTEXES, L_VISIBILITY, L_NODES, L_TEXINFO, \
L_FACES, L_LIGHTING, L_CLIPNODES, L_LEAFS, L_MARKSURFACES, L_EDGES, L_SURFEDGES, \
L_MODELS = range(15)

EMPTY, SOLID = -1, -2
P_X, P_Y, P_Z = 0, 1, 2

def plane(n, d, t):
    return struct.pack('<3ffi', n[0], n[1], n[2], d, t)

def node(planenum, front, back, firstface, numfaces):
    b = [-192, -192, -192, 192, 192, 192]
    return struct.pack('<i2h6h2h', planenum, front, back, *b, firstface, numfaces)

def leaf(contents, marks, visofs=-1):
    amb = (0, 0, 0, 0)
    if contents == EMPTY:
        b = [-192, -192, -64, 192, 192, 192]
    else:
        b = [-32768, -32768, -32768, 32767, 32767, 32767]
    return struct.pack('<ii6h2h4B', contents, visofs, *b, marks, (1 if marks >= 0 else 0), *amb)

def face(planenum, firstedge, numedges, texinfo=0, side=0):
    return struct.pack('<hhihh4Bi', planenum, side, firstedge, numedges, texinfo, 0, 0, 0, 0, -1)

def clipnode(planenum, front, back):
    return struct.pack('<i2h', planenum, front, back)

def build():
    X0, X1, Y0, Y1, Z0, Z1 = -192.0, 192.0, -192.0, 192.0, -64.0, 192.0
    corners = [(X0,Y0,Z0),(X1,Y0,Z0),(X1,Y1,Z0),(X0,Y1,Z0),
               (X0,Y0,Z1),(X1,Y0,Z1),(X1,Y1,Z1),(X0,Y1,Z1)]
    vertexes = b''.join(struct.pack('<3f', *c) for c in corners)

    # wall planes, normals pointing INTO the room
    planes = [
        plane(( 1,0,0), X0, P_X),  # 0: X0 wall
        plane((-1,0,0), -X1, P_X), # 1: X1 wall
        plane(( 0,1,0), Y0, P_Y),  # 2: Y0 wall
        plane(( 0,-1,0), -Y1, P_Y),# 3: Y1 wall
        plane(( 0,0,1), Z0, P_Z),  # 4: floor
        plane(( 0,0,-1), -Z1, P_Z),# 5: ceiling
    ]

    texinfo = struct.pack('<8fii', 1,0,0,0, 0,1,0,0, 0, 0)

    quads = [  # (verts CCW seen from inside, plane index)
        ([4,5,1,0], 0),   # X0 wall
        ([6,7,3,2], 1),   # X1 wall
        ([5,6,2,1], 2),   # Y0 wall
        ([7,4,0,3], 3),   # Y1 wall
        ([0,1,2,3], 4),   # floor
        ([4,5,6,7], 5),   # ceiling
    ]
    faces, edges, surfedges = [], [], []
    for verts, pnum in quads:
        start = len(surfedges)
        for i, v in enumerate(verts):
            nxt = verts[(i + 1) % 4]
            edges.append(struct.pack('<HH', v, nxt))
            surfedges.append(len(edges) - 1)
        faces.append(face(pnum, start, 4))

    # BSP tree: chain through the 6 planes; leaf 0 MUST be solid (BSP convention),
    # the room interior ends in leaf 1 (EMPTY)
    nodes = [
        node(0, 1, -1, 0, 1),   # node0: plane0, front->node1, back->leaf0(solid); face0
        node(1, 2, -2, 1, 1),   # node1: plane1; face1
        node(2, 3, -1, 2, 1),   # node2: plane2; face2
        node(3, 4, -1, 3, 1),   # node3: plane3; face3
        node(4, 5, -1, 4, 1),   # node4: plane4; face4
        node(5, -1, -2, 5, 1),  # node5: plane5, front->leaf0(solid), back->leaf1(empty); face5
    ]
    leafs = [
        leaf(SOLID, -1),  # leaf0: always-solid outside leaf (engine requirement)
        leaf(EMPTY, 0),   # leaf1: the room (marks -> all faces)
    ]
    marks = struct.pack('<6H', 0, 1, 2, 3, 4, 5)  # leaf0 sees all 6 faces

    # clipnodes: 3 hulls, each an independent 6-node chain over the same planes
    cl = []
    for h in range(3):
        base = h * 6
        f = lambda i: i if i >= 0 else i   # node child index = absolute
        cl.append(clipnode(0, base + 1, SOLID))
        cl.append(clipnode(1, SOLID, base + 2))
        cl.append(clipnode(2, base + 3, SOLID))
        cl.append(clipnode(3, SOLID, base + 4))
        cl.append(clipnode(4, base + 5, SOLID))
        cl.append(clipnode(5, SOLID, EMPTY))

    # embedded texture
    tex_name = b'AAATEST\0'
    w = h = 8
    m1 = bytes([0x40] * 64); m2 = bytes([0x40] * 16); m3 = bytes([0x40] * 4); m4 = bytes([0x40])
    pal = bytes([0x60, 0x60, 0x60]) * 256
    tdata = m1 + m2 + m3 + m4 + pal
    # dmiptexlump_t: int count; int dataofs[count]; miptex headers; data
    base = 4 + 4 + 40  # count + dataofs[1] + one miptex header
    thdr = struct.pack('<ii', 1, 8) + struct.pack('<16sII4I', tex_name, w, h, base,
                                                  base + len(m1), base + len(m1) + len(m2),
                                                  base + len(m1) + len(m2) + len(m3))
    textures = thdr + tdata

    entities = (
        '{\n"classname" "worldspawn"\n"wad" ""\n"sounds" "1"\n"mapversion" "220"\n}\n'
        '{\n"classname" "info_player_start"\n"origin" "0 0 36"\n"angle" "0"\n}\n'
        # Counter-Strike spawns its T-side players from
        # info_player_deathmatch — without it the CS gamedll fails
        # PutClientInServer ("no info_player_start on level") and any
        # bot connecting on a host test map crashes in its constructor
        '{\n"classname" "info_player_deathmatch"\n"origin" "40 0 36"\n"angle" "0"\n}\n'
        '{\n"classname" "info_player_deathmatch"\n"origin" "-40 0 36"\n"angle" "0"\n}\n'
        '{\n"classname" "light"\n"origin" "0 0 140"\n"light" "400"\n}\n'
        '{\n"classname" "light_environment"\n"origin" "0 0 160"\n"_light" "255 255 255 200"\n}\n'
    ).encode()

    # worldspawn model: hull0 -> node tree (0), hull1/2 -> clipnode roots 6 / 12
    models = struct.pack('<9f4i3i', X0,Y0,Z0, X1,Y1,Z1, 0,0,0, 0, 6, 12, 1, 0, len(faces), 0)

    lumps = {
        L_ENTITIES: entities,
        L_PLANES: b''.join(planes),
        L_TEXTURES: textures,
        L_VERTEXES: vertexes,
        L_VISIBILITY: b'',
        L_NODES: b''.join(nodes),
        L_TEXINFO: texinfo,
        L_FACES: b''.join(faces),
        L_LIGHTING: b'',
        L_CLIPNODES: b''.join(cl),
        L_LEAFS: b''.join(leafs),
        L_MARKSURFACES: marks,
        L_EDGES: b''.join(edges),
        L_SURFEDGES: struct.pack('<%di' % len(surfedges), *surfedges),
        L_MODELS: models,
    }

    header = struct.pack('<I', 30)  # BSP30: version int only, lumps follow immediately
    out = header
    body = b''
    cur = 4 + len(lumps) * 8  # version(4) + 15 x lump(fileofs+filelen = 8 bytes)
    for i in range(len(lumps)):
        d = lumps[i]
        out += struct.pack('<II', cur, len(d))
        pad = (4 - (len(d) & 3)) & 3
        body += d + b'\0' * pad
        cur += len(d) + pad
    return out + body

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'amxx_test.bsp'
    data = build()
    open(out, 'wb').write(data)
    print('wrote %s: %d bytes' % (out, len(data)))
