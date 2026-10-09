// ============================================================
//  XFBIN Import - das Figurenfenster (modal)
//
//  Aufbau wie beim Dragon Ball Legends und Bleach Import: oben das
//  Spiel (alle installierten Storm-Teile werden gefunden, jeder
//  andere Ordner geht ueber "Browse"), Statuszeile, Reiter, Suche,
//  selbst gezeichnete zweizeilige Liste, Detailzeile, Fusszeile.
// ============================================================
#include "xfbinimport.h"
#include "xfbinimport_res.h"
#include "xfbin_app.h"
#include "xfbin_ui.h"

#include <shlobj.h>
#include <shobjidl.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

extern HINSTANCE hInstance;

namespace xfi {

namespace {

constexpr int kReiterZahl = 3;
const wchar_t* const kReiter[kReiterZahl] = { L"Characters", L"Others", L"All" };

enum : unsigned { kL = 1, kO = 2, kR = 4, kU = 8 };
struct Anker {
    int id;
    RECT start;
    unsigned flags;
};

struct Fenster {
    HWND h = nullptr;
    xfui::Palette pal;
    HFONT fontNormal = nullptr, fontFett = nullptr;
    int zeilenHoehe = 16;
    const xfbin::Spieldaten* daten = nullptr;
    const xfbin::Katalog* katalog = nullptr;
    std::vector<xfbin::SpielOrt> spiele;
    std::wstring ordner;
    std::vector<size_t> sichtbar;
    size_t anzahl[kReiterZahl] = {};
    int reiter = 0;
    bool texturen = true;
    bool effekte = true;
    bool wc3 = false;
    bool reaktionen = false;
    bool leeren = false;
    bool importiert = false;
    std::wstring status;
    bool statusFehler = false;
    std::vector<Anker> anker;
    SIZE startClient{ 0, 0 }, startFenster{ 0, 0 };
};

Fenster* Zustand(HWND h) { return reinterpret_cast<Fenster*>(GetWindowLongPtrW(h, GWLP_USERDATA)); }

void Status(Fenster& f, const std::wstring& t, bool fehler = false) {
    f.status = t;
    f.statusFehler = fehler;
    InvalidateRect(GetDlgItem(f.h, IDC_STATUS), nullptr, FALSE);
    UpdateWindow(GetDlgItem(f.h, IDC_STATUS));
}

const wchar_t* kOrdnerTitel = L"Ninja Storm game folder (or a folder with unpacked .xfbin files)";

bool WaehleOrdner(HWND besitzer, std::wstring& ordner) {
#ifndef __IFileOpenDialog_INTERFACE_DEFINED__
    // Max 2016: das SDK stellt Windows-Header ohne IFileOpenDialog ein
    BROWSEINFOW bi = {};
    bi.hwndOwner = besitzer;
    bi.lpszTitle = kOrdnerTitel;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST id = SHBrowseForFolderW(&bi);
    if (id == nullptr) return false;
    wchar_t pfad[MAX_PATH] = {};
    const bool ok = SHGetPathFromIDListW(id, pfad) != FALSE;
    CoTaskMemFree(id);
    if (ok) ordner = pfad;
    return ok;
#else
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return false;
    DWORD opt = 0;
    dlg->GetOptions(&opt);
    dlg->SetOptions(opt | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dlg->SetTitle(kOrdnerTitel);
    bool ok = false;
    if (SUCCEEDED(dlg->Show(besitzer))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR pfad = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pfad))) {
                ordner = pfad;
                CoTaskMemFree(pfad);
                ok = true;
            }
            item->Release();
        }
    }
    dlg->Release();
    return ok;
#endif
}

// ------------------------------------------------------------
//  Masse und Anordnung (mitwachsendes Fenster)
// ------------------------------------------------------------
int EintragsHoehe(HWND h) {
    const int z = xfui::ZeilenHoehe(h);
    return 2 * z + std::max(4, z / 3) + 2;
}

