// ============================================================
//  XFBIN Spiel - Implementierung. Siehe xfbin_spiel.h.
// ============================================================

#include "xfbin_spiel.h"
#include "xfbin_reader.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace xfbin {

std::string Klein(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string DateiName(const std::string& pfad) {
    const size_t p = pfad.find_last_of("/\\");
    return p == std::string::npos ? pfad : pfad.substr(p + 1);
}

namespace {

std::wstring KleinW(std::wstring s) {
    for (wchar_t& c : s) c = static_cast<wchar_t>(std::towlower(c));
    return s;
}

std::string Utf8(const std::wstring& w) {
#ifdef _WIN32
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(std::max(0, n)), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
#else
    return std::string(w.begin(), w.end());
#endif
}

std::wstring Breit(const std::string& s) {
#ifdef _WIN32
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(std::max(0, n)), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &w[0], n);
    return w;
#else
    return std::wstring(s.begin(), s.end());
#endif
}

bool EndetMit(const std::string& s, const std::string& e) {
    return s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0;
}
bool BeginntMit(const std::string& s, const std::string& a) { return s.compare(0, a.size(), a) == 0; }

// Archive, in denen keine Modelle stehen. Die Filmarchive sind
// bis zu 16 GB gross - schon das Inhaltsverzeichnis waere hier
// verschwendete Zeit.
bool UnwichtigesArchiv(const std::string& name) {
    for (const char* p : { "movie", "adx2", "sound", "customtheme" })
        if (BeginntMit(name, p)) return true;
    return false;
}

// Rang eines Archivs - hoeher gewinnt. Patches vor DLC vor
// Grundspiel; innerhalb gleicher Stufe die Plattformfassung
// ("_win32") vor der allgemeinen.
long Rang(const std::string& rel) {
    long r = 100;
    auto zahlNach = [&](const std::string& marke) -> long {
        const size_t p = rel.find(marke);
        if (p == std::string::npos) return -1;
        size_t q = p + marke.size();
        long v = 0;
        bool ziffer = false;
        while (q < rel.size() && std::isdigit(static_cast<unsigned char>(rel[q]))) { v = v * 10 + (rel[q] - '0'); ++q; ziffer = true; }
        return ziffer ? v : 0;
    };
    const long patch = zahlNach("patch/");
    const long dlc   = BeginntMit(rel, "dlc/") ? zahlNach("dlc/") : zahlNach("/dlc/");
    if (patch >= 0)    r = 300 + patch;
    else if (dlc >= 0) r = 200 + dlc;
    r *= 10;
    const std::string name = DateiName(rel);
    if (name.find("_win32") != std::string::npos || name.find("_win64") != std::string::npos) r += 5;
    return r;
}

// Pfad einer losen Datei im Bestand: ab "data/", sonst als
// data/spc/<name> - so liegen ausgepackte Figuren meist vor.
std::string LoserSchluessel(const std::string& rel) {
    const std::string k = Klein(rel);
    const size_t p = k.rfind("data/");
    if (p != std::string::npos) return k.substr(p);
    return "data/spc/" + DateiName(k);
}

std::string RelPfad(const fs::path& wurzel, const fs::path& p) {
    std::error_code ec;
    std::string rel = Utf8(fs::relative(p, wurzel, ec).wstring());
    if (ec) rel = Utf8(p.filename().wstring());
    std::replace(rel.begin(), rel.end(), '\\', '/');
    return Klein(rel);
}

template <typename F>
void Durchlaufe(const std::wstring& ordner, int tiefe, F&& f) {
    std::error_code ec;
    fs::recursive_directory_iterator it(ordner, fs::directory_options::skip_permission_denied, ec), ende;
    for (; !ec && it != ende; it.increment(ec)) {
        if (it.depth() > tiefe) { it.disable_recursion_pending(); continue; }
        if (it->is_regular_file(ec)) f(it->path());
    }
}

#ifdef _WIN32
std::wstring Registry(HKEY wurzel, const wchar_t* schluessel, const wchar_t* wert) {
    wchar_t puffer[1024] = {};
    DWORD groesse = sizeof(puffer);
    if (RegGetValueW(wurzel, schluessel, wert, RRF_RT_REG_SZ, nullptr, puffer, &groesse) == ERROR_SUCCESS) return puffer;
    return std::wstring();
}
#endif

// Steam-Bibliotheken: der Steam-Ordner selbst plus alles, was
// steamapps/libraryfolders.vdf unter "path" nennt.
std::vector<std::wstring> SteamBibliotheken() {
    std::vector<std::wstring> wurzeln;
#ifdef _WIN32
    for (const std::wstring& w : { Registry(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath"),
                                   Registry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath"),
                                   Registry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath"),
                                   std::wstring(L"C:\\Program Files (x86)\\Steam") })
        if (!w.empty()) wurzeln.push_back(w);
#endif
    std::vector<std::wstring> aus;
    auto neu = [&aus](std::wstring p) {
        std::replace(p.begin(), p.end(), L'/', L'\\');
        while (!p.empty() && p.back() == L'\\') p.pop_back();
        for (const std::wstring& a : aus) if (KleinW(a) == KleinW(p)) return;
        std::error_code ec;
        if (fs::is_directory(p + L"\\steamapps\\common", ec)) aus.push_back(p);
    };
    for (const std::wstring& w : wurzeln) {
        neu(w);
        std::ifstream in(fs::path(w + L"\\steamapps\\libraryfolders.vdf"));
        std::string zeile;
        while (std::getline(in, zeile)) {
            const size_t k = zeile.find("\"path\"");
            if (k == std::string::npos) continue;
            const size_t a = zeile.find('"', k + 6);
            const size_t e = a == std::string::npos ? a : zeile.rfind('"');
            if (a == std::string::npos || e == std::string::npos || e <= a) continue;
            std::string p = zeile.substr(a + 1, e - a - 1);
            std::string ohne;
            for (size_t i = 0; i < p.size(); ++i) {           // "C:\\Steam" -> C:\Steam
                if (p[i] == '\\' && i + 1 < p.size() && p[i + 1] == '\\') ++i;
                ohne += p[i];
            }
            neu(Breit(ohne));
        }
    }
    return aus;
}

// ------------------------------------------------------------
//  Namen der Figuren
//
//  Die drei letzten Zeichen des Codes benennen die Person, die
//  erste Stelle die Fassung (1 = Teil 1, 2 = Shippuden ...).
//  Nur Codes, deren Bedeutung sicher ist - alle anderen zeigt
//  das Fenster mit ihrem Code, und nach dem laesst sich suchen.
// ------------------------------------------------------------
struct NameEintrag { const char* kurz; const char* name; };
const NameEintrag kNamen[] = {
    { "nrt", "Naruto Uzumaki" }, { "ssk", "Sasuke Uchiha" }, { "skr", "Sakura Haruno" }, { "roc", "Rock Lee" },
    { "nej", "Neji Hyuga" }, { "ten", "Tenten" }, { "ino", "Ino Yamanaka" }, { "sik", "Shikamaru Nara" },
    { "tyo", "Choji Akimichi" }, { "kib", "Kiba Inuzuka" }, { "sin", "Shino Aburame" }, { "hnt", "Hinata Hyuga" },
    { "gar", "Gaara" }, { "knk", "Kankuro" }, { "tmr", "Temari" }, { "kks", "Kakashi Hatake" },
    { "guy", "Might Guy" }, { "asm", "Asuma Sarutobi" }, { "jry", "Jiraiya" }, { "tnd", "Tsunade" },
    { "orc", "Orochimaru" }, { "kbt", "Kabuto Yakushi" }, { "itc", "Itachi Uchiha" }, { "ksm", "Kisame Hoshigaki" },
    { "cyb", "Chiyo" }, { "sai", "Sai" }, { "ymt", "Yamato" }, { "sco", "Sasori" }, { "ddr", "Deidara" },
    { "hdn", "Hidan" }, { "kzu", "Kakuzu" }, { "sgt", "Suigetsu Hozuki" }, { "jug", "Jugo" }, { "kar", "Karin" },
    { "tob", "Tobi" }, { "knn", "Konan" }, { "pea", "Pain" }, { "klb", "Killer Bee" }, { "hkg", "Hiruzen Sarutobi" },
    { "kmm", "Kimimaro" }, { "mdr", "Madara Uchiha" }, { "dnz", "Danzo Shimura" }, { "rkg", "Raikage A" },
    { "tkg", "Onoki" }, { "mkg", "Mei Terumi" }, { "zbz", "Zabuza Momochi" }, { "hak", "Haku" }, { "obt", "Obito Uchiha" },
    { "fuu", "Fu" }, { "ygr", "Yagura" }, { "utk", "Utakata" }, { "mfn", "Mifune" }, { "han", "Han" }, { "hnz", "Hanzo" },
    { "ngt", "Nagato" }, { "mnt", "Minato Namikaze" }, { "hsr", "Hashirama Senju" }, { "tbr", "Tobirama Senju" },
    { "brt", "Boruto Uzumaki" }, { "srd", "Sarada Uchiha" }, { "mtk", "Mitsuki" }, { "kgy", "Kaguya Otsutsuki" },
};

std::string NameFuer(const std::string& code) {
    if (code.size() == 4) {
        const std::string kurz = code.substr(1);
        for (const NameEintrag& n : kNamen)
            if (kurz == n.kurz) return n.name;
    }
    return code;
}

std::string ZusatzFuer(const std::string& code, int kostuem) {
    std::string z;
    if (!code.empty() && code[0] == '1') z = "Part I";
    if (kostuem > 1) z += (z.empty() ? "" : ", ") + std::string("costume ") + std::to_string(kostuem);
    return z;
}

// ------------------------------------------------------------
//  Namen aus dem Spiel selbst (Storm 4, Connections)
//
//  playerSettingParam.bin zaehlt jede waehlbare Fassung auf: einen
//  Suchcode ("9nrt00" - Figurencode und Variante) und zwei Text-IDs
//  ("c_cha_00" Grundname, "c_cha_721" Fassung). messageInfo.bin
//  haelt die Texte, aber nur unter dem CRC32 der ID - in der
//  MSB-zuerst-Variante (Polynom 04C11DB7, Start und Ende FFFFFFFF,
//  "BZIP2"), als Little Endian abgelegt.
//
//  Beide Dateien bestehen aus festen Eintraegen mit 8-Byte-Zeigern,
//  die relativ zur eigenen Position zaehlen. Der Text-Eintrag:
//    Connections  48 Byte, Zeiger bei +0x18, FFFFFFFF bei +0x24
//    Storm 4      40 Byte, Zeiger bei +0x10, FFFFFFFF bei +0x1C
//  Storm 4 speichert die Fassung nur als Zusatz ("(Kurama Link
//  Mode)"), Connections als ganzen Namen.
// ------------------------------------------------------------
uint32_t Crc32Msb(const std::string& s) {
    static uint32_t tab[256];
    static bool fertig = false;
    if (!fertig) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i << 24;
            for (int k = 0; k < 8; ++k) c = (c & 0x80000000u) ? (c << 1) ^ 0x04C11DB7u : (c << 1);
            tab[i] = c;
        }
        fertig = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (unsigned char ch : s) c = (c << 8) ^ tab[((c >> 24) ^ ch) & 0xFF];
    return c ^ 0xFFFFFFFFu;
}

// Zeichenkette hinter einem relativen 8-Byte-Zeiger, leer wenn keiner.
std::string TextHinter(const std::vector<uint8_t>& b, size_t p) {
    if (p + 8 > b.size()) return std::string();
    int64_t v = 0;
    std::memcpy(&v, &b[p], 8);
    if (v == 0) return std::string();
    const int64_t t = static_cast<int64_t>(p) + v;
    if (t <= 0 || t >= static_cast<int64_t>(b.size())) return std::string();
    const auto ende = std::find(b.begin() + t, b.end(), uint8_t(0));
    return std::string(b.begin() + t, ende);
}

uint32_t U32Le(const std::vector<uint8_t>& b, size_t p) {
    uint32_t v = 0;
    if (p + 4 <= b.size()) std::memcpy(&v, &b[p], 4);
    return v;
}

struct SpielNamen {
    std::map<std::string, std::string> name;   // Code -> "Naruto Uzumaki (Baryon Mode)"
};

SpielNamen LeseSpielNamen(const Spieldaten& d) {
    SpielNamen aus;
    std::vector<uint8_t> psp, msg;
    std::string fehler;
    bool da = false;
    for (const char* p : { "data/spc/playersettingparam.bin.xfbin", "data/spc/win64/playersettingparam.bin.xfbin" })
        if (d.Lies(p, psp, fehler)) { da = true; break; }
    if (!da) return aus;
    da = false;
    for (const char* p : { "data/message/win64/eng/messageinfo.bin.xfbin", "data/message/win32/eng/messageinfo.bin.xfbin" })
        if (d.Lies(p, msg, fehler)) { da = true; break; }
    if (!da) return aus;

    // Texte: Zeiger, 12 Byte spaeter FFFFFFFF. Der Schluessel steht
    // 0x18 (Connections) oder 0x10 (Storm 4) vor dem Zeiger; dazwischen
    // darf ein Zeiger auf den Kurznamen stehen ("Naruto"). Welcher
    // Abstand gilt, zeigt die Schrittweite: der naechste Eintrag folgt
    // nach Abstand + 0x18 Byte mit demselben Muster.
    auto eintrag = [&](size_t q) { return q + 0x10 <= msg.size() && U32Le(msg, q + 0x0C) == 0xFFFFFFFFu; };
    size_t abstand = 0, besteZahl = 0;
    for (const size_t a : { size_t(0x18), size_t(0x10) }) {
        size_t n = 0;
        for (size_t q = a; q + a + 0x28 <= msg.size(); q += 4)
            if (eintrag(q) && eintrag(q + a + 0x18) && !TextHinter(msg, q).empty()) ++n;
        if (n > besteZahl) { besteZahl = n; abstand = a; }
    }
    std::map<uint32_t, std::string> texte;
    if (abstand != 0)
        for (size_t q = abstand; q + 0x10 <= msg.size(); q += 4) {
            if (!eintrag(q)) continue;
            const std::string s = TextHinter(msg, q);
            if (!s.empty()) texte[U32Le(msg, q - abstand)] = s;
        }
    auto text = [&](const std::string& id) -> std::string {
        if (id.empty()) return std::string();
        const auto it = texte.find(Crc32Msb(id));
        return it == texte.end() ? std::string() : it->second;
    };

    // Fassungen: Suchcode, 0x10 dahinter die beiden Text-IDs.
    auto istSuchcode = [](const std::string& s) {
        if (s.size() != 6) return false;
        for (size_t i = 0; i < 4; ++i)
            if (!(std::isdigit(static_cast<unsigned char>(s[i])) || std::islower(static_cast<unsigned char>(s[i])))) return false;
        return std::isdigit(static_cast<unsigned char>(s[4])) && std::isdigit(static_cast<unsigned char>(s[5]));
    };
    std::map<std::string, std::pair<int, std::string>> best;   // Code -> (Variante, Name)
    for (size_t p = 0; p + 0x20 <= psp.size(); p += 4) {
        const std::string sc = TextHinter(psp, p);
        if (!istSuchcode(sc)) continue;
        // Die Text-IDs: 0x10 hinter dem Suchcode (Storm 4, Connections)
        // oder 0x18 (Storm 3 - dort steht davor noch eine Zahl).
        std::string i1, i2;
        for (const size_t o : { size_t(0x10), size_t(0x18) }) {
            i1 = TextHinter(psp, p + o);
            if (i1.find('_') != std::string::npos) { i2 = TextHinter(psp, p + o + 8); break; }
            i1.clear();
        }
        if (i1.empty()) continue;
        const std::string grund = text(i1), fassung = text(i2);
        std::string n;
        // Storm 4 kennt nur den Zusatz ("(Kurama Link Mode)"), Storm 3
        // kuerzt ("Naruto (WS: Rasen Shuriken)") - dann der volle
        // Grundname mit dem Klammerteil.
        const size_t klammer = fassung.find('(');
        if (fassung.empty()) n = grund;
        else if (grund.empty() || fassung.compare(0, grund.size(), grund) == 0) n = fassung;
        else if (klammer != std::string::npos) n = grund + " " + fassung.substr(klammer);
        else n = fassung;
        if (n.empty()) continue;
        const std::string code = sc.substr(0, 4);
        const int variante = (sc[4] - '0') * 10 + (sc[5] - '0');
        const auto it = best.find(code);
        if (it == best.end() || variante < it->second.first) best[code] = { variante, n };
    }
    for (const auto& kv : best) aus.name[kv.first] = kv.second.second;
    return aus;
}

// Codes aus characode.bin.xfbin, in Kader-Reihenfolge. Die Datei
// ist eine Liste aus vierstelligen, nullterminierten Codes.
std::vector<std::string> KaderReihenfolge(const Spieldaten& d) {
    std::vector<std::string> aus;
    std::vector<uint8_t> b;
    std::string fehler;
    if (!d.Lies("data/spc/characode.bin.xfbin", b, fehler)) return aus;
    for (size_t i = 1; i + 5 <= b.size(); ++i) {
        if (b[i - 1] != 0 || b[i + 4] != 0) continue;
        bool ok = true;
        for (size_t k = 0; k < 4; ++k) {
            const unsigned char c = b[i + k];
            if (!(std::isdigit(c) || std::islower(c))) { ok = false; break; }
        }
        if (ok) aus.emplace_back(reinterpret_cast<const char*>(&b[i]), 4);
    }
    return aus;
}

bool IstCode(const std::string& s) {
    if (s.size() != 4) return false;
    for (char c : s)
        if (!(std::isdigit(static_cast<unsigned char>(c)) || std::islower(static_cast<unsigned char>(c)))) return false;
    return true;
}

// Ist der Rest hinter dem Code eine eigene Animationsdatei?
//   bod<N><x>   Grundbewegung, Kombos, Support ... dieses Kostuems
//   skl, spl, awa, kiz ...  Jutsus und Spezialangriffe
// Ausgeschlossen: alles mit "_" (Finisher, Opfer-, Schadens- und
// Farbfassungen) und die Parameterdateien (.bin).
bool EigeneAnimDatei(const std::string& rest, int kostuem) {
    if (rest.empty() || rest.find('_') != std::string::npos || rest.find(".bin") != std::string::npos) return false;
    for (const char* weg : { "eff", "prm", "acc", "blood", "dmg", "col" })
        if (rest.find(weg) != std::string::npos) return false;
    if (BeginntMit(rest, "bod")) {
        size_t p = 3;
        int n = 0;
        while (p < rest.size() && std::isdigit(static_cast<unsigned char>(rest[p]))) n = n * 10 + (rest[p++] - '0');
        return n == kostuem && p < rest.size() && std::isalpha(static_cast<unsigned char>(rest[p]));
    }
    for (const char* art : { "skl", "spl", "awa", "kiz", "atk", "sup", "ougi" })
        if (BeginntMit(rest, art)) return true;
    return false;
}

} // namespace

