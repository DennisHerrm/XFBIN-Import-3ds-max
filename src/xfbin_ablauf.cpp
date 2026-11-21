// ============================================================
//  xfbin_ablauf.cpp - was die Fenster ausloesen
//
//  Ablage, Einstellungen und Protokoll wie beim Dragon Ball
//  Legends Import (%LOCALAPPDATA%\XfbinImport), dazu der Import
//  einer Figur aus den Spieldaten und das Setzen von Animationen.
//
//  Der Import selbst ist der bewaehrte Ablauf aus dem frueheren
//  MaxScript-Fenster, nur in C++ und mit Puffern statt Pfaden:
//
//    1. ALLE eigenen Animationsdateien lesen - sie bestimmen, wie
//       viele Exemplare eines Skeletts gebraucht werden (zwei
//       gleiche Waffen), und das Skelett wird danach gebaut.
//       Dabei bleibt nur, was das Skelett der Figur bewegt.
//    2. Modell (und eigene Zusatzmodelle wie Waffen): Bones,
//       Meshes, Skin, Texturen, Materialien.
//    3. Ebenen, Material-Editor, Kennzeichnung der Knoten.
// ============================================================
#include "xfbinimport.h"
#include "xfbin_app.h"

#include <maxscript/maxscript.h>
#include <notetrck.h>

#ifndef POINTHELP_CLASS_ID
#define POINTHELP_CLASS_ID 0x1230
#endif

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <sstream>

XfbinImportInterface* GetXfbinInterface();

namespace xfi {

std::wstring Breit(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(std::max(0, n)), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::string Utf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(std::max(0, n)), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}

// ------------------------------------------------------------
//  Ablage, Einstellungen, Protokoll
// ------------------------------------------------------------
std::wstring Ablage() {
    wchar_t puffer[MAX_PATH] = {};
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", puffer, MAX_PATH);
    std::wstring o = (n > 0 && n < MAX_PATH) ? std::wstring(puffer) : std::wstring(L".");
    o += L"\\XfbinImport";
    CreateDirectoryW(o.c_str(), nullptr);
    return o;
}

namespace {

std::map<std::wstring, std::wstring> LiesEinstellungen() {
    std::map<std::wstring, std::wstring> m;
    FILE* f = nullptr;
    if (_wfopen_s(&f, (Ablage() + L"\\einstellungen.txt").c_str(), L"rb") != 0 || f == nullptr) return m;
    std::string inhalt;
    char puffer[4096];
    size_t n;
    while ((n = std::fread(puffer, 1, sizeof puffer, f)) > 0) inhalt.append(puffer, n);
    std::fclose(f);
    size_t p = 0;
    while (p < inhalt.size()) {
        size_t e = inhalt.find('\n', p);
        if (e == std::string::npos) e = inhalt.size();
        std::string z = inhalt.substr(p, e - p);
        p = e + 1;
        while (!z.empty() && (z.back() == '\r' || z.back() == ' ')) z.pop_back();
        const size_t g = z.find('=');
        if (g == std::string::npos || z.empty() || z[0] == '#') continue;
        m[Breit(z.substr(0, g))] = Breit(z.substr(g + 1));
    }
    return m;
}

FILE* g_log = nullptr;

} // namespace

std::wstring LiesEinstellung(const std::wstring& schluessel) {
    const auto m = LiesEinstellungen();
    const auto it = m.find(schluessel);
    return it == m.end() ? std::wstring() : it->second;
}

void SchreibeEinstellung(const std::wstring& schluessel, const std::wstring& wert) {
    auto m = LiesEinstellungen();
    m[schluessel] = wert;
    FILE* f = nullptr;
    if (_wfopen_s(&f, (Ablage() + L"\\einstellungen.txt").c_str(), L"wb") != 0 || f == nullptr) return;
    std::fputs("# XFBIN Import - Einstellungen\r\n", f);
    for (const auto& kv : m) {
        const std::string z = Utf8(kv.first) + "=" + Utf8(kv.second) + "\r\n";
        std::fwrite(z.data(), 1, z.size(), f);
    }
    std::fclose(f);
}

void LogNeu(const char* titel) {
    if (g_log != nullptr) std::fclose(g_log);
    g_log = nullptr;
    if (_wfopen_s(&g_log, (Ablage() + L"\\import.log").c_str(), L"wb") != 0) g_log = nullptr;
    if (g_log != nullptr) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        std::fprintf(g_log, "XFBIN Import %ls  %04d-%02d-%02d %02d:%02d:%02d  Max %d\r\n%s\r\n", XFBINIMPORT_VERSION_STR, st.wYear,
                     st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, MAX_RELEASE, titel);
        std::fflush(g_log);
    }
}

void Log(const char* format, ...) {
    char puffer[4096];
    va_list a;
    va_start(a, format);
    std::vsnprintf(puffer, sizeof puffer, format, a);
    va_end(a);
    if (g_log != nullptr) {
        std::fputs(puffer, g_log);
        std::fputs("\r\n", g_log);
        std::fflush(g_log);
    }
    FILE* v = nullptr;
    if (_wfopen_s(&v, (Ablage() + L"\\verlauf.log").c_str(), L"ab") == 0 && v != nullptr) {
        std::fputs(puffer, v);
        std::fputs("\r\n", v);
        std::fclose(v);
    }
}

uint32_t ThemeFarbe(int welche) { return static_cast<uint32_t>(GetCustSysColor(welche)); }