void MerkeAnker(Fenster& f) {
    struct { int id; unsigned fl; } const liste[] = {
        { IDC_SPIEL_LABEL, kL | kO }, { IDC_SPIEL, kL | kO | kR }, { IDC_DURCHSUCHEN, kO | kR }, { IDC_ORDNER, kL | kO | kR },
        { IDC_STATUS, kL | kO | kR },
        { IDC_TAB0, kL | kO }, { IDC_TAB0 + 1, kL | kO }, { IDC_TAB0 + 2, kL | kO },
        { IDC_TEXTUREN, kO | kR }, { IDC_EFFEKTE, kO | kR }, { IDC_REAKTIONEN, kO | kR }, { IDC_LEEREN, kO | kR }, { IDC_WC3, kO | kR },
        { IDC_SUCHE_LABEL, kL | kO }, { IDC_SUCHE, kL | kO | kR }, { IDC_ANZAHL, kO | kR },
        { IDC_LISTE, kL | kO | kR | kU }, { IDC_DETAIL, kL | kR | kU }, { IDC_FUSS, kL | kR | kU },
        { IDC_ANIMFENSTER, kR | kU }, { IDOK, kR | kU }, { IDCANCEL, kR | kU },
    };
    RECT c{};
    GetClientRect(f.h, &c);
    f.startClient = { c.right - c.left, c.bottom - c.top };
    RECT w{};
    GetWindowRect(f.h, &w);
    f.startFenster = { w.right - w.left, w.bottom - w.top };
    for (const auto& e : liste) {
        HWND h = GetDlgItem(f.h, e.id);
        if (h == nullptr) continue;
        RECT r{};
        GetWindowRect(h, &r);
        MapWindowPoints(nullptr, f.h, reinterpret_cast<POINT*>(&r), 2);
        f.anker.push_back({ e.id, r, e.fl });
    }
}