// ============================================================
//  Spiele finden
// ============================================================
bool IstSpielOrdner(const std::wstring& ordner) {
    std::error_code ec;
    if (ordner.empty() || !fs::is_directory(ordner, ec)) return false;
    bool gefunden = false;
    Durchlaufe(ordner, 5, [&](const fs::path& p) {
        if (gefunden) return;
        const std::wstring e = KleinW(p.extension().wstring());
        if (e == L".cpk" || e == L".xfbin") gefunden = true;
    });
    return gefunden;
}

// Die Exe verraet das Spiel zuverlaessiger als der Ordner: Steam
// legt Storm 1 und 2 je nach Region unter "QWANT" und "Qwant2" ab.
static std::wstring SpielNachExe(const std::wstring& ordner) {
    static const struct { const wchar_t* exe; const wchar_t* name; } kExe[] = {
        { L"NSUNSC.exe", L"Storm Connections" }, { L"NSUNS4.exe", L"Storm 4" },
        { L"NS3FB.exe", L"Storm 3 Full Burst" }, { L"NSUNS3.exe", L"Storm 3 Full Burst" },
        { L"NSUNSR.exe", L"Storm Revolution" },  { L"NSUNS2.exe", L"Storm 2" },
        { L"NSUNS1.exe", L"Storm 1" },
    };
    std::error_code ec;
    for (const auto& e : kExe)
        if (fs::exists(fs::path(ordner) / e.exe, ec)) return e.name;
    return std::wstring();
}

