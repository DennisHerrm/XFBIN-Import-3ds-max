# Prueft, ob jeder std::-Typ im Header auch dort eingebunden ist.
# Genau das ist mit std::map schiefgegangen: der Typ stand im
# Header, das #include nur in der .cpp - und die .cpp bindet den
# Header VOR ihren eigenen Includes ein.
import re
import sys

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
    for typ, hdr in need.items():
        if re.search(r'\b' + re.escape(typ) + r'\b', body) and hdr not in have:
            print('%s: benutzt %s, bindet %s aber nicht ein' % (path, typ, hdr))
            bad += 1

if bad:
    print('\n%d fehlende(s) Include.' % bad)
    sys.exit(1)

print('Alle benutzten std-Typen sind eingebunden.')
sys.exit(0)
