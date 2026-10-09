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

    x.Close();
    EffekteZuruecksetzen();
    if (o.leeren) {
        FuehreSkript(L"delete objects");
        x.ClearScene();
    } else {
        x.PruneScene();
    }
    x.SetBoneSize(o.knochenGroesse);
    x.SetSplitByMaterial(o.wc3);
    x.ClearAnims();

    const std::string clump = xfbin::ClumpName(f);
    std::wstring fehler;
    size_t dateien = 0;

    // --- 1. Animationen: nur die eigenen ---------------------
    for (const std::string& pfad : f.anims) {
        if (!Oeffne(x, d, pfad, fehler)) { Log("  %s: %s", pfad.c_str(), Utf8(fehler).c_str()); continue; }
        const size_t von = x.Anims().size();
        if (x.ParseAnimsAppend() < 0 && x.Anims().size() == von) continue;
        // Ein anderes Kostuem spricht dasselbe Skelett unter
        // seinem eigenen Namen an ("1nrtbod1" fuer bod2).
        for (const std::string& c : x.AnimClumpNames(von)) {
            const std::string kopf = f.code + "bod";
            if (c == clump || c.size() <= kopf.size() || c.compare(0, kopf.size(), kopf) != 0) continue;
            if (!std::all_of(c.begin() + static_cast<std::ptrdiff_t>(kopf.size()), c.end(),
                             [](char ch) { return ch >= '0' && ch <= '9'; })) continue;
            x.RetargetAnims(von, c, clump, std::string(), std::string());
        }
        const size_t behalten = x.KeepAnimsFor(von, clump);
        x.TagAnims(von, xfbin::AnimGruppe(f, pfad));
        ++dateien;
        Log("  %s: %zu eigene Animationen", pfad.c_str(), behalten);
    }
    if (o.reaktionen && !f.gemeinsam.empty() && Oeffne(x, d, f.gemeinsam, fehler)) {
        const size_t von = x.Anims().size();
        x.ParseAnimsAppend();
        const std::string cmn = OhneEndung(xfbin::DateiName(f.gemeinsam));     // "1cmnbod1"
        x.RetargetAnims(von, cmn, clump, cmn.substr(0, 4), f.code);              // 1cmn00t0 x -> 1nrt00t0 x
        const size_t behalten = x.KeepAnimsFor(von, clump);
        x.TagAnims(von, "Reactions");
        Log("  %s: %zu Reaktionen uebertragen", f.gemeinsam.c_str(), behalten);
    }
    const size_t animZahl = x.Anims().size();

    // Partikel der eigenen Animationen: welche Effekt-Animationen,
    // Mesh- und Sprite-Partikel sie starten.
    EffektVorbereitung ev;
    if (o.effekte) EffekteVorbereiten(x, d, f.anims, ev);

    // --- 2. Modelle ------------------------------------------
    // Das Koerpermodell, dazu eigene Zusatzmodelle, die die
    // Animationen ansprechen (Waffen, Begleiter mit dem Code der
    // Figur) - fremde Modelle nicht.
    //
    // Ausnahme Verwandlungen: spricht eine EIGENE Animation ein
    // anderes Koerpermodell an, ist das die Figur in anderer Form -
    // Narutos Erwachen (1nrtawa) zeigt den Neunschwaenzigen-Mantel
    // 1nrvbod1. Das gemeinsame Gegnermodell 1cmnbod1 ist keine.
    std::vector<std::string> modelle = { f.modell };
    std::vector<std::string> formen;              // Codes der Verwandlungen ("1nrv")
    g_figurClumps = { clump };
    for (const std::string& c : x.AnimClumpNames(0)) {
        if (c == clump) continue;
        const bool eigen = c.compare(0, f.code.size(), f.code) == 0;
        const bool form = !eigen && IstKoerper(c);
        if (!eigen && !form) continue;
        const std::string p = "data/spc/" + c + ".xfbin";
        if (!d.Gibt(p) || std::find(modelle.begin(), modelle.end(), p) != modelle.end()) continue;
        modelle.push_back(p);
        if (form) {
            formen.push_back(c.substr(0, 4));
            g_figurClumps.push_back(c);
            Log("  Verwandlung: %s", c.c_str());
        }
    }

    std::wstring texOrdner;
    if (o.texturen) {
        texOrdner = Ablage() + L"\\textures\\" + Sauber(xfbin::SpielName(d.Ordner())) + L"\\" + Breit(f.code);
        std::error_code ec;
        std::filesystem::create_directories(texOrdner, ec);
    }

    int knochen = 0, objekte = 0, texturen = 0;
    for (const std::string& m : modelle) {
        if (!Oeffne(x, d, m, fehler)) {
            if (m == f.modell) { bericht = L"model could not be read: " + fehler; return false; }
            continue;
        }
        const int b = x.BuildSkeletonN(o.knochenModus, o.massstab, 0);
        const int n = x.BuildMeshesN(o.lodWeg ? 1 : 0, o.normalen ? 1 : 0, o.skin ? 1 : 0, o.massstab, 0);
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

    // --- 2b. Effekte -----------------------------------------
    // Die Jutsu- und Ultimate-Animationen bewegen eigene Clumps:
    // Tafeln, Linien, Ringe, Rauch, Doppelgaenger. Sie stehen nicht
    // in gleichnamigen Dateien, sondern in Sammeldateien:
    //   data/effect/1efb_nrtspl1.xfbin   Effekte der Figur
    //   data/spc/1nrtspl1_bod1.xfbin     Doppelgaenger der Ultimate
    //   data/effect/1efcmn.xfbin         ueber 300 gemeinsame Effekte
    // Gebaut wird nur, was die eigenen Animationen ansprechen.
    std::set<std::string> effektClumps;
    int effekte = 0;
    if (o.effekte) {
        std::set<std::string> gesucht(ev.clumps.begin(), ev.clumps.end());
        for (const std::string& c : x.AnimClumpNames(0)) gesucht.insert(c);
        for (const std::string& c : { clump }) gesucht.erase(c);
        for (const std::string& m : modelle) gesucht.erase(OhneEndung(xfbin::DateiName(m)));

        const std::string kurz = f.code.substr(1);   // "nrt"
        std::vector<std::string> kandidaten = ev.dateien;
        for (const std::string& p : d.Pfade()) {
            if (std::find(kandidaten.begin(), kandidaten.end(), p) != kandidaten.end()) continue;
            const std::string n = xfbin::DateiName(p);
            const bool spc = p.compare(0, 9, "data/spc/") == 0 && p.find('/', 9) == std::string::npos;
            const bool eff = p.compare(0, 12, "data/effect/") == 0 && p.find('/', 12) == std::string::npos;
            bool eigenerCode = n.compare(0, f.code.size(), f.code) == 0;
            for (const std::string& fc : formen) if (n.compare(0, fc.size(), fc) == 0) eigenerCode = true;
            if (spc && eigenerCode &&
                (n.find("_bod") != std::string::npos || n.find("eff") != std::string::npos) &&
                n.find("_dmg") == std::string::npos && n.find("_col") == std::string::npos && n.find(".bin") == std::string::npos)
                kandidaten.push_back(p);
            else if (eff && (n.find(kurz) != std::string::npos || n.find("efcmn") != std::string::npos))
                kandidaten.push_back(p);
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
            x.SetClumpFilter(treffer);
            const int eb = x.BuildSkeletonN(o.knochenModus, o.massstab, 0);
            const int en = x.BuildMeshesN(o.lodWeg ? 1 : 0, o.normalen ? 1 : 0, o.skin ? 1 : 0, o.massstab, 0);
            x.SetClumpFilter({});
            if (o.texturen) {
                texturen += x.ExportTextures(texOrdner.c_str());
                x.BuildMaterials(texOrdner.c_str());
            }
            for (const std::string& t : treffer) { gesucht.erase(t); effektClumps.insert(t); }
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

    // --- 2c. Partikel und gestartete Effekte anbinden ---------
    std::vector<ULONG> partikelKnoten;
    if (o.effekte && ev.emitter > 0) {
        std::string elog;
        partikelKnoten = EffekteAnbinden(x, d, texOrdner, o.massstab, effektClumps, elog);
        Log("  Partikel: %zu Emitter, %zu Effekt-Animationen - %s", ev.emitter, ev.animsFx, elog.c_str());
    }

    // --- 3. Ebenen, Kennzeichnung, Material-Editor ------------
    const std::wstring anzeige = Breit(f.name == f.code ? f.code : f.name + " " + f.code);
    std::wistringstream bericht2(x.GetLayerReport());
    std::map<std::wstring, std::vector<ULONG>> ebenen;
    std::wstring zeile;
    while (std::getline(bericht2, zeile)) {
        std::wistringstream z(zeile);
        std::wstring cl, inst, art, handle;
        if (!std::getline(z, cl, L'\t') || !std::getline(z, inst, L'\t') || !std::getline(z, art, L'\t') || !std::getline(z, handle, L'\t')) continue;
        const ULONG h = static_cast<ULONG>(_wtoi64(handle.c_str()));
        const bool effekt = effektClumps.count(Utf8(cl)) > 0;
        std::wstring name;
        if (effekt) {
            // Alle Effekte zusammen - sonst waeren es zwei Ebenen je Effekt.
            name = anzeige + ((art == L"bone") ? L" - effect bones" : L" - effects");
            // Unsichtbar, bis eine Animation den Effekt zeigt.
            if (art != L"bone")
                if (INode* n = ip->GetINodeByHandle(h)) n->SetVisibility(0, 0.0f);
        } else {
            const bool form = cl != Breit(clump) &&
                              std::find(g_figurClumps.begin(), g_figurClumps.end(), Utf8(cl)) != g_figurClumps.end();
            name = (cl == Breit(clump) ? anzeige : anzeige + L" " + cl + (form ? L" (form)" : L""));
            if (_wtoi(inst.c_str()) > 0) name += L" #" + std::to_wstring(_wtoi(inst.c_str()) + 1);
            name += (art == L"bone") ? L" - bones" : L" - meshes";
            // Die Verwandlung erscheint nur, wenn eine Animation sie zeigt.
            if (form && art != L"bone")
                if (INode* n = ip->GetINodeByHandle(h)) n->SetVisibility(0, 0.0f);
        }
        ebenen[name].push_back(h);
        if (art == L"bone") {
            if (INode* n = ip->GetINodeByHandle(h)) {
                n->SetUserPropString(_T("xfbin_code"), Breit(f.code).c_str());
                n->SetUserPropInt(_T("xfbin_kostuem"), f.kostuem);
                n->SetUserPropString(_T("xfbin_spiel"), d.Ordner().c_str());
            }
        }
    }
    for (ULONG h : partikelKnoten) ebenen[anzeige + L" - particles"].push_back(h);
    if (o.ebenen && !ebenen.empty()) {
        std::wstring sk = L"(\n fn xfL n hs = (\n  local l = LayerManager.getLayerFromName n\n"
                          L"  if l == undefined do l = LayerManager.newLayerFromName n\n"
                          L"  if l != undefined do for h in hs do (local o = maxOps.getNodeByHandle h; if o != undefined do l.addNode o)\n )\n";
        for (const auto& kv : ebenen) {
            sk += L" xfL \"" + SkriptText(kv.first) + L"\" #(";
            for (size_t i = 0; i < kv.second.size(); ++i) sk += (i ? L"," : L"") + std::to_wstring(kv.second[i]);
            sk += L")\n";
        }
        sk += L" OK\n)\n";
        FuehreSkript(sk);
    }
    if (o.medit && objekte > 0) {
        FuehreSkript(L"(local aMats = #(); for o in geometry where o.material != undefined and (findItem aMats o.material) == 0 do append aMats o.material;"
                     L" for i = 1 to (amin 24 aMats.count) do setMeditMaterial i aMats[i]; OK)");
    }

    x.SetSplitByMaterial(false);

    // WC3: Blickrichtung. Storm-Figuren laufen in Max nach +Y;
    // WhiteoutDex schreibt MDX = (-Y, X, Z), eine Einheit, die in
    // Warcraft III nach vorn (+X) schauen soll, muss in Max nach
    // -Y blicken. Ein Helfer ueber den obersten Knochen dreht die
    // Figur um 180 Grad - die Figur selbst bleibt unangetastet,
    // und die WC3-Haltung "auf der Stelle" rechnet im Raum des
    // Elternknotens weiter.
    if (o.wc3) {
        std::wistringstream rep(x.GetLayerReport());
        std::vector<INode*> oben;
        std::wstring z;
        while (std::getline(rep, z)) {
            const size_t a = z.rfind(L'	');
            if (a == std::wstring::npos || z.find(L"	bone	") == std::wstring::npos) continue;
            INode* n = ip->GetINodeByHandle(static_cast<ULONG>(_wtoi64(z.c_str() + a + 1)));
            if (n != nullptr && n->GetParentNode() != nullptr && n->GetParentNode()->IsRootNode()) oben.push_back(n);
        }
        if (Object* p = static_cast<Object*>(ip->CreateInstance(HELPER_CLASS_ID, Class_ID(POINTHELP_CLASS_ID, 0)))) {
            INode* helfer = ip->CreateObjectNode(p);
            if (helfer != nullptr) {
                const std::wstring hn = anzeige + L" WC3 root";
                helfer->SetName(const_cast<MCHAR*>(hn.c_str()));
                for (INode* n : oben) helfer->AttachChild(n, 1);
                Matrix3 tm;
                tm.IdentityMatrix();
                tm.RotateZ(PI);
                helfer->SetNodeTM(0, tm);
                helfer->SetUserPropString(_T("xfbin_code"), Breit(f.code).c_str());
                Log("  WC3: %zu Wurzelknochen unter \"%s\" um 180 Grad gedreht", oben.size(), Utf8(hn).c_str());
            }
        }
    }

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
    ip->RedrawViews(ip->GetTime());

    const double s = (GetTickCount() - t0) / 1000.0;
    wchar_t puffer[400];
    swprintf_s(puffer, L"%ls: %d bones, %d objects, %d textures, %zu animations from %zu files, %d effects, %zu particle emitters (%.1f s).",
               anzeige.c_str(), knochen, objekte, texturen, animZahl, dateien, effekte, partikelKnoten.size(), s);
    bericht = puffer;
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

    ip->ProgressStart(const_cast<wchar_t*>(L"XFBIN Import - animations (Esc cancels)"), TRUE, KeinFortschritt, nullptr);
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
