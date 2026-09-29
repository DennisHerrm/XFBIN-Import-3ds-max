# Prueft, ob jeder std::-Typ im Header auch dort eingebunden ist.
# Genau das ist mit std::map schiefgegangen: der Typ stand im
# Header, das #include nur in der .cpp - und die .cpp bindet den
# Header VOR ihren eigenen Includes ein.
import re
import sys

# Symbole des Max-SDK und der Header, in dem sie stehen.
#
# Dieselbe Falle wie bei den std-Typen, nur eine Ebene weiter: die
# neueren SDKs ziehen fast alles ueber max.h mit, das von 2016
# nicht. Jedes dieser Paare hat hier schon einmal einen
# kompletten Build-Durchlauf ueber zwoelf Versionen gekostet.
#
# Beim Benutzen eines neuen SDK-Symbols hier eintragen - dann
# meldet die Pruefung das fehlende Include, statt es dem
# Compiler zu ueberlassen.
sdk_need = {
    'RegisterNotification':   '<notify.h>',
    'UnRegisterNotification': '<notify.h>',
    'NOTIFY_':                '<notify.h>',
    'SceneImport':            '<impexp.h>',
    'SceneExport':            '<impexp.h>',
    'ClassDesc2':             '<iparamb2.h>',
    'IParamBlock2':           '<iparamb2.h>',
    'ISkinImportData':        '<iskin.h>',
    'ISkin':                  '<iskin.h>',
    'IDerivedObject':         '<modstack.h>',
    'CreateDerivedObject':    '<modstack.h>',
    'StdMat2':                '<stdmat.h>',
    'BitmapTex':              '<stdmat.h>',
    'StdUVGen':               '<stdmat.h>',
    'MeshNormalSpec':         '<MeshNormalSpec.h>',
    'IKeyControl':            '<istdplug.h>',
    'GetKeyControlInterface': '<istdplug.h>',
    'IBezFloatKey':           '<istdplug.h>',
}

need = {
    'std::map': '<map>', 'std::set': '<set>', 'std::vector': '<vector>',
    'std::string': '<string>', 'std::wstring': '<string>',
    'std::unique_ptr': '<memory>', 'std::shared_ptr': '<memory>',
    'std::array': '<array>', 'std::pair': '<utility>',
    'std::function': '<functional>', 'std::unordered_map': '<unordered_map>',
    'std::ostringstream': '<sstream>', 'std::wostringstream': '<sstream>',
    'std::ifstream': '<fstream>', 'std::ofstream': '<fstream>',
}

def includes_of(path):
    try:
        return set(re.findall(r'#include\s+(<[\w.]+>)',
                              open(path, encoding='utf-8').read()))
    except OSError:
        return set()


bad = 0
for path in sys.argv[1:]:
    try:
        s = open(path, encoding='utf-8').read()
    except OSError:
        continue
    body = re.sub(r'//.*', '', s)
    have = includes_of(path)

    # Eine .cpp bekommt die Includes ihres eigenen Headers mit -
    # sonst meldet die Pruefung lauter Treffer, die keine sind.
    if path.endswith('.cpp'):
        have |= includes_of(path[:-4] + '.h')
    # --- swprintf_s mit %s in einen festen Puffer ---
    lines = s.split('\n')
    for i, ln in enumerate(lines):
        if 'swprintf_s(' not in ln:
            continue
        size = None
        for j in range(i, max(0, i - 8), -1):
            m = re.search(r'wchar_t\s+\w+\[(\d+)\]', lines[j])
            if m:
                size = m.group(1)
                break
        if size and '%s' in ' '.join(lines[i:i + 8]):
            print('%s:%d: swprintf_s mit %%s in wchar_t[%s] - '
                  'Zeichenketten aus der Datei haben keine Obergrenze'
                  % (path, i + 1, size))
            bad += 1

    for typ, hdr in need.items():
        if re.search(r'\b' + re.escape(typ) + r'\b', body) and hdr not in have:
            print('%s: benutzt %s, bindet %s aber nicht ein' % (path, typ, hdr))
            bad += 1

    # --- SDK-Symbole gegen ihre Header ---
    for sym, hdr in sdk_need.items():
        pattern = (r'\b' + re.escape(sym) if sym.endswith('_')
                   else r'\b' + re.escape(sym) + r'\b')
        if re.search(pattern, body) and hdr not in have:
            print('%s: benutzt %s, bindet %s aber nicht ein' % (path, sym, hdr))
            bad += 1

if bad:
    print('\n%d fehlende(s) Include.' % bad)
    sys.exit(1)

print('Alle benutzten std-Typen sind eingebunden.')
sys.exit(0)
