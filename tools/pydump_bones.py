# ============================================================
#  pydump_bones.py - Referenz-Skelettdump
#
#  Liest die Datei mit der Python-Lib des Blender-Importers und
#  rechnet die Bone-Matrizen so, wie es blender/importer.py
#  macht - also in SPALTENvektor-Konvention mit mathutils-Logik,
#  hier mit numpy nachgebaut:
#
#      rot   = Euler(radians(rot), 'ZYX')   -> M_col = Rx @ Ry @ Rz
#      local = Translation(p) @ R @ Scale(s)
#      world = parent_world @ local
#
#  Ausgegeben wird die TRANSPONIERTE Form, damit sie sich direkt
#  mit dem Zeilenvektor-Dump aus xfbindump.exe --bones-o diffen
#  laesst.
#
#  UNTERSCHIED ZUM BLENDER-IMPORT, bewusst:
#  Blender rechnet Positionen mit pos_cm_to_m um, multipliziert
#  also mit 0.01. Hier NICHT - das Max-Plugin behaelt Zentimeter.
#  Die Translationen sind deshalb 100x groesser als das, was in
#  Blender im N-Panel steht.
#
#  Aufruf:
#    python pydump_bones.py <addon-ordner> <datei.xfbin> <dump.txt>
# ============================================================

import sys
import os
import math
import numpy as np

sys.path.insert(0, sys.argv[1])

from xfbin_lib.xfbin.util import BinaryReader, Endian
from xfbin_lib.xfbin.structure.br.br_xfbin import BrXfbin


def esc(s):
    b = s.encode('cp932', errors='replace') if isinstance(s, str) else s
    out = []
    for c in b:
        if c == 0x5C:
            out.append('\\\\')
        elif 0x20 <= c < 0x7F:
            out.append(chr(c))
        else:
            out.append('\\x%02X' % c)
    return ''.join(out)


def num(v):
    v = float(v)
    if v == 0.0:
        v = 0.0                      # -0 zu 0 normalisieren
    return '%.6f' % v


# --- Spaltenvektor-Rotationsmatrizen, wie mathutils sie baut ---

def rot_x(deg):
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]], dtype=np.float64)


def rot_y(deg):
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]], dtype=np.float64)


def rot_z(deg):
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]], dtype=np.float64)


def euler_zyx(rot):
    """Euler(rot, 'ZYX') heisst: Z zuerst anwenden.
    In Spaltenschreibweise steht das zuerst Angewendete RECHTS."""
    return rot_x(rot[0]) @ rot_y(rot[1]) @ rot_z(rot[2])


def loc_rot_scale(pos, rot, scl):
    """mathutils Matrix.LocRotScale: T @ R @ S"""
    m = np.eye(4, dtype=np.float64)
    m[:3, :3] = euler_zyx(rot) @ np.diag(scl)
    m[:3, 3] = pos
    return m


def read_coord(chunk):
    br = BinaryReader(chunk.data, Endian.BIG, 'cp932')
    pos = br.read_float32(3)
    rot = br.read_float32(3)
    scl = br.read_float32(3)
    opacity = br.read_float32()
    flags = br.read_uint16() if chunk.version > 0x66 else 0
    return pos, rot, scl, opacity, flags


def main():
    path = sys.argv[2]
    outp = sys.argv[3]

    with open(path, 'rb') as f:
        data = f.read()
    with BinaryReader(data, Endian.BIG, 'cp932') as br:
        bx = br.read_struct(BrXfbin)

    clumps = []

    for page in bx.pages:
        chunks = list(page.chunksDict.items())
        by_local = {}
        for local_idx, ch in chunks:
            if ch.type == 'nuccChunkCoord':
                by_local[local_idx] = ch

        for local_idx, ch in chunks:
            if ch.type != 'nuccChunkClump':
                continue

            cr = BinaryReader(ch.data, Endian.BIG, 'cp932')
            field00 = cr.read_uint32()
            coord_count = cr.read_uint16()
            model_groups = cr.read_uint8()
            extra_groups = cr.read_uint8()
            if field00 == 2:
                cr.read_float32(6)
                cr.read_uint32()

            parents = [cr.read_int16() for _ in range(coord_count)]
            coord_idx = [cr.read_uint32() for _ in range(coord_count)]

            nodes = []
            for i in range(coord_count):
                cc = by_local.get(coord_idx[i])
                if cc is None:
                    nodes.append(None)
                    continue
                pos, rot, scl, opacity, flags = read_coord(cc)
                nodes.append({
                    'name': cc.name, 'pos': pos, 'rot': rot, 'scl': scl,
                    'opacity': opacity, 'flags': flags,
                    'parent': parents[i] if parents[i] >= 0 else -1,
                })

            # Weltmatrizen, Eltern zuerst
            world = [None] * coord_count

            def solve(i, guard=0):
                if world[i] is not None:
                    return world[i]
                if guard > coord_count:
                    return np.eye(4)
                n = nodes[i]
                local = loc_rot_scale(n['pos'], n['rot'], n['scl'])
                if n['parent'] < 0:
                    world[i] = local
                else:
                    world[i] = solve(n['parent'], guard + 1) @ local
                return world[i]

            for i in range(coord_count):
                if nodes[i] is not None:
                    solve(i)

            def depth(i):
                d, g = 0, 0
                while nodes[i]['parent'] >= 0 and g <= coord_count:
                    i = nodes[i]['parent']
                    d += 1
                    g += 1
                return d

            clumps.append({
                'name': ch.name, 'nodes': nodes, 'world': world,
                'depth': depth, 'field00': field00,
                'model_groups': model_groups, 'extra_groups': extra_groups,
                'roots': sum(1 for n in nodes if n and n['parent'] < 0),
            })

    with open(outp, 'w', encoding='utf-8') as o:
        o.write('# XFBIN bone dump v1\n')
        o.write('file %s\n' % os.path.basename(path))
        o.write('clumps %d\n' % len(clumps))

        for c, cl in enumerate(clumps):
            o.write('clump[%d] name=%s nodes=%d roots=%d '
                    'modelGroups=%d extraGroups=%d field00=%d\n'
                    % (c, esc(cl['name']), len(cl['nodes']), cl['roots'],
                       cl['model_groups'], cl['extra_groups'], cl['field00']))

            for i, n in enumerate(cl['nodes']):
                o.write('bone[%d.%d] name=%s parent=%d depth=%d flags=%d\n'
                        % (c, i, esc(n['name']), n['parent'],
                           cl['depth'](i), n['flags']))
                o.write('  pos %s %s %s\n' % tuple(num(v) for v in n['pos']))
                o.write('  rot %s %s %s\n' % tuple(num(v) for v in n['rot']))
                o.write('  scl %s %s %s  opacity %s\n'
                        % (num(n['scl'][0]), num(n['scl'][1]),
                           num(n['scl'][2]), num(n['opacity'])))

                # 4x4 Spaltenform -> 4x3 Zeilenform:
                # Basis transponieren, Translation in Zeile 3.
                w = cl['world'][i]
                r = w[:3, :3].T
                for k in range(3):
                    o.write('  world%d %s %s %s\n'
                            % (k, num(r[k, 0]), num(r[k, 1]), num(r[k, 2])))
                o.write('  world3 %s %s %s\n'
                        % (num(w[0, 3]), num(w[1, 3]), num(w[2, 3])))

        o.write('end\n')

    print('ok %s  (%d Clumps)' % (outp, len(clumps)))


main()