std::wstring SpielName(const std::wstring& ordner) {
    const std::wstring nachExe = SpielNachExe(ordner);
    if (!nachExe.empty()) return nachExe;
    std::wstring n = fs::path(ordner).filename().wstring();
    const std::wstring k = KleinW(n);
    if (k.find(L"connections") != std::wstring::npos) return L"Storm Connections";
    if (k.find(L"storm 4") != std::wstring::npos) return L"Storm 4";
    if (k.find(L"storm 3") != std::wstring::npos) return L"Storm 3 Full Burst";
    if (k.find(L"revolution") != std::wstring::npos) return L"Storm Revolution";
    if (k.find(L"storm 2") != std::wstring::npos) return L"Storm 2";
    if (k.find(L"trilogy") != std::wstring::npos) return L"Storm Trilogy";
    // Storm 1 heisst auf Steam schlicht "NARUTO: Ultimate Ninja STORM"
    if (k.size() >= 11 && k.compare(k.size() - 11, 11, L"ninja storm") == 0) return L"Storm 1";
    return n;
}

std::vector<SpielOrt> FindeSpiele() {
    std::vector<SpielOrt> aus;
    for (const std::wstring& bib : SteamBibliotheken()) {
        std::error_code ec;
        for (fs::directory_iterator it(bib + L"\\steamapps\\common", ec), ende; !ec && it != ende; it.increment(ec)) {
            if (!it->is_directory(ec)) continue;
            const std::wstring k = KleinW(it->path().filename().wstring());
            if (k.find(L"ninja storm") == std::wstring::npos && SpielNachExe(it->path().wstring()).empty()) continue;
            if (!IstSpielOrdner(it->path().wstring())) continue;
            aus.push_back({ SpielName(it->path().wstring()), it->path().wstring() });
        }
    }
    // Neueste zuerst: Connections, 4, 3 ...
    auto platz = [](const SpielOrt& s) {
        const std::wstring k = KleinW(s.name);
        if (k.find(L"connections") != std::wstring::npos) return 0;
        if (k.find(L"storm 4") != std::wstring::npos) return 1;
        if (k.find(L"storm 3") != std::wstring::npos) return 2;
        if (k.find(L"revolution") != std::wstring::npos) return 3;
        if (k.find(L"storm 2") != std::wstring::npos) return 4;
        if (k.find(L"storm 1") != std::wstring::npos) return 6;
        return 5;
    };
    std::stable_sort(aus.begin(), aus.end(), [&](const SpielOrt& a, const SpielOrt& b) { return platz(a) < platz(b); });
    return aus;
}

