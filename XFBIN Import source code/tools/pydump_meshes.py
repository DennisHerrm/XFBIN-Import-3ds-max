# ============================================================
#  pydump_meshes.py - Referenz-Meshdump
#
#  Liest die NUD-Meshes mit der Python-Lib des Blender-Importers
#  und gibt sie im selben Zeilenformat aus wie
#  xfbindump.exe --meshes-o, damit sich beide diffen lassen.
#
#  Aufruf:
#    python pydump_meshes.py <addon-ordner> <datei.xfbin> <dump.txt>
#                            [--no-verts]
#
#  HINWEIS zu Farben: bei uvColorType & 4 (Halbgleitkomma-Farben)
#  runden die beiden Implementierungen unterschiedlich. Die
#  Testdateien benutzen durchgehend Byte-Farben, deshalb faellt
#  das hier nicht auf - bei einer Datei mit f16-Farben waere eine
#  Abweichung in dieser Spalte zu erwarten und kein Fehler.
# ============================================================

import sys
import os
import numpy as np

sys.path.insert(0, sys.argv[1])

from xfbin_lib.xfbin.util import BinaryReader, Endian
from xfbin_lib.xfbin.structure.br.br_xfbin import BrXfbin
from xfbin_lib.xfbin.structure.br.br_nud import BrNud
from xfbin_lib.xfbin.structure.nud import Nud


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
        v = 0.0
    return '%.6f' % v


def read_model_header(ch):
    """Kopf des nuccChunkModel, versionsabhaengig - wie br_nucc.py."""
    br = BinaryReader(ch.data, Endian.BIG, 'cp932')
    ver = ch.version
    if 0x73 < ver < 0x76:
        br.read_uint16()
        rigging = br.read_uint16()
        attrs = br.read_uint16()
        br.read_uint16()
        clump = br.read_uint32()
        hit = br.read_uint32()
        bone = br.read_uint32()
        nud_size = br.read_uint32()
        br.read_uint16()
        render = br.read_uint8()
        lightmode = br.read_uint8()
    else:
        br.read_uint16()
        rigging = br.read_uint16()
        attrs = br.read_uint16()
        render = br.read_uint8()
        lightmode = br.read_uint8()
        if ver > 0x73:
            br.read_uint32()
        clump = br.read_uint32()
        hit = br.read_uint32()
        bone = br.read_uint32()
        nud_size = br.read_uint32()

    bbox = None
    if attrs & 0x04:
        bbox = br.read_float32(6)

    nud_pos = br.pos()
    return dict(rigging=rigging, attrs=attrs, clump=clump, hit=hit,
                bone=bone, nud_size=nud_size, render=render,
                lightmode=lightmode, bbox=bbox, nud_pos=nud_pos)