void Ordne(Fenster& f, int cx, int cy) {
    if (f.anker.empty()) return;
    const int dx = cx - f.startClient.cx, dy = cy - f.startClient.cy;
    HDWP h = BeginDeferWindowPos(static_cast<int>(f.anker.size()));
    for (const Anker& a : f.anker) {
        RECT n = a.start;
        if (a.flags & kR) { if (a.flags & kL) n.right += dx; else { n.left += dx; n.right += dx; } }
        if (a.flags & kU) { if (a.flags & kO) n.bottom += dy; else { n.top += dy; n.bottom += dy; } }
        HWND c = GetDlgItem(f.h, a.id);
        if (h != nullptr && c != nullptr)
            h = DeferWindowPos(h, c, nullptr, n.left, n.top, n.right - n.left, n.bottom - n.top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (h != nullptr) EndDeferWindowPos(h);
    InvalidateRect(f.h, nullptr, TRUE);
}

std::wstring Anzeigename(const xfbin::FigurEintrag& fe) {
    std::string n = fe.name;
    if (!fe.zusatz.empty()) n += "  (" + fe.zusatz + ")";
    return Breit(n);
}

// "Basic · Combos · Jutsu x2 · Ultimate"
std::wstring Gruppen(const xfbin::FigurEintrag& fe) {
    std::vector<std::pair<std::string, int>> g;
    for (const std::string& a : fe.anims) {
        const std::string n = xfbin::AnimGruppe(fe, a);
        auto it = std::find_if(g.begin(), g.end(), [&](const auto& p) { return p.first == n; });
        if (it == g.end()) g.emplace_back(n, 1);
        else ++it->second;
    }
    std::string s;
    for (const auto& p : g) s += (s.empty() ? "" : "  \xC2\xB7  ") + p.first + (p.second > 1 ? " x" + std::to_string(p.second) : std::string());
    return s.empty() ? std::wstring(L"model only - no own animation files") : Breit(s);
}

// ------------------------------------------------------------
//  Zeichnen
// ------------------------------------------------------------
void ZeichneEintrag(const Fenster& f, const DRAWITEMSTRUCT& d) {
    HDC dc = d.hDC;
    const RECT r = d.rcItem;
    const bool sel = (d.itemState & ODS_SELECTED) != 0;
    HBRUSH hg = CreateSolidBrush(sel ? f.pal.auswahl : f.pal.feld);
    FillRect(dc, &r, hg);
    DeleteObject(hg);
    if (f.katalog == nullptr || d.itemID == static_cast<UINT>(-1) || d.itemID >= f.sichtbar.size()) return;
    const int rand = std::max(4, f.zeilenHoehe / 3);
    const RECT z1 = { r.left + rand, r.top + rand / 2, r.right - rand, r.top + rand / 2 + f.zeilenHoehe };
    const RECT z2 = { z1.left, z1.bottom, z1.right, z1.bottom + f.zeilenHoehe };
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ altF = SelectObject(dc, f.fontNormal);

    const xfbin::FigurEintrag& fe = f.katalog->figuren[f.sichtbar[d.itemID]];
    const std::wstring name = Anzeigename(fe);
    const std::wstring meta = Breit(fe.code + " bod" + std::to_string(fe.kostuem));
    const std::wstring unter = Gruppen(fe);

    RECT mess = z1;
    DrawTextW(dc, meta.c_str(), -1, &mess, DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
    const int mbreite = std::min(static_cast<int>(mess.right - mess.left), static_cast<int>(z1.right - z1.left) / 2);
    RECT mz = { z1.right - mbreite, z1.top, z1.right, z1.bottom };
    SetTextColor(dc, sel ? f.pal.auswahlDim : f.pal.feldDim);
    DrawTextW(dc, meta.c_str(), -1, &mz, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);

    RECT nz = z1;
    nz.right = mz.left - 2 * rand;
    SelectObject(dc, f.fontFett != nullptr ? f.fontFett : f.fontNormal);
    SetTextColor(dc, sel ? f.pal.auswahlText : f.pal.feldText);
    DrawTextW(dc, name.c_str(), -1, &nz, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

    SelectObject(dc, f.fontNormal);
    RECT uz = z2;
    SetTextColor(dc, sel ? f.pal.auswahlDim : f.pal.feldDim);
    DrawTextW(dc, unter.c_str(), -1, &uz, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, altF);
    if (!sel) xfui::Trennlinie(f.pal, dc, r, rand);
    if ((d.itemState & ODS_FOCUS) != 0 && (d.itemState & ODS_NOFOCUSRECT) == 0) {
        RECT fr = r;
        DrawFocusRect(dc, &fr);
    }
}

// ------------------------------------------------------------
//  Liste und Anzeige
// ------------------------------------------------------------
int Gewaehlt(const Fenster& f) {
    const LRESULT i = SendDlgItemMessageW(f.h, IDC_LISTE, LB_GETCURSEL, 0, 0);
    if (i == LB_ERR || static_cast<size_t>(i) >= f.sichtbar.size()) return -1;
    return static_cast<int>(f.sichtbar[static_cast<size_t>(i)]);
}

void Bedienbarkeit(Fenster& f) {
    const bool liste = f.katalog != nullptr;
    for (int k = 0; k < kReiterZahl; ++k) EnableWindow(GetDlgItem(f.h, IDC_TAB0 + k), liste);
    EnableWindow(GetDlgItem(f.h, IDC_SUCHE), liste);
    EnableWindow(GetDlgItem(f.h, IDC_LISTE), liste);
    EnableWindow(GetDlgItem(f.h, IDOK), liste && Gewaehlt(f) >= 0);
    InvalidateRect(GetDlgItem(f.h, IDOK), nullptr, FALSE);
}

void ZeigeDetail(Fenster& f) {
    const int g = Gewaehlt(f);
    std::wstring t;
    if (g >= 0) {
        const xfbin::FigurEintrag& fe = f.katalog->figuren[static_cast<size_t>(g)];
        t = Breit(fe.modell + "  \xC2\xB7  " + std::to_string(fe.anims.size()) + " own animation file(s)" +
                  (fe.gemeinsam.empty() ? std::string() : "  \xC2\xB7  shared reactions: " + xfbin::DateiName(fe.gemeinsam)));
    } else if (!f.sichtbar.empty()) {
        t = L"Double-click to import (skeleton, meshes, skin, textures and the character's own animations).";
    } else if (f.katalog != nullptr) {
        t = L"Nothing matches - try another tab or a shorter search.";
    }
    SetDlgItemTextW(f.h, IDC_DETAIL, t.c_str());
}

std::vector<std::string> Suchwoerter(HWND h, int id) {
    wchar_t puffer[512] = {};
    GetDlgItemTextW(h, id, puffer, 512);
    const std::string s = xfbin::Klein(Utf8(puffer));
    std::vector<std::string> woerter;
    std::string wort;
    for (char c : s) {
        if (c == ' ' || c == '\t') { if (!wort.empty()) { woerter.push_back(wort); wort.clear(); } }
        else wort += c;
    }
    if (!wort.empty()) woerter.push_back(wort);
    return woerter;
}

void FuelleListe(Fenster& f) {
    HWND lb = GetDlgItem(f.h, IDC_LISTE);
    const std::vector<std::string> woerter = Suchwoerter(f.h, IDC_SUCHE);
    f.sichtbar.clear();
    std::fill(std::begin(f.anzahl), std::end(f.anzahl), size_t(0));
    if (f.katalog != nullptr) {
        for (size_t i = 0; i < f.katalog->figuren.size(); ++i) {
            const xfbin::FigurEintrag& fe = f.katalog->figuren[i];
            ++f.anzahl[fe.art];
            ++f.anzahl[xfbin::kArtAlle];
            if (f.reiter != xfbin::kArtAlle && fe.art != f.reiter) continue;
            const std::string k = xfbin::Klein(fe.name + " " + fe.code + " " + fe.zusatz + " bod" + std::to_string(fe.kostuem));
            bool alle = true;
            for (const std::string& w : woerter) if (k.find(w) == std::string::npos) { alle = false; break; }
            if (alle) f.sichtbar.push_back(i);
        }
    }
    for (int k = 0; k < kReiterZahl; ++k) {
        std::wstring t = kReiter[k];
        if (f.katalog != nullptr) t += L"  " + std::to_wstring(f.anzahl[k]);
        SetDlgItemTextW(f.h, IDC_TAB0 + k, t.c_str());
        InvalidateRect(GetDlgItem(f.h, IDC_TAB0 + k), nullptr, FALSE);
    }
    SendMessageW(lb, WM_SETREDRAW, FALSE, 0);
    SendMessageW(lb, LB_SETCOUNT, static_cast<WPARAM>(f.sichtbar.size()), 0);
    SendMessageW(lb, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
    SendMessageW(lb, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(lb, nullptr, TRUE);
    std::wstring z;
    if (f.katalog != nullptr) z = std::to_wstring(f.sichtbar.size()) + L" of " + std::to_wstring(f.anzahl[f.reiter]);
    SetDlgItemTextW(f.h, IDC_ANZAHL, z.c_str());
    ZeigeDetail(f);
    Bedienbarkeit(f);
}

// Auswahlfeld "Game": gefundene Spiele, dazu der eingestellte
// Ordner, wenn er keines davon ist.
void FuelleSpiele(Fenster& f) {
    HWND c = GetDlgItem(f.h, IDC_SPIEL);
    SendMessageW(c, CB_RESETCONTENT, 0, 0);
    bool drin = false;
    for (const xfbin::SpielOrt& s : f.spiele) if (_wcsicmp(s.ordner.c_str(), f.ordner.c_str()) == 0) drin = true;
    if (!drin && !f.ordner.empty()) f.spiele.push_back({ L"Folder: " + xfbin::SpielName(f.ordner), f.ordner });
    int wahl = -1;
    for (size_t i = 0; i < f.spiele.size(); ++i) {
        SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(f.spiele[i].name.c_str()));
        if (_wcsicmp(f.spiele[i].ordner.c_str(), f.ordner.c_str()) == 0) wahl = static_cast<int>(i);
    }
    if (f.spiele.empty()) SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"(no Ninja Storm game found - use Browse)"));
    SendMessageW(c, CB_SETCURSEL, static_cast<WPARAM>(wahl >= 0 ? wahl : 0), 0);
}

void Lade(Fenster& f) {
    SetDlgItemTextW(f.h, IDC_ORDNER, f.ordner.empty() ? L"" : f.ordner.c_str());
    if (!xfbin::IstSpielOrdner(f.ordner)) {
        f.daten = nullptr;
        f.katalog = nullptr;
        Status(f, L"No Ninja Storm game found. Install Storm 3, 4 or Connections via Steam, or pick a folder with Browse.", !f.ordner.empty());
        FuelleListe(f);
        return;
    }
    Status(f, L"Reading the game archives...");
    HCURSOR alt = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    std::string fehler;
    const bool ok = Daten(f.ordner, f.daten, f.katalog, fehler);
    SetCursor(alt);
    if (!ok) {
        f.daten = nullptr;
        f.katalog = nullptr;
        Status(f, L"Could not read the game data: " + Breit(fehler), true);
    } else {
        SchreibeEinstellung(L"Spielordner", f.ordner);
        Status(f, xfbin::SpielName(f.ordner) + L": " + std::to_wstring(f.katalog->figuren.size()) + L" characters and costumes in " +
                      std::to_wstring(f.daten->ArchivZahl()) + L" archives.");
    }
    FuelleListe(f);
}

void Importiere(Fenster& f) {
    const int g = Gewaehlt(f);
    if (g < 0 || f.katalog == nullptr || f.daten == nullptr) return;
    const xfbin::FigurEintrag& fe = f.katalog->figuren[static_cast<size_t>(g)];
    ImportOptionen o;
    o.texturen = f.texturen;
    o.effekte = f.effekte;
    o.wc3 = f.wc3;
    o.reaktionen = f.reaktionen;
    o.leeren = f.leeren;
    Status(f, L"Importing " + Anzeigename(fe) + L"...");
    HCURSOR alt = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    std::wstring bericht;
    const bool ok = ImportiereFigur(*f.daten, fe, o, bericht);
    SetCursor(alt);
    if (ok) f.importiert = true;
    Status(f, (ok ? L"Imported " : L"Import failed: ") + bericht + (ok ? L"  Next: Animations..." : L""), !ok);
}

void Einrichten(Fenster& f) {
    f.pal.Baue(&ThemeFarbe);
    f.fontNormal = reinterpret_cast<HFONT>(SendMessageW(f.h, WM_GETFONT, 0, 0));
    LOGFONTW lf{};
    if (f.fontNormal != nullptr && GetObjectW(f.fontNormal, sizeof(lf), &lf) == sizeof(lf)) {
        lf.lfWeight = FW_SEMIBOLD;
        f.fontFett = CreateFontIndirectW(&lf);
    }
    f.zeilenHoehe = xfui::ZeilenHoehe(f.h);
    SendDlgItemMessageW(f.h, IDC_LISTE, LB_SETITEMHEIGHT, 0, EintragsHoehe(f.h));
    xfui::DunkleTitelleiste(f.h, f.pal.dunkel);
    if (f.pal.dunkel) {
        SetWindowTheme(GetDlgItem(f.h, IDC_LISTE), L"DarkMode_Explorer", nullptr);
        SetWindowTheme(GetDlgItem(f.h, IDC_SPIEL), L"DarkMode_CFD", nullptr);
    }
    SetWindowTextW(f.h, (std::wstring(L"Ninja Storm Importer ") + XFBINIMPORT_VERSION_STR).c_str());
    {
        const int innen = std::max(3, f.zeilenHoehe / 4);
        SendDlgItemMessageW(f.h, IDC_SUCHE, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(innen, innen));
    }
    SendDlgItemMessageW(f.h, IDC_SUCHE, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Filter by name or code, e.g.  naruto  or  2ssk"));
    SetDlgItemTextW(f.h, IDC_FUSS, (std::wstring(L"Ninja Storm Importer ") + XFBINIMPORT_VERSION_STR).c_str());
    MerkeAnker(f);
    f.reiter = std::clamp(_wtoi(LiesEinstellung(L"Reiter").c_str()), 0, kReiterZahl - 1);
    f.texturen = LiesEinstellung(L"Texturen") != L"0";
    f.effekte = LiesEinstellung(L"Effekte") != L"0";
    f.wc3 = LiesEinstellung(L"WC3") == L"1";
    f.reaktionen = LiesEinstellung(L"Reaktionen") == L"1";
    f.leeren = LiesEinstellung(L"Leeren") == L"1";
    f.spiele = xfbin::FindeSpiele();
    f.ordner = StartOrdner();
    FuelleSpiele(f);
    Lade(f);
    SetFocus(GetDlgItem(f.h, f.katalog != nullptr ? IDC_SUCHE : IDC_DURCHSUCHEN));
    {
        wchar_t s[256] = {};
        if (GetEnvironmentVariableW(L"XFBINIMPORT_TESTSUCHE", s, 256) > 0) SetDlgItemTextW(f.h, IDC_SUCHE, s);
    }
    TestFotoStarten(f.h);
}

INT_PTR Verarbeite(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_INITDIALOG) {
        Fenster* neu = reinterpret_cast<Fenster*>(lp);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(neu));
        neu->h = h;
        Einrichten(*neu);
        return FALSE;
    }
    if (msg == WM_MEASUREITEM) {
        MEASUREITEMSTRUCT* mi = reinterpret_cast<MEASUREITEMSTRUCT*>(lp);
        if (mi != nullptr && mi->CtlID == IDC_LISTE) { mi->itemHeight = static_cast<UINT>(EintragsHoehe(h)); return TRUE; }
        if (mi != nullptr && mi->CtlID == IDC_SPIEL) { mi->itemHeight = static_cast<UINT>(xfui::ZeilenHoehe(h) + 4); return TRUE; }
        return FALSE;
    }
    Fenster* f = Zustand(h);
    if (f == nullptr) return FALSE;
    switch (msg) {
    case WM_CTLCOLORDLG:
        return reinterpret_cast<INT_PTR>(f->pal.pinselGrund);
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wp);
        const int id = GetDlgCtrlID(reinterpret_cast<HWND>(lp));
        const bool leise = id == IDC_SPIEL_LABEL || id == IDC_ORDNER || id == IDC_SUCHE_LABEL || id == IDC_DETAIL || id == IDC_FUSS || id == IDC_ANZAHL;
        SetBkColor(dc, f->pal.grund);
        SetTextColor(dc, leise ? f->pal.dim : f->pal.text);
        return reinterpret_cast<INT_PTR>(f->pal.pinselGrund);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wp);
        SetBkColor(dc, f->pal.feld);
        SetTextColor(dc, f->pal.feldText);
        return reinterpret_cast<INT_PTR>(f->pal.pinselFeld);
    }
    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* d = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
        if (d == nullptr) return FALSE;
        if (d->CtlType == ODT_COMBOBOX) xfui::ZeichneAuswahlfeld(f->pal, f->fontNormal, *d);
        else if (d->CtlType == ODT_LISTBOX) ZeichneEintrag(*f, *d);
        else if (d->CtlType == ODT_STATIC) xfui::ZeichneStatus(f->pal, f->fontNormal, *d, f->status, f->statusFehler);
        else if (d->CtlType == ODT_BUTTON) {
            const int id = static_cast<int>(d->CtlID);
            const bool betont = (id >= IDC_TAB0 && id < IDC_TAB0 + kReiterZahl && id - IDC_TAB0 == f->reiter) ||
                                (id == IDC_TEXTUREN && f->texturen) || (id == IDC_EFFEKTE && f->effekte) || (id == IDC_WC3 && f->wc3) ||
                                (id == IDC_REAKTIONEN && f->reaktionen) ||
                                (id == IDC_LEEREN && f->leeren) || id == IDOK;
            xfui::ZeichneKnopf(f->pal, f->fontNormal, *d, betont);
        }
        return TRUE;
    }
    case WM_SIZE:
        Ordne(*f, LOWORD(lp), HIWORD(lp));
        return TRUE;
    case WM_TIMER:
        if (wp == kTestFotoTimer) {
            SendDlgItemMessageW(h, IDC_LISTE, LB_SETCURSEL, 0, 0);
            ZeigeDetail(*f);
            Bedienbarkeit(*f);
            UpdateWindow(h);
            TestFotoMachen(h, L"figuren");
            EndDialog(h, 0);
        }
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(h, &ps);
        for (int id : { IDC_SUCHE, IDC_LISTE }) xfui::Kante(h, dc, id, f->pal.linie);
        EndPaint(h, &ps);
        return TRUE;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = reinterpret_cast<MINMAXINFO*>(lp);
        if (mm != nullptr && f->startFenster.cx > 0) { mm->ptMinTrackSize.x = f->startFenster.cx; mm->ptMinTrackSize.y = f->startFenster.cy; }
        return TRUE;
    }
    case WM_COMMAND: {
        const int id = LOWORD(wp), code = HIWORD(wp);
        if (id >= IDC_TAB0 && id < IDC_TAB0 + kReiterZahl) {
            if (code == BN_CLICKED) {
                f->reiter = id - IDC_TAB0;
                SchreibeEinstellung(L"Reiter", std::to_wstring(f->reiter));
                FuelleListe(*f);
            }
            return TRUE;
        }
        auto umschalten = [&](bool& wert, const wchar_t* schluessel, int knopf) {
            wert = !wert;
            SchreibeEinstellung(schluessel, wert ? L"1" : L"0");
            InvalidateRect(GetDlgItem(h, knopf), nullptr, FALSE);
        };
        switch (id) {
        case IDC_SPIEL:
            if (code == CBN_SELCHANGE) {
                const LRESULT i = SendDlgItemMessageW(h, IDC_SPIEL, CB_GETCURSEL, 0, 0);
                if (i != CB_ERR && static_cast<size_t>(i) < f->spiele.size()) {
                    f->ordner = f->spiele[static_cast<size_t>(i)].ordner;
                    Lade(*f);
                }
            }
            return TRUE;
        case IDC_DURCHSUCHEN:
            if (code == BN_CLICKED) {
                std::wstring o;
                if (WaehleOrdner(h, o)) {
                    f->ordner = o;
                    FuelleSpiele(*f);
                    Lade(*f);
                }
            }
            return TRUE;
        case IDC_TEXTUREN:
            if (code == BN_CLICKED) umschalten(f->texturen, L"Texturen", IDC_TEXTUREN);
            return TRUE;
        case IDC_WC3:
            if (code == BN_CLICKED) {
                umschalten(f->wc3, L"WC3", IDC_WC3);
                Status(*f, f->wc3 ? L"WC3 mode on: one mesh per material and the character faces -Y (WhiteoutDex / Warcraft III). Also used by the animation window."
                                  : L"WC3 mode off: the character is imported as in the game.");
            }
            return TRUE;
        case IDC_EFFEKTE:
            if (code == BN_CLICKED) {
                umschalten(f->effekte, L"Effekte", IDC_EFFEKTE);
                Status(*f, f->effekte ? L"Effects on: the effect models the character's jutsu and ultimates use are imported (boards, rings, smoke, clones) - visible only while an animation shows them."
                                      : L"Effects off: only the character and its own models.");
            }
            return TRUE;
        case IDC_REAKTIONEN:
            if (code == BN_CLICKED) {
                umschalten(f->reaktionen, L"Reaktionen", IDC_REAKTIONEN);
                Status(*f, f->reaktionen ? L"Shared reactions on: the hit, fall and get-up animations every character uses are added (group \"Reactions\")."
                                         : L"Shared reactions off: only the character's own animations are loaded.");
            }
            return TRUE;
        case IDC_LEEREN:
            if (code == BN_CLICKED) {
                umschalten(f->leeren, L"Leeren", IDC_LEEREN);
                Status(*f, f->leeren ? L"Clear scene on: everything in the scene is deleted before the import."
                                     : L"Clear scene off: the character is added to the scene.");
            }
            return TRUE;
        case IDC_SUCHE:
            if (code == EN_CHANGE) FuelleListe(*f);
            return TRUE;
        case IDC_LISTE:
            if (code == LBN_SELCHANGE) { ZeigeDetail(*f); Bedienbarkeit(*f); }
            else if (code == LBN_DBLCLK && IsWindowEnabled(GetDlgItem(h, IDOK))) Importiere(*f);
            return TRUE;
        case IDOK:
            // Eingabetaste im Suchfeld: erst den ersten Treffer waehlen, dann importieren.
            if (Gewaehlt(*f) < 0 && !f->sichtbar.empty()) {
                SendDlgItemMessageW(h, IDC_LISTE, LB_SETCURSEL, 0, 0);
                ZeigeDetail(*f);
                Bedienbarkeit(*f);
                SetFocus(GetDlgItem(h, IDC_LISTE));
            } else if (IsWindowEnabled(GetDlgItem(h, IDOK))) {
                Importiere(*f);
            }
            return TRUE;
        case IDC_ANIMFENSTER:
            if (code == BN_CLICKED) OeffneAnimFenster(h);
            return TRUE;
        case IDCANCEL:
            EndDialog(h, f->importiert ? 1 : 0);
            return TRUE;
        default:
            break;
        }
        break;
    }
    case WM_CLOSE:
        EndDialog(h, f->importiert ? 1 : 0);
        return TRUE;
    case WM_DESTROY:
        f->pal.Frei();
        if (f->fontFett != nullptr) { DeleteObject(f->fontFett); f->fontFett = nullptr; }
        return FALSE;
    default:
        break;
    }
    return FALSE;
}

