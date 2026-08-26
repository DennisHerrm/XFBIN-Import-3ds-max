# ============================================================
#  VERSION_SETZEN.py - alle Versionsstellen auf einmal
#
#  Aufruf (aus dem Projektordner):
#    python tools\\VERSION_SETZEN.py 1.8.0
#
#  Die Versionsnummer steht an sechs Stellen: im Plugin-Header,
#  in der PackageContents.xml, in zwei Batchdateien und in den
#  MaxScript-Kopfzeilen - dazu die Version, die das Skript vom
#  Plugin ERWARTET. Laufen die auseinander, meldet die
#  Oberflaeche beim Start einen Versionskonflikt, und im
#  schlimmsten Fall spricht ein neues Skript mit einem alten
#  Plugin.
#
#  Setzt ausserdem einen neuen ProductCode: laut Autodesk-Doku
#  gehoert der bei jeder Aenderung von AppVersion neu vergeben.
#  UpgradeCode bleibt unveraendert - er ist die dauerhafte
#  Kennung des Pakets.
# ============================================================

import re
import sys
import glob
import uuid


def read(path, nl=None):
    return open(path, encoding='utf-8', newline=nl).read()


def write(path, text, nl=None):
    open(path, 'w', encoding='utf-8', newline=nl).write(text)


def main():
    if len(sys.argv) != 2:
        print('Aufruf: python tools\\VERSION_SETZEN.py <version>')
        return 2

    new = sys.argv[1]
    if not re.match(r'^\d+\.\d+\.\d+$', new):
        print('Version muss die Form 1.8.0 haben.')
        return 2

    # Jede Stelle wird GESETZT, nicht ersetzt.
    #
    # Vorher lief das ueber "alte Nummer suchen, neue einsetzen".
    # Das geht schief, sobald eine Stelle aus irgendeinem Grund
    # nicht auf der alten Nummer steht - dann findet die Suche
    # nichts, meldet aber auch nichts, und die Stelle bleibt
    # zurueck. Genau so stand der Plugin-Header fuenf Versionen
    # lang auf einem alten Wert, waehrend alles andere weiterlief.
    header = read('src/xfbinimport.h')
    old = re.search(r'XFBINIMPORT_VERSION_STR\s+_T\("([\d.]+)"\)', header).group(1)
    print('%s -> %s' % (old, new))

    write('src/xfbinimport.h',
          re.sub(r'(XFBINIMPORT_VERSION_STR\s+_T\(")[\d.]+("\))',
                 r'\g<1>%s\g<2>' % new, header))

    xml = read('package/XfbinImport/PackageContents.xml')
    xml = re.sub(r'(AppVersion=")[\d.]+(")',      r'\g<1>%s\g<2>' % new, xml)
    xml = re.sub(r'(FriendlyVersion=")[\d.]+(")', r'\g<1>%s\g<2>' % new, xml)
    xml = re.sub(r'(Version=")[\d.]+(")',         r'\g<1>%s\g<2>' % new, xml)
    xml = re.sub(r'ProductCode="\{[^}]+\}"',
                 'ProductCode="{%s}"' % str(uuid.uuid4()).upper(), xml)
    write('package/XfbinImport/PackageContents.xml', xml)

    for f in ('BAUE_ALLE.bat', 'INSTALLIERE.bat'):
        t = read(f, '')
        t = re.sub(r'(XFBIN Import )[\d.]+', r'\g<1>%s' % new, t)
        write(f, t, '')

    for f in glob.glob('scripts/*.mcr') + glob.glob('scripts/*.ms'):
        t = read(f)
        t = re.sub(r'(Version )[\d.]+', r'\g<1>%s' % new, t)
        write(f, t)

    print('Alle Stellen gesetzt. ProductCode neu vergeben.')
    # Gegenprobe: stehen jetzt wirklich ueberall dieselben Zahlen?
    found = {
        'src/xfbinimport.h':
            re.search(r'_T\("([\d.]+)"\)', read('src/xfbinimport.h')).group(1),
        'PackageContents.xml':
            re.search(r'AppVersion="([\d.]+)"',
                      read('package/XfbinImport/PackageContents.xml')).group(1),
        'XfbinImport.mcr (Kopfzeile)':
            re.search(r'Version ([\d.]+)',
                      read('scripts/XfbinImport.mcr')).group(1),
    }

    bad = {k: v for k, v in found.items() if v != new}
    if bad:
        print('FEHLER - diese Stellen stimmen nicht:')
        for k, v in bad.items():
            print('   %-28s %s' % (k, v))
        return 1

    for k, v in found.items():
        print('   %-28s %s' % (k, v))

    print('Nicht vergessen: BAUE_ALLE.bat, sonst passt die .dlu nicht.')
    return 0


sys.exit(main())