// ============================================================
//  Dateibestand
// ============================================================
bool Spieldaten::Oeffne(const std::wstring& ordner, std::string& error) {
    ordner_ = ordner;
    archive_.clear();
    index_.clear();
    std::error_code ec;
    if (!fs::is_directory(ordner, ec)) { error = "folder not found"; return false; }

    std::vector<std::pair<long, fs::path>> cpks;
    std::vector<fs::path> lose;
    Durchlaufe(ordner, 6, [&](const fs::path& p) {
        const std::wstring e = KleinW(p.extension().wstring());
        if (e == L".cpk") {
            const std::string rel = RelPfad(ordner, p);
            if (!UnwichtigesArchiv(DateiName(rel))) cpks.emplace_back(Rang(rel), p);
        } else if (e == L".xfbin") {
            lose.push_back(p);
        }
    });
    std::sort(cpks.begin(), cpks.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first < b.first : a.second < b.second;
    });

    // Niedrigster Rang zuerst, jede Quelle VORN einfuegen - am Ende
    // steht so die wichtigste vorn.
    std::string letzterFehler;
    for (const auto& c : cpks) {
        auto a = std::make_unique<CpkArchive>();
        std::string f;
        if (!a->Open(c.second.wstring(), f)) { letzterFehler = Utf8(c.second.filename().wstring()) + ": " + f; continue; }
        const int nr = static_cast<int>(archive_.size());
        const auto& e = a->Entries();
        for (size_t i = 0; i < e.size(); ++i) {
            if (!EndetMit(Klein(e[i].name), ".xfbin")) continue;
            auto& liste = index_[Klein(e[i].Path())];
            Quelle q;
            q.archiv = nr;
            q.eintrag = i;
            liste.insert(liste.begin(), q);
        }
        archive_.push_back(std::move(a));
    }
    // Ausgepackte Dateien haben Vorrang - wer eine Datei auspackt
    // und aendert, will seine Fassung sehen.
    for (const fs::path& p : lose) {
        auto& liste = index_[LoserSchluessel(RelPfad(ordner, p))];
        Quelle q;
        q.datei = p.wstring();
        liste.insert(liste.begin(), q);
    }
    if (index_.empty()) {
        error = letzterFehler.empty() ? "no game archives (.cpk) or .xfbin files in this folder" : letzterFehler;
        return false;
    }
    return true;
}

