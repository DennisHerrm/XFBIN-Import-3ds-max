# ============================================================
#  pydump.py - Referenz-Dump aus der Python-Lib
#
#  Erzeugt exakt dasselbe Zeilenformat wie xfbindump.exe, damit
#  sich beide Implementierungen mit einem simplen diff
#  vergleichen lassen.
#
#  Aufruf:
#    python pydump.py <addon-ordner> <datei.xfbin> <dump.txt>
#
#  <addon-ordner> ist der entpackte Blender-XFBIN-Importer,
#  also der Ordner, der xfbin_lib\ enthaelt.
#
#  BEKANNTER UNTERSCHIED (kein Fehler):
#  Die erste Page jeder Datei enthaelt zwei nuccChunkNull. Die
#  Python-Lib legt die Chunks einer Page in einem dict ab, das
#  ueber den page-lokalen Map-Index geht - beide Nulls haben
#  Index 0, der zweite ueberschreibt also den ersten. Der Dump
#  von xfbindump.exe zeigt beide. Erwartete Abweichung:
#  genau eine Zeile mehr im C++-Dump, in page[0].
# ============================================================

import sys, os
sys.path.insert(0, sys.argv[1])
from xfbin_lib.xfbin.util import BinaryReader, Endian
from xfbin_lib.xfbin.structure.br.br_xfbin import BrXfbin

def esc(s):
    if isinstance(s, str):
        b = s.encode('cp932', errors='replace')
    else:
        b = s
    out = []
    for c in b:
        if c == 0x5C: out.append('\\\\')
        elif 0x20 <= c < 0x7F: out.append(chr(c))
        else: out.append('\\x%02X' % c)
    return ''.join(out)

path = sys.argv[2]
outp = sys.argv[3]
with open(path,'rb') as f: data = f.read()
with BinaryReader(data, Endian.BIG, 'cp932') as br:
    bx = br.read_struct(BrXfbin)

t = bx.chunkTable
o = open(outp,'w',encoding='utf-8')
w = o.write
w("# XFBIN dump v1\n")
w("file %s\n" % os.path.basename(path))
h = bx.header
w("header nuccId=%d chunkTableSize=%d minPageSize=%d nuccId2=%d unk=%d\n" %
  (h.nuccId, h.chunkTableSize, h.minPageSize, h.nuccId2, h.unk))
w("table types=%d paths=%d names=%d maps=%d indices=%d refs=%d\n" %
  (len(t.chunkTypes), len(t.filePaths), len(t.chunkNames), len(t.chunkMaps),
   len(t.chunkMapIndices), len(t.chunkMapReferences)))
for i,s in enumerate(t.chunkTypes): w("type[%d] %s\n" % (i, esc(s)))
for i,s in enumerate(t.filePaths):  w("path[%d] %s\n" % (i, esc(s)))
for i,s in enumerate(t.chunkNames): w("name[%d] %s\n" % (i, esc(s)))
for i,m in enumerate(t.chunkMaps):
    w("map[%d] type=%d path=%d name=%d\n" % (i, m.chunkTypeIndex, m.filePathIndex, m.chunkNameIndex))
for i,r in enumerate(t.chunkMapReferences):
    w("ref[%d] name=%d map=%d\n" % (i, r.chunkNameIndex, r.chunkMapIndex))
for i,v in enumerate(t.chunkMapIndices): w("idx[%d] %d\n" % (i, v))

for p, page in enumerate(bx.pages):
    chunks = list(page.chunksDict.items())
    w("page[%d] chunks=%d pageSize=%d refSize=%d\n" %
      (p, len(chunks), page.pageChunk.pageSize, page.pageChunk.referenceSize))
    for c,(localIdx, ch) in enumerate(chunks):
        w("chunk[%d.%d] type=%s name=%s path=%s ver=%d unk=%d size=%d localMap=%d\n" %
          (p, c, esc(ch.type), esc(ch.name), esc(ch.filePath),
           ch.version, ch.anmvalue, len(ch.data), localIdx))
w("end\n")
o.close()
print("ok", outp)
