# ============================================================
#  PRUEFE_MELDUNGEN.py - Meldungsaufrufe uebersetzen
#
#  Aufruf (aus dem Projektordner):
#    python tools\PRUEFE_MELDUNGEN.py
#
#  Schneidet die Msg-Klasse und jeden Log()- bzw.
#  AddWarning()-Aufruf, der sie benutzt, aus dem Quelltext,
#  ersetzt die Bezeichner durch Platzhalter passenden Typs und
#  laesst den Compiler darueber laufen.
#
#  Warum: die Meldungen werden nur beim Bauen gegen ein Max-SDK
#  uebersetzt. Ein vergessenes .str() oder ein Typ, den der
#  Stream nicht kennt, faellt sonst erst nach zwoelf SDK-Builds
#  auf - obwohl an der Stelle nichts steht, was das Max SDK
#  braeuchte.
#
#  Setzt einen C++-Compiler im Pfad voraus (g++ oder cl).
#  Ist keiner da, wird die Pruefung uebersprungen statt zu
#  scheitern.
#
#  Rueckgabe: 0 = sauber oder uebersprungen, 1 = Fehler.
# ============================================================

import os
import re
import shutil
import subprocess
import sys
import tempfile

SRC = 'src/xfbinimport.cpp'

# Bezeichner aus dem Plugin -> Platzhalter passenden Typs
REPL = [
    (r'Cp932ToWide\([^()]*\)', 'sv'),
    (r'[A-Za-z_][\w:.]*\.size\(\)\s*-\s*[A-Za-z_]\w*', 'zv'),
    (r'[A-Za-z_][\w:.]*\.size\(\)', 'zv'),
    (r'static_cast<\w+>\([^()]*\)', 'iv'),
    (r'\bms\w+_\b', 'dv'),
    (r'\b(wout|wdir|firstUnmatched|firstFail|firstFailure|firstMissing|'
     r'skinText)\b', 'sv'),
    (r'\b(total|bones|verts|tris|keys|before)\b', 'zv'),
    (r'\b(made|skipped|skinned|dupes|failed|unmatched|foreignClump|missing|'
     r'rejected|weighted|touched|hidden|keysSet|n)\b', 'iv'),
    (r'\banm\.frameCount\b|\bstartFrame\b|\bscale\b', 'dv'),
    (r'\bchannelMask\b|\bexplicitNormals\b', 'iv'),
    (r'\bok\b', 'bv'),
]


def find_compiler():
    for name in ('g++', 'clang++'):
        if shutil.which(name):
            return [name, '-std=c++17', '-fsyntax-only', '-Wall']
    if shutil.which('cl'):
        return ['cl', '/std:c++17', '/Zs', '/nologo', '/EHsc']
    return None


def main():
    cc = find_compiler()
    if cc is None:
        print('Kein C++-Compiler im Pfad - Pruefung uebersprungen.')
        return 0

    try:
        raw = open(SRC, encoding='utf-8').read()
    except OSError:
        print('%s nicht gefunden - aus dem Projektordner starten.' % SRC)
        return 2

    body_src = re.sub(r'//[^\n]*', '', raw)

    m = re.search(r'class Msg \{.*?\n\};', raw, re.S)
    if m is None:
        print('Msg-Klasse nicht gefunden - nichts zu pruefen.')
        return 0
    msgclass = m.group(0)

    calls = []
    for mm in re.finditer(r'\b(Log|AddWarning)\(', body_src):
        i = mm.end() - 1
        depth = 0
        j = i
        while j < len(body_src):
            if body_src[j] == '(':
                depth += 1
            elif body_src[j] == ')':
                depth -= 1
                if depth == 0:
                    break
            j += 1
        expr = body_src[i:j + 1]
        if 'Msg()' in expr:
            calls.append((mm.group(1), expr))

    if not calls:
        print('Keine Meldungen mit Msg gefunden.')
        return 0

    lines = []
    for fn, expr in calls:
        e = expr
        for pat, rep in REPL:
            e = re.sub(pat, rep, e)
        lines.append('    %s%s;' % ('Log' if fn == 'Log' else 'Warn', e))

    prog = ('#include <sstream>\n#include <string>\n#include <cstddef>\n'
            'namespace {\n%s\n}\n'
            'void Log(const std::wstring&) {}\n'
            'void Warn(const std::wstring&) {}\n'
            'int main() {\n'
            '    std::wstring sv = L"s"; double dv = 1.0;\n'
            '    size_t zv = 1; int iv = 1; bool bv = true;\n'
            '    (void)sv; (void)dv; (void)zv; (void)iv; (void)bv;\n'
            '%s\n}\n' % (msgclass, '\n'.join(lines)))

    tmp = os.path.join(tempfile.gettempdir(), 'xfbin_msgcheck.cpp')
    open(tmp, 'w', encoding='utf-8').write(prog)

    r = subprocess.run(cc + [tmp], capture_output=True, text=True)

    if r.returncode == 0:
        print('Alle %d Meldungen uebersetzen.' % len(calls))
        return 0

    print('Fehler in den Meldungen:\n')
    print((r.stderr or r.stdout)[:2000])
    return 1


sys.exit(main())