bool Spieldaten::Gibt(const std::string& pfad) const { return index_.count(Klein(pfad)) > 0; }

bool Spieldaten::Lies(const std::string& pfad, std::vector<uint8_t>& out, std::string& error) const {
    const auto it = index_.find(Klein(pfad));
    if (it == index_.end()) { error = pfad + ": not in the game data"; return false; }
    for (const Quelle& q : it->second) {
        if (q.archiv >= 0) {
            const CpkArchive& a = *archive_[static_cast<size_t>(q.archiv)];
            if (a.Read(a.Entries()[q.eintrag], out, error)) return true;
        } else {
            std::ifstream in(fs::path(q.datei), std::ios::binary);
            if (in) {
                out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
                if (UnwrapPayload(out, error)) return true;
            } else {
                error = pfad + ": cannot read the file";
            }
        }
    }
    return false;
}

std::vector<std::string> Spieldaten::Pfade() const {
    std::vector<std::string> aus;
    aus.reserve(index_.size());
    for (const auto& kv : index_) aus.push_back(kv.first);
    return aus;
}

// ============================================================
//  Katalog
// ============================================================
std::string ClumpName(const FigurEintrag& f) { return f.clump.empty() ? f.code + "bod" + std::to_string(f.kostuem) : f.clump; }

