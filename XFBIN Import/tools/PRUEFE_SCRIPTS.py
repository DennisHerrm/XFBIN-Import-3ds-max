# ============================================================
#  PRUEFE_SCRIPTS.py - Linter fuer die MaxScript-Dateien
#
#  Aufruf (aus dem Projektordner):
#    python tools\PRUEFE_SCRIPTS.py
#
#  Prueft in scripts\ und tools\ die vier Fehlerarten, die in
#  diesem Projekt tatsaechlich vorgekommen sind - jede davon
#  faellt sonst erst in 3ds Max auf:
#
#    1. Nicht-ASCII-Zeichen. Die Kodierung von .mcr-Dateien ist
#       ueber die Max-Versionen hinweg nicht verlaesslich.
#    2. C-Kommentare (//). MAXScript kommentiert mit --; ein //
#       ergibt "Syntax error: at /, expected <rollout clause>".
#    3. Unausgeglichene Klammern.
#    4. Vorwaertsreferenzen auf fn. MAXScript loest Namen in
#       einem Rollout von oben nach unten auf; eine weiter unten
#       definierte Funktion ist beim Aufruf undefined.
#    5. Ereignishandler fuer Controls, die es nicht gibt - meist
#       ein Tippfehler oder ein umbenanntes Control.
#    6. Aufrufe von XfbinCpp.*, die die Plugin-API nicht kennt.
#       Faellt sonst erst zur Laufzeit auf, und auch nur wenn man
#       den Knopf drueckt.
#    7. "continue" in Schleifen. Gibt es in MAXScript nicht -
#       gewohnt aus anderen Sprachen, und faellt erst auf, wenn
#       die Schleife tatsaechlich laeuft.
#
#  Rueckgabe: 0 = sauber, 1 = Probleme gefunden.
# ============================================================

import re
import glob
import sys


# Die von xfbinimport.h veroeffentlichte MaxScript-API. Beim
# Erweitern des Plugins hier mitpflegen - dann meldet der Linter
# einen Tippfehler im Skript, statt ihn erst im Listener zu zeigen.
PLUGIN_API = set("""
version lastError warnings log timings setDebug
open close isOpen dump
pageCount chunkCount countOfType namesOfType summary
parseSkeleton clumpCount boneCount boneSummary boneDump buildSkeleton
parseMeshes modelCount meshSummary meshDump buildMeshes buildMeshesSkinned
parseAnims parseAnimsAppend clearAnims animCount animName animSummary
animDump buildAnim buildAnimEx
setQuatMode setBoneSize
sceneBoneCount sceneClumpName clearScene sceneReport layerReport buildMaterialAnim
parseTextures textureCount materialCount textureSummary exportTextures
buildMaterials
buildAnimAt animFrames animIsSkeletal animIsSequenceSafe buildBindPoseKey buildIdleKeys buildVisibility
sceneRootName
fileClumpName requiredInstances buildSkeletonN buildMeshesN buildIdleKeys
""".split())


def check(path):
    raw = open(path, encoding='utf-8').read()
    problems = []

    nonascii = [(i + 1, c) for i, ln in enumerate(raw.split('\n'))
                for c in ln if ord(c) > 127]
    if nonascii:
        problems.append('%d Nicht-ASCII-Zeichen (erste Zeile %d)'
                        % (len(nonascii), nonascii[0][0]))

    cstyle = [i + 1 for i, ln in enumerate(raw.split('\n'))
              if re.match(r'^\s*//', ln)]
    if cstyle:
        problems.append('C-Kommentar // in Zeile(n) %s'
                        % ', '.join(str(x) for x in cstyle[:5]))

    src = re.sub(r'/\*.*?\*/', '', raw, flags=re.S)
    clean = '\n'.join(re.sub(r'"(\\.|[^"\\])*"', '""', re.sub(r'--.*$', '', l))
                      for l in src.split('\n'))
    par = clean.count('(') - clean.count(')')
    brk = clean.count('[') - clean.count(']')
    if par:
        problems.append('Klammerbilanz () = %+d' % par)
    if brk:
        problems.append('Klammerbilanz [] = %+d' % brk)

    lines = raw.split('\n')

    defs = {}
    for i, ln in enumerate(lines):
        m = re.match(r'\s*fn (\w+)', ln)
        if m:
            defs[m.group(1)] = i
    for i, ln in enumerate(lines):
        t = re.sub(r'--.*$', '', ln)
        for name, line in defs.items():
            if (re.search(r'\b' + name + r'\s*\(', t)
                    and not re.match(r'\s*fn ' + name, ln)
                    and i < line):
                problems.append('%s in Zeile %d aufgerufen, aber erst in '
                                'Zeile %d definiert' % (name, i + 1, line + 1))

    # --- continue: gibt es in MAXScript nicht ---
    for i, ln in enumerate(lines):
        t = re.sub(r'--.*$', '', ln)
        if re.search(r'\bcontinue\b', t):
            problems.append('Zeile %d: "continue" gibt es in MAXScript nicht'
                            % (i + 1))

    # --- Handler auf existierende Controls ---
    ctrls = set()
    for ln in lines:
        m = re.match(r'\s*(label|button|checkBox|editText|dropDownList|spinner'
                     r'|listBox|comboBox|radioButtons|colorPicker|slider|'
                     r'mapButton|materialButton|pickButton|progressBar|'
                     r'imgTag|bitmap|groupBox|timer)\s+(\w+)', ln)
        if m:
            ctrls.add(m.group(2))
    rollouts = set(re.findall(r'^\s*rollout\s+(\w+)', raw, re.M))
    for i, ln in enumerate(lines):
        m = re.match(r'\s*on (\w+) (\w+) do', ln)
        if m and m.group(1) not in ctrls and m.group(1) not in rollouts:
            problems.append('Zeile %d: Handler fuer unbekanntes Control "%s"'
                            % (i + 1, m.group(1)))

    # --- Aufrufe gegen die Plugin-API ---
    unknown = sorted(set(re.findall(r'XfbinCpp\.(\w+)', raw)) - PLUGIN_API)
    if unknown:
        problems.append('Unbekannte Plugin-Funktion(en): %s'
                        % ', '.join(unknown))

    return problems


def main():
    files = sorted(glob.glob('scripts/*.mcr') + glob.glob('scripts/*.ms')
                   + glob.glob('tools/*.ms'))
    if not files:
        print('Keine Skriptdateien gefunden - aus dem Projektordner starten.')
        return 2

    bad = 0
    for f in files:
        problems = check(f)
        if problems:
            bad += 1
            print('%s:' % f)
            for p in problems:
                print('   %s' % p)
        else:
            print('%-34s ok' % f)

    print()
    if bad:
        print('%d Datei(en) mit Problemen.' % bad)
        return 1
    print('Alle sauber.')
    return 0


sys.exit(main())