// ------------------------------------------------------------
//  Spieldaten
// ------------------------------------------------------------
namespace {

std::unique_ptr<xfbin::Spieldaten> g_daten;
std::unique_ptr<xfbin::Katalog>    g_katalog;
std::wstring                       g_datenOrdner;

// Die zuletzt importierte Figur - das Animationsfenster arbeitet
// mit ihr, denn ihre Animationen sind die geladenen.
xfbin::FigurEintrag g_figur;
bool                g_figurDa = false;
std::wstring        g_figurOrdner;
std::set<std::string> g_effektClumps;   // Effekt-Clumps der Figur in der Szene
std::vector<std::string> g_figurClumps;   // die Figur und ihre Verwandlungen (WC3: auf der Stelle)

// Ist das ein Koerpermodell - "<code>bod<N>"?
bool IstKoerper(const std::string& c) {
    if (c.size() < 8 || c.compare(4, 3, "bod") != 0 || c.find("cmn") != std::string::npos) return false;
    for (size_t i = 7; i < c.size(); ++i) if (c[i] < '0' || c[i] > '9') return false;
    return true;
}

#if defined(MAX_RELEASE) && (MAX_RELEASE >= 24000)
bool FuehreSkript(const std::wstring& skript) {
    return ExecuteMAXScriptScript(skript.c_str(), MAXScript::ScriptSource::NonEmbedded, TRUE) != FALSE;
}
#else
bool FuehreSkript(const std::wstring& skript) {
    return ExecuteMAXScriptScript(const_cast<wchar_t*>(skript.c_str()), TRUE) != FALSE;
}
#endif

std::wstring SkriptText(const std::wstring& s) {
    std::wstring w;
    for (wchar_t c : s) {
        if (c == L'\\' || c == L'"') w += L'\\';
        w += c;
    }
    return w;
}

DWORD WINAPI KeinFortschritt(LPVOID) { return 0; }

std::wstring Sauber(std::wstring s) {
    for (wchar_t& c : s)
        if (c == L'\\' || c == L'/' || c == L':' || c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|') c = L'_';
    return s;
}

// Eine Datei aus den Spieldaten oeffnen.
bool Oeffne(XfbinImportInterface& x, const xfbin::Spieldaten& d, const std::string& pfad, std::wstring& fehler) {
    std::vector<uint8_t> b;
    std::string f;
    if (!d.Lies(pfad, b, f)) { fehler = Breit(f); return false; }
    if (x.OpenBuffer(b, Breit(pfad)) < 0) { fehler = Breit(pfad) + L": " + x.GetLastError(); return false; }
    return true;
}

std::string OhneEndung(const std::string& name) {
    const size_t p = name.rfind('.');
    return p == std::string::npos ? name : name.substr(0, p);
}

// Die Clump-Namen einer Spieldatei (nur der Kopf wird ausgewertet).
std::vector<std::string> ClumpsIn(const xfbin::Spieldaten& d, const std::string& pfad) {
    std::vector<std::string> aus;
    std::vector<uint8_t> b;
    std::string fe;
    xfbin::XfbinFile kopf;
    if (!d.Lies(pfad, b, fe) || !xfbin::ReadXfbinBuffer(b.data(), b.size(), kopf).ok) return aus;
    for (const xfbin::XfbinPage& pg : kopf.pages)
        for (const xfbin::XfbinChunk& ch : pg.chunks)
            if (ch.type != nullptr && ch.name != nullptr && *ch.type == "nuccChunkClump") aus.push_back(*ch.name);
    return aus;
}

// Katalogeintrag eines Codes (niedrigstes Kostuem) - fuer Clump und
// Gruppen einer anderen Fassung. nullptr, wenn der Katalog zu einem
// anderen Bestand gehoert.
const xfbin::FigurEintrag* KatalogFigur(const xfbin::Spieldaten& d, const std::string& code) {
    if (!g_katalog || g_daten.get() != &d) return nullptr;
    const xfbin::FigurEintrag* best = nullptr;
    for (const xfbin::FigurEintrag& f : g_katalog->figuren)
        if (f.code == code && (best == nullptr || f.kostuem < best->kostuem)) best = &f;
    return best;
}

// Effekt-Clumps: "2nrteff1_10", "1efc_dmy01", "2efb_frs00".
bool IstEffektName(const std::string& c) {
    return c.find("eff") != std::string::npos || (c.size() > 4 && c[1] == 'e' && c[2] == 'f' && c[4] == '_');
}

// "bod" + Ziffern: eine Modelldatei, keine Animationsdatei (bod1c).
bool IstModellRest(const std::string& rest) {
    if (rest.size() < 4 || rest.compare(0, 3, "bod") != 0) return false;
    return std::all_of(rest.begin() + 3, rest.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
}

} // namespace

bool Daten(const std::wstring& ordner, const xfbin::Spieldaten*& daten, const xfbin::Katalog*& katalog, std::string& fehler) {
    daten = nullptr;
    katalog = nullptr;
    if (ordner.empty()) { fehler = "no game folder"; return false; }
    if (!g_daten || g_datenOrdner != ordner) {
        auto d = std::make_unique<xfbin::Spieldaten>();
        if (!d->Oeffne(ordner, fehler)) return false;
        g_katalog = std::make_unique<xfbin::Katalog>(xfbin::BaueKatalog(*d));
        g_daten = std::move(d);
        g_datenOrdner = ordner;
        Log("Spieldaten: %s - %zu Archive, %zu Dateien, %zu Figuren", Utf8(ordner).c_str(), g_daten->ArchivZahl(),
            g_daten->DateiZahl(), g_katalog->figuren.size());
    }
    daten = g_daten.get();
    katalog = g_katalog.get();
    return true;
}

std::wstring StartOrdner() {
    const std::wstring gemerkt = LiesEinstellung(L"Spielordner");
    if (xfbin::IstSpielOrdner(gemerkt)) return gemerkt;
    const std::vector<xfbin::SpielOrt> spiele = xfbin::FindeSpiele();
    return spiele.empty() ? std::wstring() : spiele.front().ordner;
}

const xfbin::FigurEintrag* SzenenFigur() {
    XfbinImportInterface* x = GetXfbinInterface();
    if (!g_figurDa || x == nullptr || x->SceneBoneCount() <= 0) return nullptr;
    return &g_figur;
}

std::wstring SzenenFigurName() {
    const xfbin::FigurEintrag* f = SzenenFigur();
    if (f == nullptr) return std::wstring();
    std::string n = f->name;
    if (n != f->code) n += " (" + f->code + ")";
    if (!f->zusatz.empty()) n += ", " + f->zusatz;
    return Breit(n) + L"  -  " + xfbin::SpielName(g_figurOrdner);
}

// ============================================================
//  Import einer Figur
// ============================================================
bool ImportiereFigur(const xfbin::Spieldaten& d, const xfbin::FigurEintrag& f, const ImportOptionen& o, std::wstring& bericht) {
    XfbinImportInterface* xp = GetXfbinInterface();
    Interface* ip = GetCOREInterface();
    if (xp == nullptr || ip == nullptr) { bericht = L"plugin interface missing"; return false; }
    XfbinImportInterface& x = *xp;

    const std::string titel = "Import " + f.code + " bod" + std::to_string(f.kostuem) + " (" + f.name + ") aus " + Utf8(d.Ordner());
    LogNeu(titel.c_str());
    const DWORD t0 = GetTickCount();
    DWORD tAbschnitt = t0;
    std::string zeiten;   // je Abschnitt, fuers Protokoll
    auto zeit = [&](const char* was) {
        const DWORD jetzt = GetTickCount();
        zeiten += std::string(zeiten.empty() ? "" : ", ") + was + " " + std::to_string((jetzt - tAbschnitt) / 1000.0).substr(0, 4) + " s";
        tAbschnitt = jetzt;
    };

    x.Close();
    x.TexturIndexLeeren();
    x.SetFarbVariante(o.farbe);
    EffekteZuruecksetzen();
    if (o.leeren) {
        // Dazu die Ebenen, die dabei leer werden - sonst blieben etwa
        // die "clone"-Ebenen des vorigen Imports stehen.
        FuehreSkript(L"(delete objects; for i = LayerManager.count - 1 to 1 by -1 do (local l = LayerManager.getLayer i; local ns; l.nodes &ns;"
                     L" if ns.count == 0 and l.getNumChildren() == 0 and not l.current do LayerManager.deleteLayerByName l.name); OK)");
        x.ClearScene();
    } else {
        x.PruneScene();
    }
    x.SetBoneSize(o.knochenGroesse);
    x.SetSplitByMaterial(o.wc3);
    x.ClearAnims();
    // Filter und Ebenen-Haken gelten nur fuer diesen Import - auch bei
    // einem Abbruch. Danach ist wieder die Ebene aktuell, die es vorher
    // war (die Knoten entstehen gleich in ihrer eigenen).
    FuehreSkript(L"(global xfEbeneVorher = LayerManager.current.name; OK)");
    struct FilterZurueck {
        XfbinImportInterface& x;
        ~FilterZurueck() {
            x.SetClumpSkip({});
            x.SetSingleInstance({});
            x.SetLayerHook(nullptr);
            FuehreSkript(L"(local l = LayerManager.getLayerFromName xfEbeneVorher; if l != undefined do l.current = true; OK)");
        }
    } filterZurueck{ x };

    const std::string clump = xfbin::ClumpName(f);
    // Code des Modells - meist der der Figur, bei Fassungen ohne
    // eigenes Modell ein anderer (Kurama Link 4nrt -> 3nrtbod1).
    const std::string mc = clump.substr(0, 4);
    std::wstring fehler;
    size_t dateien = 0;

    // Ein weiterer Koerper mit dem Code der Figur ist ein
    // Doppelgaenger: 2nrtbod2 und 2nrtbod3 sind Narutos
    // Schattendoppelgaenger (eigenes, einfacheres Modell). Echte
    // Kostueme stehen in eigenen Dateien und werden auf die Figur
    // umgeleitet, Verwandlungen haben einen anderen Code (1nrv).
    auto istDoppelgaenger = [&](const std::string& c) {
        return c != clump && IstKoerper(c) && (c.compare(0, f.code.size(), f.code) == 0 || c.compare(0, mc.size(), mc) == 0);
    };
    std::set<std::string> doppelgaenger;   // bei "Original only" ausgelassen

    // Knochen-Praefix des eigenen Skeletts: "2tyo00" fuer Chojis
    // erstes Kostuem, "2tyo01" fuer das zweite. Animationen eines
    // anderen Kostuems sprechen ihr eigenes an.
    std::string zielPraefix;
    {
        std::vector<uint8_t> b;
        std::string fe, w;
        xfbin::XfbinFile datei;
        std::vector<xfbin::Clump> cl;
        if (d.Lies(f.modell, b, fe) && xfbin::ReadXfbinBuffer(b.data(), b.size(), datei).ok) xfbin::ParseClumps(datei, cl, fe, w);
        for (const xfbin::Clump& c : cl) {
            if (istDoppelgaenger(c.name)) doppelgaenger.insert(c.name);
            if (c.name != clump) continue;
            for (const xfbin::CoordNode& n : c.nodes)
                if (n.name.size() >= mc.size() + 2 && n.name.compare(0, mc.size(), mc) == 0) { zielPraefix = n.name.substr(0, mc.size() + 2); break; }
        }
    }
    auto praefixMit = [](const xfbin::AnmClumpRef& r, const std::string& code) {
        for (const std::string& b : r.boneNames)
            if (b.size() >= code.size() + 2 && b.compare(0, code.size(), code) == 0) return b.substr(0, code.size() + 2);
        return std::string();
    };
    auto praefixVon = [&](const xfbin::AnmClumpRef& r) { return praefixMit(r, mc); };

    // Koerper einer anderen Fassung derselben Figur (andere Ziffer) auf
    // die eigene umhaengen: Chojis 3tyoawa bewegt "2tyobod1", seine
    // Form-Datei auch "2tyvbod1" - gemeint sind 3tyobod1 und die Form
    // 3tyvbod1. Wie bei Kostuemen nur, wenn die Animation das Ziel
    // nicht selbst nennt.
    auto fassungUmhaengen = [&](size_t von) {
        for (const std::string& c : x.AnimClumpNames(von)) {
            if (!IstKoerper(c)) continue;
            const std::string code = c.substr(0, 4);
            if (code == f.code || code == mc) continue;
            for (const std::string& basis : { mc, f.code }) {
                if (code[0] == basis[0]) continue;
                std::string ziel;
                if (code.compare(1, 3, basis, 1, 3) == 0) {
                    ziel = clump;                                              // 2tyo -> die Figur
                } else if (code.compare(1, 2, basis, 1, 2) == 0) {
                    const std::string z = std::string(1, basis[0]) + c.substr(1);   // 2tyvbod1 -> 3tyvbod1
                    if (d.Gibt("data/spc/" + z + ".xfbin")) ziel = z;
                }
                if (ziel.empty()) continue;
                std::set<std::string> quellen;
                for (size_t i = von; i < x.Anims().size(); ++i)
                    for (const xfbin::AnmClumpRef& r : x.Anims()[i].clumps)
                        if (r.clumpName == c) quellen.insert(praefixMit(r, code));
                for (const std::string& q : quellen) {
                    std::string zp = (ziel == clump) ? zielPraefix : std::string();
                    if (zp.empty() && q.size() == 6) zp = ziel.substr(0, 4) + q.substr(4);   // 2tyv00 -> 3tyv00
                    const bool gleich = q.empty() || zp.empty();
                    x.RetargetAnims(von, c, ziel, gleich ? std::string() : q, gleich ? std::string() : zp, true, q);
                }
                break;
            }
        }
    };

    // --- 1. Animationen: nur die eigenen ---------------------
    for (const std::string& pfad : f.anims) {
        if (!Oeffne(x, d, pfad, fehler)) { Log("  %s: %s", pfad.c_str(), Utf8(fehler).c_str()); continue; }
        const size_t von = x.Anims().size();
        if (x.ParseAnimsAppend() < 0 && x.Anims().size() == von) continue;
        // Ein anderes Kostuem spricht dieselbe Figur unter seinem
        // eigenen Namen an ("2tyobod1" fuer Chojis bod2) - samt
        // seinem Knochen-Praefix ("2tyo00t0" statt "2tyo01t0").
        // Umgeleitet wird nur, wenn
        //   * es ein echtes Kostuem ist, mit eigener Modelldatei -
        //     Narutos 1nrtbod2 ist ein Doppelgaenger ohne Datei, und
        //     umgeleitet wurde er zu einer zweiten Naruto-Kopie;
        //   * die Animation die Figur nicht selbst nennt ("1nrtskl2_atk"
        //     nennt 1nrtbod1 UND 1nrtbod2).
        for (const std::string& c : x.AnimClumpNames(von)) {
            const std::string kopf = mc + "bod";
            if (c == clump || c.size() <= kopf.size() || c.compare(0, kopf.size(), kopf) != 0) continue;
            if (!std::all_of(c.begin() + static_cast<std::ptrdiff_t>(kopf.size()), c.end(),
                             [](char ch) { return ch >= '0' && ch <= '9'; })) continue;
            if (!d.Gibt("data/spc/" + c + ".xfbin")) continue;
            std::set<std::string> quellen;
            for (size_t i = von; i < x.Anims().size(); ++i)
                for (const xfbin::AnmClumpRef& r : x.Anims()[i].clumps)
                    if (r.clumpName == c) quellen.insert(praefixVon(r));
            for (const std::string& q : quellen) {
                const bool gleich = q.empty() || zielPraefix.empty() || q == zielPraefix;
                x.RetargetAnims(von, c, clump, gleich ? std::string() : q, gleich ? std::string() : zielPraefix, true, q);
            }
        }
        fassungUmhaengen(von);
        const size_t behalten = x.KeepAnimsFor(von, clump, true);
        x.TagAnims(von, xfbin::AnimGruppe(f, pfad));
        ++dateien;
        Log("  %s: %zu eigene Animationen", pfad.c_str(), behalten);
    }

    // --- 1b. Bewegungen anderer Fassungen und der Form --------
    // Die prm-Datei nennt, welche Bewegungen die Figur benutzt - auch
    // die einer frueheren Fassung (Choji 3tyo bringt nur 3tyoawa mit
    // und nutzt 161 Bewegungen von 2tyo) und die ihrer Form im
    // Erwachen (Narutos Fuchsmantel: 2nrvawa.xfbin). Die einer
    // frueheren Fassung bewegen deren Skelett und werden auf die Figur
    // umgehaengt (2tyobod1, 2tyo00 -> 3tyobod1, 3tyo00); die der Form
    // bewegen die Form.
    std::vector<std::string> animDateien = f.anims;   // alle gelesenen - fuer die Effekte
    std::vector<std::string> fassungen;               // Codes frueherer Fassungen ("2tyo")
    std::set<std::string> prmFormen;                  // Formen mit eigenen Bewegungen ("2nrv")
    for (const xfbin::Bewegungsquelle& q : xfbin::PrmBewegungen(d, f)) {
        xfbin::FigurEintrag qf;
        qf.code = q.code;
        if (const xfbin::FigurEintrag* k = KatalogFigur(d, q.code)) qf = *k;
        if (!q.form) fassungen.push_back(q.code);
        else prmFormen.insert(q.code);
        size_t zahl = 0;
        for (const std::string& pfad : q.dateien) {
            if (std::find(animDateien.begin(), animDateien.end(), pfad) != animDateien.end()) continue;
            if (!Oeffne(x, d, pfad, fehler)) { Log("  %s: %s", pfad.c_str(), Utf8(fehler).c_str()); continue; }
            const size_t von = x.Anims().size();
            if (x.ParseAnimsAppend() < 0 && x.Anims().size() == von) continue;
            if (x.KeepAnimsNamed(von, q.namen) == 0) continue;
            // 4nrt-Bewegungen (Kurama Link) sprechen dessen Modell
            // 3nrtbod1 an - auch das gehoert hier zur Figur.
            if (!q.form) {
                const std::string quellClump = xfbin::ClumpName(qf);
                if (quellClump != clump && quellClump.compare(1, 3, mc, 1, 3) != 0) {
                    std::set<std::string> praefixe;
                    for (size_t i = von; i < x.Anims().size(); ++i)
                        for (const xfbin::AnmClumpRef& r : x.Anims()[i].clumps)
                            if (r.clumpName == quellClump) praefixe.insert(praefixMit(r, quellClump.substr(0, 4)));
                    for (const std::string& pq : praefixe) {
                        const bool gleich = pq.empty() || zielPraefix.empty();
                        x.RetargetAnims(von, quellClump, clump, gleich ? std::string() : pq, gleich ? std::string() : zielPraefix, true, pq);
                    }
                }
            }
            fassungUmhaengen(von);
            const size_t behalten = x.KeepAnimsFor(von, clump, true);
            x.TagAnims(von, xfbin::AnimGruppe(qf, pfad));
            if (behalten == 0) continue;
            animDateien.push_back(pfad);
            ++dateien;
            zahl += behalten;
        }
        Log("  %s %s: %zu Animationen aus %zu Dateien (prm nennt %zu Namen)", q.form ? "Form" : "Fassung", q.code.c_str(), zahl,
            q.dateien.size(), q.namen.size());
    }
    if (o.reaktionen && !f.gemeinsam.empty() && Oeffne(x, d, f.gemeinsam, fehler)) {
        const size_t von = x.Anims().size();
        x.ParseAnimsAppend();
        const std::string cmn = OhneEndung(xfbin::DateiName(f.gemeinsam));     // "1cmnbod1"
        x.RetargetAnims(von, cmn, clump, cmn.substr(0, 4), mc);                  // 1cmn00t0 x -> 1nrt00t0 x
        const size_t behalten = x.KeepAnimsFor(von, clump);
        x.TagAnims(von, "Reactions");
        Log("  %s: %zu Reaktionen uebertragen", f.gemeinsam.c_str(), behalten);
    }
    const size_t animZahl = x.Anims().size();

    // --- 1c. WC3: Doppelgaenger auf moeglichst wenige Klon-Rigs ---
    // Jede Animation legt ihre Doppelgaenger der Reihe nach auf das
    // erste freie Rig des einfachen Klon-Modells - auch den vollen
    // zweiten Naruto (2nrtbod1 #2), den nur 2nrtskl1_atk2 zeigt, und
    // 2nrtbod3. Es braucht dann nur so viele Rigs, wie eine Animation
    // gleichzeitig zeigt; die Sequenzen liegen ja hintereinander. Was
    // das einfache Modell nicht hat (Gesichtsknochen), faellt weg. Die
    // Doppelgaenger einer Form bleiben, wie sie sind.
    std::set<std::string> ungenutzteKlone;   // nach dem Zusammenlegen ohne Referenz
    if (o.wc3) {
        auto istKlonKoerper = [&](const std::string& c) {
            if (c == clump || !IstKoerper(c)) return false;
            if (istDoppelgaenger(c)) return true;
            for (const std::string& b : { f.code, mc })            // Fassung: 2nrtbod2 bei 7nrt
                if (c[0] != b[0] && c.compare(1, 3, b, 1, 3) == 0) return true;
            return false;
        };
        // Das Klon-Modell: der meistgenutzte eigene Doppelgaenger.
        std::map<std::string, int> nutzung;
        for (const xfbin::Anm& a : x.Anims())
            for (const xfbin::AnmClumpRef& r : a.clumps)
                if (istKlonKoerper(r.clumpName) && istDoppelgaenger(r.clumpName)) ++nutzung[r.clumpName];
        if (!nutzung.empty()) {
            const std::string rig = std::max_element(nutzung.begin(), nutzung.end(),
                                                     [](const auto& p, const auto& q) { return p.second < q.second; })->first;
            std::string rigPraefix;
            for (const xfbin::Anm& a : x.Anims()) {
                for (const xfbin::AnmClumpRef& r : a.clumps)
                    if (r.clumpName == rig) { rigPraefix = praefixMit(r, rig.substr(0, 4)); break; }
                if (!rigPraefix.empty()) break;
            }
            std::set<std::string> vorher;
            size_t umgehaengt = 0, hoechstens = 0;
            for (size_t i = 0; i < x.Anims().size(); ++i) {
                const xfbin::Anm& a = x.Anims()[i];
                bool originalGesehen = false;
                std::vector<size_t> klonRefs;
                // Nur Referenzen mit Modell: die acht Hilfsknochen-Referenzen
                // aus 1cmncol1fe ergaeben sonst acht Rigs.
                for (size_t k = 0; k < a.clumps.size(); ++k) {
                    const std::string& c = a.clumps[k].clumpName;
                    const bool modell = !a.clumps[k].modelNames.empty();
                    if (c == clump) {
                        if (originalGesehen && modell) klonRefs.push_back(k);   // das erste ist das Original
                        originalGesehen = true;
                    } else if (istKlonKoerper(c) && modell) {
                        klonRefs.push_back(k);
                    }
                }
                hoechstens = std::max(hoechstens, klonRefs.size());
                for (size_t k : klonRefs) {
                    const xfbin::AnmClumpRef& r = x.Anims()[i].clumps[k];
                    vorher.insert(r.clumpName);
                    if (r.clumpName == rig) continue;
                    const std::string quelle = praefixMit(r, r.clumpName.substr(0, 4));
                    const bool gleich = quelle.empty() || rigPraefix.empty();
                    x.RetargetRef(i, k, rig, gleich ? std::string() : quelle, gleich ? std::string() : rigPraefix);
                    ++umgehaengt;
                }
            }
            for (const std::string& c : vorher)
                if (c != rig && c != clump) ungenutzteKlone.insert(c);
            for (const std::string& c : doppelgaenger)   // auch die der Modelldatei, die keiner mehr nennt
                if (c != rig) ungenutzteKlone.insert(c);
            std::string weg;
            for (const std::string& c : ungenutzteKlone) weg += " " + c;
            Log("  WC3: %zu Doppelgaenger-Referenzen auf %s umgelegt, hoechstens %zu gleichzeitig - ohne Rig:%s", umgehaengt, rig.c_str(),
                hoechstens, weg.empty() ? " -" : weg.c_str());
        }
    }
    zeit("Animationen");

    // Was die Grundbewegung (Stand, Lauf, Sprung, Wurf ...) meistens mit
    // Modell zeigt, ist immer zu sehen - Samehada auf Kisames Ruecken,
    // Kankuros Puppe. Gezaehlt je Exemplar: Hakus Senbon fliegen nur in
    // wenigen Wuerfen, und von Samehada traegt Kisame eins.
    std::map<std::pair<std::string, int>, int> grundZahl;
    int grundAnims = 0;
    for (size_t i = 0; i < x.Anims().size() && i < x.AnimTags().size(); ++i) {
        if (x.AnimTags()[i] != "Basic") continue;
        ++grundAnims;
        std::map<std::string, int> lauf;
        for (const xfbin::AnmClumpRef& r : x.Anims()[i].clumps) {
            const int k = lauf[r.clumpName]++;
            if (!r.modelNames.empty()) ++grundZahl[{ r.clumpName, k }];
        }
    }
    auto immerDa = [&](const std::string& c, int inst) {
        const auto it = grundZahl.find({ c, inst });
        return it != grundZahl.end() && 2 * it->second > grundAnims;
    };

    // Partikel der eigenen Animationen: welche Effekt-Animationen,
    // Mesh- und Sprite-Partikel sie starten.
    EffektVorbereitung ev;
    if (o.effekte) EffekteVorbereiten(x, d, animDateien, ev);

    // --- 2. Modelle ------------------------------------------
    // Das Koerpermodell, dazu eigene Zusatzmodelle, die die
    // Animationen ansprechen (Waffen, Begleiter mit dem Code der
    // Figur) - fremde Modelle nicht.
    //
    // Ein anderes Koerpermodell, das eine eigene Animation anspricht:
    //   * Form - gleiche Ziffer, letzter Buchstabe anders, und entweder
    //     nennt die prm-Datei ihre Bewegungen oder der Code endet wie
    //     ueblich auf "v": Narutos Erwachen (1nrtawa) zeigt den
    //     Neunschwaenzigen-Mantel 1nrvbod1. Pains andere Pfade (2peb,
    //     2pec ...) folgen demselben Muster, sind aber keine Form.
    //     Ebenso Form: der Koerper einer frueheren Fassung (2tyo fuer 3tyo).
    //   * sonst ein Begleiter: Puppen (2scxbod2, 2kkgbod1),
    //     Beschwoerungen (3krmbod1), Pains andere Pfade, Samehada.
    // Das gemeinsame Gegnermodell 1cmnbod1 ist keins von beiden.
    auto istFormCode = [&](const std::string& c) {                                              // nrt -> nrv
        if (c.size() < 4) return false;
        for (const std::string& b : { f.code, mc })
            if (c[0] == b[0] && c.compare(1, 2, b, 1, 2) == 0 && c[3] != b[3] && (c[3] == 'v' || prmFormen.count(c) > 0)) return true;
        return false;
    };
    // Der Koerper einer anderen Fassung, der nach dem Umhaengen noch
    // NEBEN der Figur steht, ist ein Doppelgaenger: Hokage-Naruto (7nrt)
    // benutzt Narutos Schattendoppelgaenger 2nrtbod2 aus Storm 2.
    auto istFassungCode = [&](const std::string& c) {                                           // 2nrt -> 7nrt
        if (c.size() < 4) return false;
        for (const std::string& b : { f.code, mc })
            if (c[0] != b[0] && c.compare(1, 3, b, 1, 3) == 0) return true;
        return false;
    };
    std::vector<std::string> modelle = { f.modell };
    std::map<std::string, std::vector<std::string>> nurClumps;   // Datei -> nur diese Clumps bauen
    std::vector<std::string> formen;              // Codes der Verwandlungen ("1nrv")
    std::set<std::string> begleiter;              // fremde Koerper, die zur Figur gehoeren
    std::set<std::string> zubehoer;               // Teile aus Sammeldateien: Waffen, Puppenteile
    std::set<std::string> ohneDatei;              // ohne gleichnamige Datei
    g_figurClumps = { clump };
    auto einordnen = [&](const std::string& c, bool ausSammeldatei) {
        const bool eigen = c.compare(0, f.code.size(), f.code) == 0 || c.compare(0, mc.size(), mc) == 0;
        if (eigen || !IstKoerper(c)) return;
        const std::string code = c.substr(0, 4);
        if (istFormCode(code)) {
            // Ein zweiter Koerper der Form ohne eigene Datei ist ihr
            // Doppelgaenger - 2nrvbod2 in 2nrvacc1 neben 2nrvbod1.
            if (ausSammeldatei && std::any_of(g_figurClumps.begin(), g_figurClumps.end(),
                                              [&](const std::string& s) { return s != clump && s.compare(0, 4, code) == 0; })) {
                doppelgaenger.insert(c);
                return;
            }
            formen.push_back(code);
            g_figurClumps.push_back(c);
            Log("  Verwandlung: %s", c.c_str());
        } else if (istFassungCode(code)) {
            doppelgaenger.insert(c);
            Log("  Doppelgaenger aus einer anderen Fassung: %s", c.c_str());
        } else {
            begleiter.insert(c);
            Log("  Begleiter: %s", c.c_str());
        }
    };
    for (const std::string& c : x.AnimClumpNames(0)) {
        if (c == clump || c.find("cmn") != std::string::npos) continue;
        const bool eigen = c.compare(0, f.code.size(), f.code) == 0 || c.compare(0, mc.size(), mc) == 0;
        if (istDoppelgaenger(c)) {
            doppelgaenger.insert(c);
            if (o.nurOriginal) continue;
        }
        const std::string p = "data/spc/" + c + ".xfbin";
        if (!d.Gibt(p)) {
            if (!IstEffektName(c)) ohneDatei.insert(c);
            continue;
        }
        if (!eigen && !IstKoerper(c)) continue;   // fremde Teile mit eigener Datei: Sache der Effekte
        if (std::find(modelle.begin(), modelle.end(), p) != modelle.end()) continue;
        modelle.push_back(p);
        einordnen(c, false);
    }

    // Was keine gleichnamige Datei hat, steht in einer Sammeldatei der
    // Figur: Narutos Doppelgaenger 2nrtbod3 in 2nrtacc2, Hakus Senbon
    // in 1hakacc1, Samehada (2smhbod1) in 2ksmacc1, Kankuros Puppen in
    // 2knkacc1/2, Pains andere Pfade in 2peaacc1-3, Madaras Susanoo in
    // 3mdracc1. Gesucht wird in den Dateien der Figur, ihrer Formen und
    // frueheren Fassungen. Erst die Zubehoer- (acc) und Modelldateien;
    // fehlt dann noch ein Koerper, alle uebrigen Dateien - nur nach
    // Koerpern, denn sonst stehen darin Effekte, die der Effekt-
    // Durchlauf holt.
    if (!ohneDatei.empty()) {
        for (const std::string& m : modelle)
            for (const std::string& c : ClumpsIn(d, m)) ohneDatei.erase(c);
        std::vector<std::string> praefixe = { f.code, mc };
        for (const std::string& c : formen) praefixe.push_back(c);
        for (const std::string& c : fassungen) praefixe.push_back(c);
        std::vector<std::pair<std::string, bool>> dateienDerFigur;   // Pfad, Sammeldatei?
        for (const std::string& p : d.Pfade()) {
            if (p.compare(0, 9, "data/spc/") != 0 || p.find('/', 9) != std::string::npos || p.find(".bin") != std::string::npos) continue;
            const std::string n = OhneEndung(xfbin::DateiName(p));
            std::string rest;
            bool passt = false;
            for (const std::string& pr : praefixe)
                if (n.size() > pr.size() && n.compare(0, pr.size(), pr) == 0) { passt = true; rest = n.substr(pr.size()); break; }
            if (!passt || rest.find("_dmg") != std::string::npos || rest.find("_col") != std::string::npos) continue;
            if (std::find(modelle.begin(), modelle.end(), p) != modelle.end()) continue;
            dateienDerFigur.push_back({ p, rest.compare(0, 3, "acc") == 0 || IstModellRest(rest) });
        }
        for (int durchgang = 0; durchgang < 2; ++durchgang) {
            for (const auto& pd : dateienDerFigur) {
                const std::string& p = pd.first;
                const bool sammel = pd.second;
                if (ohneDatei.empty()) break;
                if (sammel != (durchgang == 0)) continue;
                if (!sammel && std::none_of(ohneDatei.begin(), ohneDatei.end(), [](const std::string& c) { return IstKoerper(c); })) break;
                std::vector<std::string> bauen;
                for (const std::string& t : ClumpsIn(d, p)) {
                    if (!ohneDatei.count(t) || (!sammel && !IstKoerper(t))) continue;
                    ohneDatei.erase(t);
                    if (!IstKoerper(t)) zubehoer.insert(t);
                    else einordnen(t, true);
                    if (o.nurOriginal && doppelgaenger.count(t)) { Log("  %s aus %s - ausgelassen (original only)", t.c_str(), p.c_str()); continue; }
                    Log("  %s aus %s", t.c_str(), p.c_str());
                    bauen.push_back(t);
                }
                if (bauen.empty()) continue;
                modelle.push_back(p);
                nurClumps[p] = bauen;
            }
        }
    }

    zeit("Suche");
    std::wstring texOrdner;
    if (o.texturen) {
        texOrdner = Ablage() + L"\\textures\\" + Sauber(xfbin::SpielName(d.Ordner())) + L"\\" + Breit(f.code);
        std::error_code ec;
        std::filesystem::create_directories(texOrdner, ec);
    }

    // "Original only": die Figur und ihre Verwandlungen je einmal,
    // die Doppelgaenger gar nicht - auch nicht aus der Modelldatei
    // selbst (2nrtbod2 steht in 2nrtbod1.xfbin).
    // WC3: Doppelgaenger, die nach dem Zusammenlegen niemand mehr nennt,
    // auch nicht bauen (2nrtbod3, wenn 2nrtbod2 das Rig ist).
    if (!ungenutzteKlone.empty() && !o.nurOriginal) x.SetClumpSkip(ungenutzteKlone);
    if (o.nurOriginal) {
        x.SetClumpSkip(doppelgaenger);
        x.SetSingleInstance(std::set<std::string>(g_figurClumps.begin(), g_figurClumps.end()));
        if (!doppelgaenger.empty()) {
            std::string liste;
            for (const std::string& c : doppelgaenger) liste += " " + c;
            Log("  Original only - Doppelgaenger ausgelassen:%s", liste.c_str());
        }
    }

    // --- Ebenen: wer ist wer ----------------------------------
    // Das Original, seine Teile (Augen, Zaehne, Waffen), Verwandlungen,
    // Begleiter und die Doppelgaenger - ein weiterer Koerper der Figur
    // (2nrtbod2) oder ein weiteres Exemplar von ihr ("#2"). Die Knoten
    // entstehen gleich in ihrer Ebene (SetLayerHook).
    const std::wstring anzeige = Breit(f.name == f.code ? f.code : f.name + " " + f.code);
    std::set<std::string> effektClumps;
    auto istForm = [&](const std::string& c) {
        return c != clump && std::find(g_figurClumps.begin(), g_figurClumps.end(), c) != g_figurClumps.end();
    };
    auto istKlon = [&](const std::string& c, int inst) {
        if (effektClumps.count(c)) return false;
        if (c == clump || istForm(c)) return inst > 0;
        return istDoppelgaenger(c) || doppelgaenger.count(c) > 0;   // auch der der Form
    };
    std::map<std::pair<std::string, int>, int> klonNr;   // in der Reihenfolge der Szene
    // "(original)" bekommt die Figur erst am Ende, wenn es Doppelgaenger gibt.
    auto ebenenName = [&](const std::string& c, int inst, bool bone) {
        if (effektClumps.count(c)) return anzeige + (bone ? L" - effect bones" : L" - effects");   // alle Effekte zusammen
        const std::wstring cl = Breit(c);
        const std::wstring nummer = inst > 0 ? L" #" + std::to_wstring(inst + 1) : std::wstring();
        std::wstring name;
        if (istKlon(c, inst)) {
            auto it = klonNr.find({ c, inst });
            if (it == klonNr.end()) it = klonNr.emplace(std::make_pair(c, inst), static_cast<int>(klonNr.size()) + 1).first;
            // "Naruto Uzumaki 2nrt clone 2 (2nrtbod2)"
            name = anzeige + L" clone " + std::to_wstring(it->second) + L" (" + cl + nummer +
                   (istForm(c) || istFormCode(c.substr(0, 4)) ? L", form" : L"") + L")";
        } else if (c == clump) {
            name = anzeige;
        } else {
            name = anzeige + L" " + cl + (istForm(c) ? L" (form)" : begleiter.count(c) ? L" (companion)" : L"") + nummer;
        }
        return name + (bone ? L" - bones" : L" - meshes");
    };
    std::wstring aktuelleEbene;
    auto setzeEbene = [&](const std::wstring& n) {
        if (!o.ebenen || n == aktuelleEbene) return;
        aktuelleEbene = n;
        const std::wstring q = L"\"" + SkriptText(n) + L"\"";
        FuehreSkript(L"(local l = LayerManager.getLayerFromName " + q + L"; if l == undefined do l = LayerManager.newLayerFromName " + q +
                     L"; if l != undefined do l.current = true; OK)");
    };
    x.SetLayerHook([&](const std::string& c, int inst, bool bone) { setzeEbene(ebenenName(c, inst, bone)); });

    int knochen = 0, objekte = 0, texturen = 0;
    std::set<std::string> gebaut;   // Clumps, die aus Modelldateien in der Szene stehen
    for (const std::string& m : modelle) {
        if (!Oeffne(x, d, m, fehler)) {
            if (m == f.modell) { bericht = L"model could not be read: " + fehler; return false; }
            continue;
        }
        const auto nur = nurClumps.find(m);
        if (nur != nurClumps.end()) x.SetClumpFilter(nur->second);
        const int b = x.BuildSkeletonN(o.knochenModus, o.massstab, 0);
        const int n = x.BuildMeshesN(o.lodWeg ? 1 : 0, o.normalen ? 1 : 0, o.skin ? 1 : 0, o.massstab, 0);
        for (const std::string& c : x.FileClumpNames()) {
            if (!x.ClumpWanted(c)) continue;
            gebaut.insert(c);
            // Weitere Teile aus Form- und Begleiterdateien - Kankuros
            // 2scxbod2.xfbin bringt Arme, Draht und Feuer mit, eine Form
            // ihre Schwaenze: wie Zubehoer, sonst stuenden sie sichtbar
            // da, waehrend ihr Koerper versteckt ist.
            if (m != f.modell && c != clump && !IstKoerper(c)) zubehoer.insert(c);
        }
        if (nur != nurClumps.end()) x.SetClumpFilter({});
        if (m == f.modell && b <= 0) { bericht = L"skeleton could not be built: " + std::wstring(x.GetLastError()); return false; }
        knochen += std::max(0, b);
        objekte += std::max(0, n);
        if (o.texturen) {
            texturen += x.ExportTextures(texOrdner.c_str());
            x.BuildMaterials(texOrdner.c_str());
        }
        const std::wstring w = x.GetWarnings();
        Log("  Modell %s: %d Bones, %d Objekte%s%s", m.c_str(), b, n, w.empty() ? "" : " - ", Utf8(w).c_str());
    }

    zeit("Modelle");
    // --- 2b. Effekte -----------------------------------------
    // Die Jutsu- und Ultimate-Animationen bewegen eigene Clumps:
    // Tafeln, Linien, Ringe, Rauch, Doppelgaenger. Sie stehen nicht
    // in gleichnamigen Dateien, sondern in Sammeldateien:
    //   data/effect/1efb_nrtspl1.xfbin   Effekte der Figur
    //   data/spc/1nrtspl1_bod1.xfbin     Doppelgaenger der Ultimate
    //   data/effect/1efcmn.xfbin         ueber 300 gemeinsame Effekte
    // Gebaut wird nur, was die eigenen Animationen ansprechen.
    int effekte = 0;
    if (o.effekte) {
        std::set<std::string> gesucht(ev.clumps.begin(), ev.clumps.end());
        for (const std::string& c : x.AnimClumpNames(0)) gesucht.insert(c);
        for (const std::string& c : { clump }) gesucht.erase(c);
        for (const std::string& m : modelle) gesucht.erase(OhneEndung(xfbin::DateiName(m)));
        for (const auto& kv : nurClumps) for (const std::string& c : kv.second) gesucht.erase(c);
        // Was die Modelldateien schon in die Szene gebracht haben
        // (2nrtbod2, 2nrteyes, die Schwaenze der Form), ist kein Effekt -
        // sonst stuende es doppelt da. Und bei "Original only" kommen
        // die Doppelgaenger auch hier nicht.
        for (const std::string& c : gebaut) gesucht.erase(c);
        if (o.nurOriginal) for (const std::string& c : doppelgaenger) gesucht.erase(c);

        // Codes, deren Dateien in Frage kommen: die Figur, ihre Formen
        // und frueheren Fassungen.
        std::vector<std::string> codes = { f.code };
        for (const std::string& c : formen) codes.push_back(c);
        for (const std::string& c : fassungen) codes.push_back(c);
        std::vector<std::string> kandidaten = ev.dateien;
        auto dazu = [&](const std::string& p) {
            if (std::find(kandidaten.begin(), kandidaten.end(), p) == kandidaten.end()) kandidaten.push_back(p);
        };
        // Effekt-Bibliotheken ohne den Code im Namen: 2efb_frs00 steht
        // in data/effect/2efb_frs.xfbin, 4efb_rsn1_00 in 4efb_rsn1.xfbin.
        // Je gesuchtem Clump zaehlt der laengste Anfang, den es als
        // Datei gibt.
        for (const std::string& g : gesucht) {
            for (size_t n = g.size(); n >= 6; --n) {
                std::string stamm = g.substr(0, n);
                while (!stamm.empty() && stamm.back() == '_') stamm.pop_back();
                if (stamm.size() < 6) break;
                if (d.Gibt("data/effect/" + stamm + ".xfbin")) { dazu("data/effect/" + stamm + ".xfbin"); break; }
            }
        }
        // Manche Ultimates tragen ihre Kulisse selbst: 9nrtspl1.xfbin
        // (Baryon) haelt neben 38 Animationen 99 Effekt-Clumps.
        for (const std::string& p : animDateien) dazu(p);
        for (const std::string& p : d.Pfade()) {
            const std::string n = xfbin::DateiName(p);
            const bool spc = p.compare(0, 9, "data/spc/") == 0 && p.find('/', 9) == std::string::npos;
            const bool eff = p.compare(0, 12, "data/effect/") == 0 && p.find('/', 12) == std::string::npos;
            std::string rest;
            bool eigenerCode = false;
            bool kurzImNamen = false;
            for (const std::string& c : codes) {
                if (!eigenerCode && n.compare(0, c.size(), c) == 0) { eigenerCode = true; rest = n.substr(c.size()); }
                if (n.find(c.substr(1)) != std::string::npos) kurzImNamen = true;   // "nrt", "nrv"
            }
            // Zubehoerdateien tragen auch Effekte (2nrteff1_10 in 2nrtacc2).
            if (spc && eigenerCode &&
                (n.find("_bod") != std::string::npos || n.find("eff") != std::string::npos || rest.compare(0, 3, "acc") == 0) &&
                n.find("_dmg") == std::string::npos && n.find("_col") == std::string::npos && n.find(".bin") == std::string::npos)
                dazu(p);
            else if (eff && (kurzImNamen || n.find("efcmn") != std::string::npos))
                dazu(p);
        }
        for (const std::string& p : kandidaten) {
            if (gesucht.empty()) break;
            std::vector<uint8_t> b;
            std::string fe;
            xfbin::XfbinFile kopf;
            if (!d.Lies(p, b, fe) || !xfbin::ReadXfbinBuffer(b.data(), b.size(), kopf).ok) continue;
            std::vector<std::string> treffer;
            for (const xfbin::XfbinPage& pg : kopf.pages)
                for (const xfbin::XfbinChunk& c : pg.chunks)
                    if (c.type != nullptr && c.name != nullptr && *c.type == "nuccChunkClump" && gesucht.count(*c.name)) treffer.push_back(*c.name);
            if (treffer.empty()) continue;
            if (x.OpenBuffer(b, Breit(p)) < 0) continue;
            // vor dem Bauen eintragen - der Ebenen-Haken legt sie dann
            // gleich in die Effekt-Ebenen
            for (const std::string& t : treffer) { gesucht.erase(t); effektClumps.insert(t); }
            x.SetClumpFilter(treffer);
            const int eb = x.BuildSkeletonN(o.knochenModus, o.massstab, 0);
            const int en = x.BuildMeshesN(o.lodWeg ? 1 : 0, o.normalen ? 1 : 0, o.skin ? 1 : 0, o.massstab, 0);
            x.SetClumpFilter({});
            if (o.texturen) {
                texturen += x.ExportTextures(texOrdner.c_str());
                x.BuildMaterials(texOrdner.c_str());
            }
            effekte += static_cast<int>(treffer.size());
            knochen += std::max(0, eb);
            objekte += std::max(0, en);
            Log("  Effekte %s: %zu Clumps, %d Bones, %d Objekte", p.c_str(), treffer.size(), eb, en);
        }
        if (!gesucht.empty()) {
            std::string rest;
            for (const std::string& g : gesucht) rest += " " + g;
            Log("  ohne Modell (Kamera, Platzhalter, Gegner):%s", rest.c_str());
        }
    }

    zeit("Effekte");
    // --- 2c. Partikel und gestartete Effekte anbinden ---------
    std::vector<ULONG> partikelKnoten;
    if (o.effekte && ev.emitter > 0) {
        std::string elog;
        setzeEbene(anzeige + L" - particles");
        partikelKnoten = EffekteAnbinden(x, d, texOrdner, o.massstab, effektClumps, elog);
        Log("  Partikel: %zu Emitter, %zu Effekt-Animationen - %s", ev.emitter, ev.animsFx, elog.c_str());
    }

    zeit("Partikel");
    // --- 2d. Texturen aus anderen Dateien --------------------
    // Effekte nehmen ihre Texturen meist aus der gemeinsamen
    // Bibliothek 1efcmn.xfbin (1efc_board01, 1efc_fire07 ...), ein
    // Doppelgaenger die Koerpertextur der Figur. Was noch fehlt,
    // wird gezielt gesucht: erst die ueblichen Verdaechtigen, dann
    // die Dateien der Figur, dann alle Effektdateien.
    if (o.texturen && !texOrdner.empty()) {
        std::set<std::string> offen = x.OffeneTexturNamen();
        if (!offen.empty()) {
            const size_t vorher = offen.size();
            std::vector<std::string> kandidaten;
            for (const std::string& p : { "data/effect/" + f.code.substr(0, 1) + "efcmn.xfbin", std::string("data/effect/1efcmn.xfbin"), f.modell })
                if (d.Gibt(p) && std::find(kandidaten.begin(), kandidaten.end(), p) == kandidaten.end()) kandidaten.push_back(p);
            std::vector<std::string> codes = { f.code, mc };
            for (const std::string& c : formen) codes.push_back(c);
            for (const std::string& c : fassungen) codes.push_back(c);
            for (const std::string& p : d.Pfade()) {
                if (p.compare(0, 9, "data/spc/") != 0 || p.find('/', 9) != std::string::npos || p.find(".bin") != std::string::npos) continue;
                if (std::any_of(codes.begin(), codes.end(), [&](const std::string& c) { return p.compare(9, c.size(), c) == 0; }))
                    kandidaten.push_back(p);
            }
            for (const std::string& p : d.Pfade())
                if (p.compare(0, 12, "data/effect/") == 0 && p.find('/', 12) == std::string::npos) kandidaten.push_back(p);
            // zuletzt die Effektdateien anderer Figuren - Hokage-Naruto
            // (7nrteff1) leiht sich eine Textur von THE LAST (6nrteff1)
            for (const std::string& p : d.Pfade()) {
                if (p.compare(0, 9, "data/spc/") != 0 || p.find('/', 9) != std::string::npos) continue;
                const std::string n = xfbin::DateiName(p);
                if (n.size() > 7 && n.compare(4, 3, "eff") == 0 && n.find(".bin") == std::string::npos) kandidaten.push_back(p);
            }
            std::set<std::string> gesehen;
            int dateienMitTreffer = 0;
            for (const std::string& p : kandidaten) {
                if (offen.empty()) break;
                if (!gesehen.insert(p).second) continue;
                std::vector<uint8_t> b;
                std::string fe;
                xfbin::XfbinFile kopf;
                if (!d.Lies(p, b, fe) || !xfbin::ReadXfbinBuffer(b.data(), b.size(), kopf).ok) continue;
                bool treffer = false;
                for (const xfbin::XfbinPage& pg : kopf.pages)
                    for (const xfbin::XfbinChunk& c : pg.chunks) {
                        if (c.type == nullptr || c.name == nullptr || *c.type != "nuccChunkTexture" || c.data.size() < 32) continue;
                        if (offen.erase(xfbin::Klein(*c.name))) treffer = true;
                    }
                if (!treffer || x.OpenBuffer(b, Breit(p)) < 0) continue;
                texturen += x.ExportTextures(texOrdner.c_str());
                ++dateienMitTreffer;
            }
            const int n = x.TexturenNachtragen();
            std::string rest;
            for (const std::string& t : offen) rest += " " + t;
            Log("  Texturen aus anderen Dateien: %zu gesucht, %d Materialien nachgetragen aus %d Dateien%s%s", vorher, n,
                dateienMitTreffer, offen.empty() ? "" : " - nicht im Spiel:", rest.c_str());
        }
    }

    zeit("Texturen");
    // --- 3. Ebenen, Kennzeichnung, Material-Editor ------------
    struct Knoten { std::string clump; int inst; bool bone; ULONG h; };
    std::vector<Knoten> knotenListe;
    {
        std::wistringstream bericht2(x.GetLayerReport());
        std::wstring zeile;
        while (std::getline(bericht2, zeile)) {
            std::wistringstream z(zeile);
            std::wstring cl, inst, art, handle;
            if (!std::getline(z, cl, L'\t') || !std::getline(z, inst, L'\t') || !std::getline(z, art, L'\t') || !std::getline(z, handle, L'\t')) continue;
            knotenListe.push_back({ Utf8(cl), _wtoi(inst.c_str()), art == L"bone", static_cast<ULONG>(_wtoi64(handle.c_str())) });
        }
    }
    // Unsichtbar, bis eine Animation sie zeigt: Effekte, Doppelgaenger
    // (sonst stuenden sie deckungsgleich ueber dem Original) und
    // Verwandlungen. Alle anderen Teile - Begleiter, Waffen, auch die
    // aus der Modelldatei - ebenso, ausser die Grundbewegung zeigt sie
    // (Samehada, Kankuros Puppe); Hakus Senbon, Madaras Susanoo oder
    // die Falten einer Ultimate (4nrtspl1_wrinkles in 9nrtbod1)
    // stuenden sonst immer da. Ohne Grundbewegung bleibt alles sichtbar.
    std::map<std::wstring, std::vector<ULONG>> ebenen;
    for (const Knoten& k : knotenListe) {
        const ULONG h = k.h;
        const bool verstecken = effektClumps.count(k.clump) > 0 || istKlon(k.clump, k.inst) || istForm(k.clump) ||
                                (k.clump != clump && grundAnims > 0 && !immerDa(k.clump, k.inst));
        if (verstecken && !k.bone)
            if (INode* n = ip->GetINodeByHandle(h)) n->SetVisibility(0, 0.0f);
        std::wstring name = ebenenName(k.clump, k.inst, k.bone);
        if (k.clump == clump && !istKlon(k.clump, k.inst) && !klonNr.empty())
            name = anzeige + L" (original)" + (k.bone ? L" - bones" : L" - meshes");
        ebenen[name].push_back(h);
        if (k.bone) {
            if (INode* n = ip->GetINodeByHandle(h)) {
                n->SetUserPropString(_T("xfbin_code"), Breit(f.code).c_str());
                n->SetUserPropInt(_T("xfbin_kostuem"), f.kostuem);
                n->SetUserPropString(_T("xfbin_spiel"), d.Ordner().c_str());
            }
        }
    }
    for (ULONG h : partikelKnoten) ebenen[anzeige + L" - particles"].push_back(h);
    zeit("Kennzeichnung");
    if (o.ebenen && !ebenen.empty()) {
        // Die Knoten sind schon in ihren Ebenen entstanden; hier kommt
        // nur nach, was anders heisst - die Ebenen des Originals bekommen
        // "(original)", wenn es Doppelgaenger gibt - oder was ohne Haken
        // entstand. Umbenennen ist eine Operation je Ebene; Knoten
        // einzeln zu verschieben kostet bei offenem Scene Explorer
        // Millisekunden je Stueck.
        std::wstring sk = L"(\n fn xfL n hs = (\n  local l = LayerManager.getLayerFromName n\n"
                          L"  if l == undefined do l = LayerManager.newLayerFromName n\n"
                          L"  if l != undefined do for h in hs do (local o = maxOps.getNodeByHandle h; if o != undefined and o.layer.name != n do l.addNode o)\n )\n"
                          L" fn xfU a b = (local l = LayerManager.getLayerFromName a; if l != undefined and (LayerManager.getLayerFromName b) == undefined do l.setname b)\n"
                          L" theHold.Suspend()\n try (\n";
        if (!klonNr.empty())
            for (const wchar_t* art : { L" - bones", L" - meshes" })
                sk += L" xfU \"" + SkriptText(anzeige + art) + L"\" \"" + SkriptText(anzeige + L" (original)" + art) + L"\"\n";
        for (const auto& kv : ebenen) {
            sk += L" xfL \"" + SkriptText(kv.first) + L"\" #(";
            for (size_t i = 0; i < kv.second.size(); ++i) sk += (i ? L"," : L"") + std::to_wstring(kv.second[i]);
            sk += L")\n";
        }
        sk += L" ) catch ()\n theHold.Resume()\n OK\n)\n";
        FuehreSkript(sk);
    }
    zeit("Ebenen");
    if (o.medit && objekte > 0) {
        FuehreSkript(L"(local aMats = #(); for o in geometry where o.material != undefined and (findItem aMats o.material) == 0 do append aMats o.material;"
                     L" for i = 1 to (amin 24 aMats.count) do setMeditMaterial i aMats[i]; OK)");
    }

    x.SetSplitByMaterial(false);

    // WC3: Blickrichtung. WhiteoutDex schreibt MDX = (-Y, X, Z); eine
    // Einheit, die in Warcraft III nach vorn (+X) schaut, schaut in Max
    // nach -Y. Genau so liegen die Storm-Figuren schon da - in der
    // Ruhepose und in allen Animationen (gemessen: Augen vor der
    // Kopfmitte in -Y). Bis 3.0.0 drehte ein Helfer "WC3 root" sie um
    // 180 Grad - und damit mit dem Ruecken nach vorn. Keine Drehung.

    // WC3: den Szenen-Check von WhiteoutDex selbst die Reste
    // erledigen lassen - eindeutige Namen (Mesh und Knochen heissen
    // im Spiel oft gleich), Wc3-Materialien statt Standard,
    // gleiche Materialien zusammengefasst. Die Diffuse-Textur
    // uebernimmt WhiteoutDex dabei als dasselbe Objekt; ihre
    // UV-Keys - die Texturanimation - bleiben also erhalten.
    if (o.wc3) {
        FuehreSkript(L"(try (if wdxSceneFixAll != undefined do (local n = wdxSceneFixAll(); format \"XFBIN Import: WhiteoutDex scene fix: % changes\\n\" n)) catch ())");
        Log("  WC3: WhiteoutDex-Szenencheck (wdxSceneFixAll) ausgefuehrt, falls installiert");
    }

    g_figur = f;
    g_figurDa = true;
    g_effektClumps = effektClumps;
    g_figurOrdner = d.Ordner();
    zeit("Material-Editor");
    ip->RedrawViews(ip->GetTime());
    zeit("Anzeige");

    const double s = (GetTickCount() - t0) / 1000.0;
    std::wstring klonText;
    if (!klonNr.empty())
        klonText = L", " + std::to_wstring(klonNr.size()) + (klonNr.size() == 1 ? L" clone" : L" clones") + L" (hidden until an animation shows them)";
    else if (o.nurOriginal && !doppelgaenger.empty())
        klonText = L", original only (" + std::to_wstring(doppelgaenger.size()) + L" clone model(s) skipped)";
    wchar_t puffer[600];
    swprintf_s(puffer, L"%ls: %d bones, %d objects, %d textures, %zu animations from %zu files, %d effects, %zu particle emitters%ls (%.1f s).",
               anzeige.c_str(), knochen, objekte, texturen, animZahl, dateien, effekte, partikelKnoten.size(), klonText.c_str(), s);
    bericht = puffer;
    Log("  Zeiten: %s", zeiten.c_str());
    Log("%s", Utf8(bericht).c_str());
    return true;
}

// ============================================================
//  Animationen
// ============================================================
namespace {

void LoescheNotizspuren(INode* w) {
    while (w != nullptr && w->NumNoteTracks() > 0) w->DeleteNoteTrack(w->GetNoteTrack(0), TRUE);
}

// Notizspur und Custom Attributes NeoDexSequenceData - dieselbe
// Definition wie WhiteoutDex und die anderen Importer (SWBF2,
// TFU2, Shinobi Striker, Dragon Ball Legends), damit der
// WC3-Export die Sequenzen findet.
bool SchreibeSequenzen(Interface* ip, const std::vector<Sequenz>& plan) {
    INode* wurzel = ip->GetRootNode();
    const TimeValue tpf = GetTicksPerFrame();
    if (wurzel != nullptr) {
        LoescheNotizspuren(wurzel);
        DefNoteTrack* nt = static_cast<DefNoteTrack*>(NewDefaultNoteTrack());
        if (nt != nullptr) {
            for (const Sequenz& sq : plan) {
                const MSTR text = Breit(sq.name).c_str();
                NoteKey* k0 = new NoteKey(sq.start * tpf, text, 0);
                NoteKey* k1 = new NoteKey(sq.ende * tpf, text, 0);
                nt->keys.Append(1, &k0);
                nt->keys.Append(1, &k1);
            }
            wurzel->AddNoteTrack(nt);
        }
    }
    std::wstring namenL, startL, endeL, nlL, rarL, spL, extL, grL;
    for (size_t i = 0; i < plan.size(); ++i) {
        const wchar_t* kk = i ? L"," : L"";
        namenL += kk + (L"\"" + SkriptText(Breit(plan[i].name)) + L"\"");
        startL += kk + std::to_wstring(plan[i].start);
        endeL += kk + std::to_wstring(plan[i].ende);
        nlL += kk + std::wstring(L"false");
        rarL += kk + std::wstring(L"0.0");
        spL += kk + std::wstring(L"0.0");
        extL += kk + std::wstring(L"\"\"");
        grL += kk + std::wstring(L"\"\"");
    }
    const std::wstring sk =
        L"(\n local ca = undefined\n try (ca = ::NeoDexSequenceCA) catch (ca = undefined)\n"
        L" if (ca == undefined) do ca = attributes \"NeoDexSequenceData\" (\n  parameters main (\n"
        L"   seqNames type:#stringTab tabSizeVariable:true\n   startFrames type:#intTab tabSizeVariable:true\n"
        L"   endFrames type:#intTab tabSizeVariable:true\n   nonLooping type:#boolTab tabSizeVariable:true\n"
        L"   rarity type:#floatTab tabSizeVariable:true\n   moveSpeed type:#floatTab tabSizeVariable:true\n"
        L"   seqExtents type:#stringTab tabSizeVariable:true\n   sharedGroup type:#stringTab tabSizeVariable:true\n  )\n )\n"
        L" for i = custAttributes.count rootNode to 1 by -1 do if (custAttributes.get rootNode i).name == \"NeoDexSequenceData\" do custAttributes.delete rootNode i\n"
        L" custAttributes.add rootNode ca\n"
        L" rootNode.seqNames = #(" + namenL + L")\n rootNode.startFrames = #(" + startL + L")\n rootNode.endFrames = #(" + endeL +
        L")\n rootNode.nonLooping = #(" + nlL + L")\n rootNode.rarity = #(" + rarL + L")\n rootNode.moveSpeed = #(" + spL +
        L")\n rootNode.seqExtents = #(" + extL + L")\n rootNode.sharedGroup = #(" + grL + L")\n OK\n)\n";
    return FuehreSkript(sk);
}

bool Bereit(XfbinImportInterface*& x, Interface*& ip, std::wstring& bericht) {
    x = GetXfbinInterface();
    ip = GetCOREInterface();
    if (x == nullptr || ip == nullptr) { bericht = L"plugin interface missing"; return false; }
    if (x->SceneBoneCount() <= 0) { bericht = L"No imported character in the scene - import one first."; return false; }
    if (x->Anims().empty()) { bericht = L"No animations loaded for this character."; return false; }
    return true;
}

} // namespace

bool WendeAnimAn(int index, const AnimOptionen& o, std::wstring& bericht) {
    XfbinImportInterface* x = nullptr;
    Interface* ip = nullptr;
    if (!Bereit(x, ip, bericht)) return false;
    if (index < 0 || index >= static_cast<int>(x->Anims().size())) { bericht = L"Select an animation first."; return false; }
    LogNeu(("Animation " + x->Anims()[static_cast<size_t>(index)].name).c_str());

    x->ClearAnimKeys();
    EffektKeysLoeschen();
    x->SetInPlace(o.wc3, g_figurClumps);
    const float laenge = x->AnimFrames(index);
    int keys = x->BuildAnimAt(index, 0.0f, 7, o.massstab);
    x->SetInPlace(false);
    if (keys <= 0) { bericht = std::wstring(L"No keys set: ") + x->GetLastError(); return false; }
    if (o.sicht || o.wc3) x->BuildVisibility(index, 0.0f, laenge);
    if (o.material) keys += x->BuildMaterialAnim(index, 0.0f, laenge);

    const int ende = std::max(1, static_cast<int>(std::lround(laenge)));
    EffektFenster fenster;
    SpawnsSetzen(*x, index, 0.0, laenge, o.massstab, fenster);
    const size_t fx = EffektSichtbarkeit(*x, { { index, 0, ende } }, fenster, g_effektClumps, o.wc3, false);
    ZeigeBereich(0, ende);
    bericht = Breit(x->Anims()[static_cast<size_t>(index)].name) + L": " + std::to_wstring(keys) + L" keys, " + std::to_wstring(ende) +
              L" frames, " + std::to_wstring(fx) + L" effect objects timed" + (o.wc3 ? L", WC3: in place." : L".");
    Log("%s", Utf8(bericht).c_str());
    return true;
}

bool WendeFolgeAn(const std::vector<int>& indizes, const AnimOptionen& o, std::vector<Sequenz>& plan, std::wstring& bericht) {
    plan.clear();
    XfbinImportInterface* x = nullptr;
    Interface* ip = nullptr;
    if (!Bereit(x, ip, bericht)) return false;
    if (indizes.empty()) { bericht = L"No animations in the list."; return false; }
    LogNeu("Alle Animationen auf die Zeitleiste");
    const DWORD t0 = GetTickCount();

    x->ClearAnimKeys();
    EffektKeysLoeschen();
    x->SetInPlace(o.wc3, g_figurClumps);
    EffektFenster fenster;

    // Bild 0 traegt die Bind-Pose, danach folgt eine Animation auf
    // die andere, getrennt durch den Abstand - das Layout, das die
    // Warcraft-III-Werkzeuge erwarten.
    int keys = x->BuildBindPoseKey(0.0f);
    double at = std::max(0, o.abstand);
    size_t uebersprungen = 0;
    bool abgebrochen = false;
    std::vector<Abschnitt> abschnitte;

    ip->ProgressStart(const_cast<wchar_t*>(L"Ninja Storm Importer - animations (Esc cancels)"), TRUE, KeinFortschritt, nullptr);
    ip->DisableSceneRedraw();
    for (size_t i = 0; i < indizes.size(); ++i) {
        const int idx = indizes[i];
        ip->ProgressUpdate(static_cast<int>(100.0 * (i + 1) / indizes.size()));
        if (ip->GetCancel()) { abgebrochen = true; ip->SetCancel(FALSE); break; }
        // Reine Kamera- oder Effektclips haben auf dem Skelett nichts verloren.
        if (x->AnimHasBones(idx) == 0) { ++uebersprungen; continue; }
        const double laenge = x->AnimFrames(idx);
        const float a = static_cast<float>(at), e = static_cast<float>(at + laenge);
        if (o.ruhe) x->BuildIdleKeys(idx, a, e);
        if (o.sicht || o.wc3) x->BuildVisibility(idx, a, e);
        if (o.material) keys += x->BuildMaterialAnim(idx, a, e);
        keys += x->BuildAnimAt(idx, a, 7, o.massstab);
        SpawnsSetzen(*x, idx, a, e, o.massstab, fenster);

        Sequenz sq;
        sq.name = x->Anims()[static_cast<size_t>(idx)].name;
        sq.start = static_cast<int>(std::lround(at));
        sq.ende = static_cast<int>(std::lround(at + laenge));
        plan.push_back(sq);
        abschnitte.push_back({ idx, sq.start, sq.ende });
        at += laenge + o.abstand;
    }
    size_t fx = 0;
    if (!abgebrochen) {
        ip->ProgressUpdate(100, FALSE, const_cast<wchar_t*>(L"Effect visibility"));
        fx = EffektSichtbarkeit(*x, abschnitte, fenster, g_effektClumps, o.wc3, true);
    }
    ip->EnableSceneRedraw();
    ip->ProgressEnd();
    x->SetInPlace(false);

    if (o.notiz && !plan.empty()) SchreibeSequenzen(ip, plan);
    const int ende = std::max(1, static_cast<int>(std::lround(at)));
    ZeigeBereich(0, ende);

    wchar_t puffer[300];
    const std::wstring wc3 = L", " + std::to_wstring(fx) + L" effect objects timed" + (o.wc3 ? L", WC3: in place" : L"");
    swprintf_s(puffer, L"%zu sequences, %d keys, %d frames%ls%ls%ls (%.1f s).", plan.size(), keys, ende,
               uebersprungen ? (L", " + std::to_wstring(uebersprungen) + L" without bones skipped").c_str() : L"",
               o.notiz ? L", note track + NeoDexSequenceData" : L"", wc3.c_str(), (GetTickCount() - t0) / 1000.0);
    bericht = std::wstring(abgebrochen ? L"Cancelled - " : L"") + puffer;
    Log("%s", Utf8(bericht).c_str());
    return !plan.empty();
}

bool LiesSequenzen(std::vector<Sequenz>& aus) {
    aus.clear();
    Interface* ip = GetCOREInterface();
    INode* w = ip ? ip->GetRootNode() : nullptr;
    if (w == nullptr) return false;
    const TimeValue tpf = std::max(1, GetTicksPerFrame());
    for (int i = 0; i < w->NumNoteTracks(); ++i) {
        DefNoteTrack* nt = static_cast<DefNoteTrack*>(w->GetNoteTrack(i));
        if (nt == nullptr) continue;
        for (int k = 0; k + 1 < nt->keys.Count(); ++k) {
            NoteKey* a = nt->keys[k];
            NoteKey* e = nt->keys[k + 1];
            if (a == nullptr || e == nullptr || a->note != e->note) continue;
            Sequenz sq;
            sq.name = Utf8(std::wstring(a->note.data()));
            sq.start = static_cast<int>(a->time / tpf);
            sq.ende = static_cast<int>(e->time / tpf);
            aus.push_back(sq);
            ++k;
        }
    }
    return !aus.empty();
}

void ZeigeBereich(int startBild, int endeBild) {
    Interface* ip = GetCOREInterface();
    if (ip == nullptr) return;
    const TimeValue tpf = GetTicksPerFrame();
    ip->SetAnimRange(Interval(startBild * tpf, std::max(startBild + 1, endeBild) * tpf));
    ip->SetTime(startBild * tpf);
}

// ------------------------------------------------------------
//  Datei -> Importieren
// ------------------------------------------------------------
int ImportiereEingang(const wchar_t* pfad) {
    std::filesystem::path p(pfad ? pfad : L"");
    // Vom Dateiordner aus nach oben bis zum Spielordner - zu
    // erkennen an "Ninja STORM" im Namen. Sonst der Dateiordner.
    std::filesystem::path ordner = p.parent_path();
    for (std::filesystem::path q = ordner; !q.empty() && q != q.root_path(); q = q.parent_path()) {
        std::wstring n = q.filename().wstring();
        for (wchar_t& c : n) c = static_cast<wchar_t>(towlower(c));
        if (n.find(L"ninja storm") != std::wstring::npos) { ordner = q; break; }
    }
    if (xfbin::IstSpielOrdner(ordner.wstring())) SchreibeEinstellung(L"Spielordner", ordner.wstring());
    return OeffneFenster() > 0 ? 1 : 0;
}

} // namespace xfi