std::string AnimGruppe(const FigurEintrag& f, const std::string& pfad) {
    std::string n = Klein(DateiName(pfad));
    if (EndetMit(n, ".xfbin")) n.resize(n.size() - 6);
    if (n.find("cmn") != std::string::npos && !BeginntMit(n, f.code)) return "Reactions";
    const std::string rest = BeginntMit(n, f.code) ? n.substr(f.code.size()) : n;
    if (BeginntMit(rest, "bod")) {
        const char art = rest.empty() ? 0 : rest.back();
        switch (art) {
        case 'c': return "Basic";
        case 'l': return "Combos";
        case 's': return "Support";
        case 'i': return "Intro & win";
        default:  return "Moves";
        }
    }
    if (BeginntMit(rest, "skl")) return "Jutsu";
    if (BeginntMit(rest, "spl")) return "Ultimate";
    if (BeginntMit(rest, "awa")) return "Awakening";
    return "Other";
}

std::map<std::string, std::string> SpielNamenLesen(const Spieldaten& daten) { return LeseSpielNamen(daten).name; }

Katalog BaueKatalog(const Spieldaten& daten) {
    Katalog k;
    const std::vector<std::string> pfade = daten.Pfade();

    // Nur data/spc selbst, keine Unterordner.
    std::vector<std::string> namen;   // ohne .xfbin
    for (const std::string& p : pfade) {
        if (!BeginntMit(p, "data/spc/") || p.find('/', 9) != std::string::npos || !EndetMit(p, ".xfbin")) continue;
        namen.push_back(p.substr(9, p.size() - 9 - 6));
    }
    std::sort(namen.begin(), namen.end());

    const std::vector<std::string> kader = KaderReihenfolge(daten);
    const SpielNamen spiel = LeseSpielNamen(daten);
    auto benenne = [&](FigurEintrag& f) {
        const auto it = spiel.name.find(f.code);
        if (it != spiel.name.end()) {
            f.name = it->second;
            f.zusatz = f.kostuem > 1 ? "costume " + std::to_string(f.kostuem) : std::string();
            f.art = kArtFigur;
        } else {
            f.name = NameFuer(f.code);
            f.zusatz = ZusatzFuer(f.code, f.kostuem);
            // Kennt das Spiel seinen Kader, ist alles andere kein Kaempfer.
            f.art = (spiel.name.empty() && f.code[0] >= '1' && f.code[0] <= '3') ? kArtFigur : kArtAndere;
        }
        const auto k = std::find(kader.begin(), kader.end(), f.code);
        if (k != kader.end()) f.reihenfolge = static_cast<int>(k - kader.begin());
    };
    auto gemeinsamFuer = [&](FigurEintrag& f) {
        for (const std::string& g : { std::string(1, f.code[0]) + "cmnbod1", std::string("1cmnbod1") }) {
            if (daten.Gibt("data/spc/" + g + ".xfbin")) { f.gemeinsam = "data/spc/" + g + ".xfbin"; break; }
        }
    };
    std::set<std::string> mitModell;
    for (const std::string& n : namen) {
        // <code>bod<N>
        if (n.size() < 8 || n.compare(4, 3, "bod") != 0) continue;
        const std::string code = n.substr(0, 4);
        if (!IstCode(code) || code.find("cmn") != std::string::npos) continue;
        size_t p = 7;
        int nr = 0;
        while (p < n.size() && std::isdigit(static_cast<unsigned char>(n[p]))) nr = nr * 10 + (n[p++] - '0');
        if (p == 7 || p != n.size()) continue;

        FigurEintrag f;
        f.code = code;
        f.kostuem = nr;
        f.modell = "data/spc/" + n + ".xfbin";
        for (const std::string& a : namen)
            if (BeginntMit(a, code) && EigeneAnimDatei(a.substr(4), nr)) f.anims.push_back("data/spc/" + a + ".xfbin");
        gemeinsamFuer(f);
        benenne(f);
        mitModell.insert(code);
        k.figuren.push_back(std::move(f));
    }

    // Waehlbare Fassungen ohne eigenes Modell: ihre Animationen
    // bewegen das Modell einer anderen. Narutos Kurama-Link-Modus
    // (4nrt) hat 4nrtbod1c/l/s und Jutsus, aber kein 4nrtbod1 - die
    // Animationen sprechen 3nrtbod1 an.
    for (const auto& kv : spiel.name) {
        const std::string& code = kv.first;
        if (mitModell.count(code) || code.find("cmn") != std::string::npos) continue;
        FigurEintrag f;
        f.code = code;
        for (const std::string& a : namen)
            if (BeginntMit(a, code) && EigeneAnimDatei(a.substr(4), 1)) f.anims.push_back("data/spc/" + a + ".xfbin");
        if (f.anims.empty()) continue;
        // Welches Koerpermodell sprechen die Animationen an? Das
        // haeufigste "<code>bod<N>" mit eigener Datei.
        std::map<std::string, int> zaehler;
        for (const std::string& a : f.anims) {
            std::vector<uint8_t> b;
            std::string fehler;
            XfbinFile datei;
            if (!daten.Lies(a, b, fehler) || !ReadXfbinBuffer(b.data(), b.size(), datei).ok) continue;
            for (const ChunkMap& m : datei.table.maps) {
                if (m.typeIndex >= datei.table.types.size() || m.nameIndex >= datei.table.names.size()) continue;
                if (datei.table.types[m.typeIndex] != "nuccChunkClump") continue;
                const std::string c = datei.table.names[m.nameIndex];
                if (c.size() < 8 || c.compare(4, 3, "bod") != 0 || c.find("cmn") != std::string::npos) continue;
                if (daten.Gibt("data/spc/" + c + ".xfbin")) ++zaehler[c];
            }
            if (!zaehler.empty()) break;
        }
        if (zaehler.empty()) continue;
        const auto top = std::max_element(zaehler.begin(), zaehler.end(),
                                          [](const auto& x, const auto& y) { return x.second < y.second; });
        f.clump = top->first;
        f.modell = "data/spc/" + f.clump + ".xfbin";
        gemeinsamFuer(f);
        benenne(f);
        k.figuren.push_back(std::move(f));
    }
    std::stable_sort(k.figuren.begin(), k.figuren.end(), [](const FigurEintrag& a, const FigurEintrag& b) {
        if (a.art != b.art) return a.art < b.art;
        if (a.reihenfolge != b.reihenfolge) return a.reihenfolge < b.reihenfolge;
        if (a.code != b.code) return a.code < b.code;
        return a.kostuem < b.kostuem;
    });
    return k;
}

} // namespace xfbin