def main():
    path = sys.argv[2]
    outp = sys.argv[3]
    include_verts = '--no-verts' not in sys.argv

    with open(path, 'rb') as f:
        data = f.read()
    with BinaryReader(data, Endian.BIG, 'cp932') as br:
        bx = br.read_struct(BrXfbin)

    models = []

    for page in bx.pages:
        for local_idx, ch in page.chunksDict.items():
            if ch.type != 'nuccChunkModel':
                continue

            hdr = read_model_header(ch)
            if hdr['nud_size'] == 0:
                continue

            nud_bytes = ch.data[hdr['nud_pos']:hdr['nud_pos'] + hdr['nud_size']]
            br_nud = BinaryReader(nud_bytes, Endian.BIG).read_struct(BrNud)

            nud = Nud()
            nud.init_data(ch.name, br_nud)

            # Materialindizes hinter dem NUD-Block
            mbr = BinaryReader(ch.data, Endian.BIG, 'cp932')
            mbr.seek(hdr['nud_pos'] + hdr['nud_size'])
            mat_count = mbr.read_uint16()

            models.append(dict(name=ch.name, nud=nud, hdr=hdr,
                               br_nud=br_nud, mats=mat_count))

    with open(outp, 'w', encoding='utf-8') as o:
        o.write('# XFBIN mesh dump v1\n')
        o.write('file %s\n' % os.path.basename(path))
        o.write('models %d\n' % len(models))

        for mi, mo in enumerate(models):
            nud = mo['nud']
            hdr = mo['hdr']
            bn = mo['br_nud']

            verts = sum(len(m.vertices) for g in nud.mesh_groups for m in g.meshes)
            tris = sum(len(m.faces) for g in nud.mesh_groups for m in g.meshes)

            o.write('model[%d] name=%s groups=%d verts=%d tris=%d '
                    'rigging=%d attrs=%d clumpIdx=%d meshBone=%d '
                    'boneRange=%d..%d materials=%d\n'
                    % (mi, esc(mo['name']), len(nud.mesh_groups), verts, tris,
                       hdr['rigging'], hdr['attrs'], hdr['clump'], hdr['bone'],
                       bn.boneStart, bn.boneEnd, mo['mats']))

            for gi, g in enumerate(nud.mesh_groups):
                brg = bn.meshGroups[gi]

                # singleBind liest die Lib als uint16, der Wert ist
                # aber vorzeichenbehaftet: 0xFFFF heisst "keine
                # Einzelbindung", und der Exporter schreibt dort
                # write_int16(-1). Hier deshalb als int16 deuten,
                # damit der Diff sauber bleibt.
                single_bind = brg.singleBind
                if single_bind >= 0x8000:
                    single_bind -= 0x10000

                o.write('group[%d.%d] name=%s meshes=%d boneFlags=%d '
                        'singleBind=%d\n'
                        % (mi, gi, esc(g.name), len(g.meshes),
                           g.bone_flags, single_bind))

                for si, m in enumerate(g.meshes):
                    names = m.vertices.dtype.names
                    has_n = 'normal' in names
                    has_c = 'color' in names
                    has_b = 'bone_ids' in names
                    uv_ch = sum(1 for k in range(4) if ('uv%d' % k) in names)

                    brm = bn.meshGroups[gi].meshes[si]

                    # Streifen und entartete Dreiecke nachrechnen -
                    # die Lib merkt sich das nicht.
                    raw = np.array(brm.faces, dtype=np.int32)
                    strips, degen, cur = 0, 0, []
                    for v in raw:
                        if v == 0xFFFF:
                            if len(cur) >= 3:
                                strips += 1
                                for k in range(len(cur) - 2):
                                    a, b, c = cur[k], cur[k + 1], cur[k + 2]
                                    if a == b or b == c or a == c:
                                        degen += 1
                            cur = []
                        else:
                            cur.append(int(v))
                    if len(cur) >= 3:
                        strips += 1
                        for k in range(len(cur) - 2):
                            a, b, c = cur[k], cur[k + 1], cur[k + 2]
                            if a == b or b == c or a == c:
                                degen += 1

                    o.write('mesh[%d.%d.%d] verts=%d tris=%d vFlags=%d '
                            'uvFlags=%d uvCh=%d normal=%d color=%d bones=%d '
                            'strips=%d degen=%d\n'
                            % (mi, gi, si, len(m.vertices), len(m.faces),
                               brm.vertexFlags, brm.uvColorFlags, uv_ch,
                               1 if has_n else 0, 1 if has_c else 0,
                               1 if has_b else 0, strips, degen))

                    if not include_verts:
                        continue

                    V = m.vertices
                    for v in range(len(V)):
                        line = ['  v%d p %s %s %s'
                                % (v, num(V['position'][v][0]),
                                   num(V['position'][v][1]),
                                   num(V['position'][v][2]))]
                        if has_n:
                            line.append(' n %s %s %s'
                                        % (num(V['normal'][v][0]),
                                           num(V['normal'][v][1]),
                                           num(V['normal'][v][2])))
                        if has_c:
                            line.append(' c %d %d %d %d'
                                        % tuple(int(x) for x in V['color'][v]))
                        for u in range(uv_ch):
                            t = V['uv%d' % u][v]
                            line.append(' t%d %s %s' % (u, num(t[0]), num(t[1])))
                        if has_b:
                            line.append(' b ' + ' '.join(
                                str(int(x)) for x in V['bone_ids'][v]))
                            line.append(' w ' + ' '.join(
                                num(x) for x in V['bone_weights'][v]))
                        o.write(''.join(line) + '\n')

                    for f in m.faces:
                        o.write('  f %d %d %d\n' % (f[0], f[1], f[2]))

        o.write('end\n')

    print('ok %s  (%d Modelle)' % (outp, len(models)))


main()
