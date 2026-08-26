# ============================================================
#  PRUEFE_API.py - Plugin-API auf Vollstaendigkeit pruefen
#
#  Aufruf (aus dem Projektordner):
#    python tools\PRUEFE_API.py
#
#  Eine Funktion muss an DREI Stellen stehen, damit MaxScript sie
#  sieht:
#
#    1. als fn_-Wert im enum            (xfbinimport.h)
#    2. in der BEGIN_FUNCTION_MAP       (xfbinimport.h)
#    3. im Interface-Deskriptor         (xfbinimport.cpp)
#
#  Fehlt die dritte, uebersetzt alles anstandslos - die Funktion
#  ist nur einfach nicht da. In MaxScript sieht das genauso aus
#  wie ein zu altes Plugin: "Unknown property". Genau so sind
#  clearAnims und parseAnimsAppend in 1.7.0 durchgerutscht.
#
#  Geprueft wird ausserdem, ob die Funktionsnamen, die die
#  Skripte beim Start abfragen, tatsaechlich veroeffentlicht sind.
#
#  Rueckgabe: 0 = sauber, 1 = Probleme gefunden.
# ============================================================

import re
import sys


def read(path):
    return open(path, encoding='utf-8').read()


def main():
    try:
        h = read('src/xfbinimport.h')
        c = read('src/xfbinimport.cpp')
    except OSError:
        print('Quelldateien nicht gefunden - aus dem Projektordner starten.')
        return 2

    problems = []

    # Der erste Eintrag steht als "fn_open = 0," da - die
    # Zuweisung muss das Muster mit abdecken.
    enum_ids = set(re.findall(r'^\s*(fn_\w+)\s*(?:=\s*\d+\s*)?,', h, re.M))
    mapped   = set(re.findall(r'FN_\d\((fn_\w+),', h))
    descr    = set(re.findall(r'^\s*(fn_\w+), _T\("', c, re.M))

    print('enum: %d   FUNCTION_MAP: %d   Deskriptor: %d'
          % (len(enum_ids), len(mapped), len(descr)))

    for name in sorted(mapped - descr):
        problems.append('%s steht in der FUNCTION_MAP, aber nicht im '
                        'Deskriptor - MaxScript sieht sie nicht' % name)

    for name in sorted(descr - mapped):
        problems.append('%s steht im Deskriptor, aber nicht in der '
                        'FUNCTION_MAP' % name)

    for name in sorted(mapped - enum_ids):
        problems.append('%s fehlt im enum' % name)

    # Namen, die die Skripte beim Start abfragen
    published = set(re.findall(r'_T\("(\w+)"\), 0, TYPE_', c))

    for f in ('scripts/XfbinImport.mcr', 'scripts/XFBIN_Import.ms'):
        try:
            s = read(f)
        except OSError:
            continue
        m = re.search(r'#\(#\w+.*?\)', s, re.S)
        if not m:
            continue
        for n in re.findall(r'#(\w+)', m.group(0)):
            if n not in published:
                problems.append('%s prueft auf "%s", das aber nicht '
                                'veroeffentlicht ist' % (f, n))

    print()
    if problems:
        for p in problems:
            print('   %s' % p)
        print('\n%d Problem(e).' % len(problems))
        return 1

    print('Alle Funktionen sind an allen drei Stellen eingetragen.')
    return 0


sys.exit(main())