// Keine Ausnahme darf in Windows' Nachrichtenschleife und damit in Max gelangen.
INT_PTR CALLBACK DlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    try {
        return Verarbeite(h, msg, wp, lp);
    } catch (const std::exception& x) {
        if (Fenster* f = Zustand(h)) Status(*f, L"Internal error: " + Breit(x.what()), true);
    } catch (...) {
        if (Fenster* f = Zustand(h)) Status(*f, L"Internal error.", true);
    }
    return FALSE;
}

} // namespace

void TestFotoStarten(HWND h) {
    wchar_t p[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"XFBINIMPORT_TESTFOTO", p, MAX_PATH) > 0) SetTimer(h, kTestFotoTimer, 4000, nullptr);
}

// Nur das Fenster selbst (PrintWindow), als 24-Bit-BMP.
bool TestFotoMachen(HWND h, const wchar_t* name) {
    KillTimer(h, kTestFotoTimer);
    wchar_t p[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"XFBINIMPORT_TESTFOTO", p, MAX_PATH) == 0) return false;
    RECT r{};
    GetWindowRect(h, &r);
    const int w = r.right - r.left, hh = r.bottom - r.top;
    if (w <= 0 || hh <= 0) return false;
    HDC sdc = GetDC(nullptr);
    HDC mdc = CreateCompatibleDC(sdc);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = hh;     // von unten nach oben, wie BMP
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ alt = SelectObject(mdc, bmp);
    const BOOL ok = PrintWindow(h, mdc, 2);   // PW_RENDERFULLCONTENT
    bool geschrieben = false;
    if (ok && bits != nullptr) {
        const std::wstring datei = std::wstring(p) + L"_" + name + L".bmp";
        FILE* fd = nullptr;
        if (_wfopen_s(&fd, datei.c_str(), L"wb") == 0 && fd != nullptr) {
            const DWORD bild = static_cast<DWORD>(w) * hh * 4;
            BITMAPFILEHEADER fh{};
            fh.bfType = 0x4D42;
            fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
            fh.bfSize = fh.bfOffBits + bild;
            std::fwrite(&fh, sizeof fh, 1, fd);
            std::fwrite(&bi.bmiHeader, sizeof bi.bmiHeader, 1, fd);
            std::fwrite(bits, 1, bild, fd);
            std::fclose(fd);
            geschrieben = true;
        }
    }
    SelectObject(mdc, alt);
    DeleteObject(bmp);
    DeleteDC(mdc);
    ReleaseDC(nullptr, sdc);
    return geschrieben;
}

int OeffneFenster() {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Fenster f;
    const INT_PTR r = DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_FIGUREN), GetCOREInterface()->GetMAXHWnd(), &DlgProc,
                                      reinterpret_cast<LPARAM>(&f));
    if (co == S_OK || co == S_FALSE) CoUninitialize();
    return r == -1 ? -1 : (r == 1 ? 1 : 0);
}

} // namespace xfi
