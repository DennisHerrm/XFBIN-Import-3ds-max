# ============================================================
#  pydump_anims.py - Referenz-Animationsdump
#
#  Aufruf:
#    python pydump_anims.py <addon-ordner> <datei.xfbin> <dump.txt>
#                           [--no-keys]
#
#  Der Dump entsteht aus ZWEI Quellen derselben Lib:
#    * die Br-Ebene (br_anm.py) liefert die Rohindizes und
#      Kurvenkoepfe,
#    * create_curve_keyframes() aus anm.py wandelt die Rohdaten
#      in Keys um - das ist die Umrechnung, die verglichen wird,
#    * read_xfbin() liefert die ueber die Referenztabelle
#      aufgeloesten Namen.
#
#  Verglichen werden Rohwerte: Zeitpunkt und Zahlen jedes Keys,
#  so wie sie aus der Datei kommen. Die Umrechnung in eine
#  Max-Transformation ist NICHT Teil dieses Vergleichs - Blender
#  rechnet dafuer in Pose-Raum um, Max nicht; die beiden waeren
#  nicht vergleichbar.
# ============================================================

import sys
import os

sys.path.insert(0, sys.argv[1])

from xfbin_lib.xfbin.util import BinaryReader, Endian
from xfbin_lib.xfbin.structure.br.br_xfbin import BrXfbin
from xfbin_lib.xfbin.structure.anm import create_curve_keyframes
from xfbin_lib.xfbin.xfbin_reader import read_xfbin
from xfbin_lib.xfbin.structure.nucc import NuccChunkAnm


def esc(s):
    if s is None:
        return ''
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


def num(v, digits=6):
    v = float(v)
    if v == 0.0:
        v = 0.0
    return '%.*f' % (digits, v)


def channel_of(curve_index, curve_format):
    """Spiegelt BoneChannelOf() aus xfbin_anm.cpp."""
    idx = curve_index + (10 if curve_format in (8, 9) else 0)
    return {0: 0, 1: 1, 2: 2, 3: 3, 11: 4}.get(idx, -1)


def as_list(v):
    if v is None:
        return []
    if isinstance(v, (int, float)):
        return [v]
    return list(v)


def main():
    path = sys.argv[2]
    outp = sys.argv[3]
    include_keys = '--no-keys' not in sys.argv

    with open(path, 'rb') as f:
        data = f.read()
    with BinaryReader(data, Endian.BIG, 'cp932') as br:
        bx = br.read_struct(BrXfbin)

    br_anims = []
    for page in bx.pages:
        for local_idx, ch in page.chunksDict.items():
            if ch.type == 'nuccChunkAnm':
                br_anims.append(ch)

    # Aufgeloeste Namen aus dem regulaeren Lesepfad.
    resolved = []
    for page in read_xfbin(path).pages:
        for ch in page.chunks:
            if isinstance(ch, NuccChunkAnm):
                resolved.append(ch)

    with open(outp, 'w', encoding='utf-8') as o:
        o.write('# XFBIN anim dump v1\n')
        o.write('file %s\n' % os.path.basename(path))
        o.write('anims %d\n' % len(br_anims))

        for ai, a in enumerate(br_anims):
            res = resolved[ai] if ai < len(resolved) else None
            frame_size = a.frame_size if a.frame_size else 100
            frames = a.frame_count / float(frame_size)

            o.write('anim[%d] name=%s frames=%s frameSize=%d loop=%d '
                    'clumps=%d coords=%d entries=%d other=%d/%d\n'
                    % (ai, esc(a.name), num(frames, 2), frame_size,
                       a.loop_flag, len(a.clumps), len(a.coord_parents),
                       len(a.entries), a.other_entry_count,
                       a.other_index_count))

            for ci, cl in enumerate(a.clumps):
                rname = ''
                nbones = len(cl.bones)
                if res is not None and ci < len(res.clumps):
                    rname = res.clumps[ci].name
                    nbones = len(res.clumps[ci].children)
                o.write('clumpref[%d.%d] name=%s bones=%d models=%d\n'
                        % (ai, ci, esc(rname), nbones, len(cl.models)))

            for ei, e in enumerate(a.entries):
                tname = ''
                if res is not None and ei < len(res.entries):
                    tname = getattr(res.entries[ei], 'name', '')

                o.write('entry[%d.%d] clump=%d bone=%d format=%d curves=%d '
                        'target=%s\n'
                        % (ai, ei, e.clump_index, e.bone_index,
                           e.entry_format, len(e.curve_headers), esc(tname)))

                for ki, (hdr, raw) in enumerate(zip(e.curve_headers, e.curves)):
                    o.write('curve[%d.%d.%d] idx=%d fmt=%d keys=%d flags=%d '
                            'ch=%d\n'
                            % (ai, ei, ki, hdr.curve_index, hdr.curve_format,
                               hdr.keyframe_count, hdr.curve_flags,
                               channel_of(hdr.curve_index, hdr.curve_format)))

                    if not include_keys:
                        continue

                    keys = list(create_curve_keyframes(frame_size, hdr, raw))
                    for k in keys:
                        vals = as_list(k.value)
                        o.write('  k %s%s\n'
                                % (num(k.frame / float(frame_size), 4),
                                   ''.join(' ' + num(v) for v in vals)))

        o.write('end\n')

    print('ok %s  (%d Animationen)' % (outp, len(br_anims)))


main()
