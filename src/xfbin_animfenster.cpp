// ============================================================
//  XFBIN Import - das Animationsfenster (modal)
//
//  Aufbau wie beim Dragon Ball Legends Import: oben Figur, Gruppe
//  und Suche, dann die Liste mit Kopfzeile (virtuell, LBS_NODATA),
//  Statuszeile, Sequenzen der Zeitleiste und unten Abstand,
//  Umschalter, "All to timeline", "Load", "Close".
//
//  Die Liste zeigt die Animationen, die beim Import der Figur
//  geladen wurden - also nur ihre eigenen.
// ============================================================
#include "xfbinimport.h"
#include "xfbinimport_res.h"
#include "xfbin_app.h"
#include "xfbin_ui.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

XfbinImportInterface* GetXfbinInterface();

extern HINSTANCE hInstance;

namespace xfi {

namespace {

struct Fenster {
    HWND h = nullptr;
    xfui::Palette pal;
    HFONT fontNormal = nullptr, fontFett = nullptr;
    int zeilenHoehe = 16;
    std::vector<std::string> gruppen;   // Eintrag 0 = alle
    int zeige = 0;
    std::vector<int> sichtbar;          // Indizes in Anims()
    std::vector<Sequenz> sequenzen;
    bool notiz = true, ruhe = true, sicht = true, material = true, wc3 = false;
    std::wstring status;
    bool statusFehler = false;
};

Fenster* Zustand(HWND h) { return reinterpret_cast<Fenster*>(GetWindowLongPtrW(h, GWLP_USERDATA)); }

void Status(Fenster& f, const std::wstring& t, bool fehler = false) {
    f.status = t;
    f.statusFehler = fehler;
    InvalidateRect(GetDlgItem(f.h, IDC_A_STATUS), nullptr, FALSE);
    UpdateWindow(GetDlgItem(f.h, IDC_A_STATUS));
}

std::vector<std::string> Suchwoerter(HWND h) {
    wchar_t puffer[512] = {};
    GetDlgItemTextW(h, IDC_A_SUCHE, puffer, 512);
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

const std::string& Gruppe(int i) {
    static const std::string leer;
    const auto& t = GetXfbinInterface()->AnimTags();
    return (i >= 0 && static_cast<size_t>(i) < t.size()) ? t[static_cast<size_t>(i)] : leer;
}

// ------------------------------------------------------------
//  Liste fuellen
// ------------------------------------------------------------
void FuelleGruppen(Fenster& f) {
    f.gruppen = { std::string() };
    for (const std::string& t : GetXfbinInterface()->AnimTags())
        if (!t.empty() && t[0] != '#' && std::find(f.gruppen.begin(), f.gruppen.end(), t) == f.gruppen.end()) f.gruppen.push_back(t);
    HWND c = GetDlgItem(f.h, IDC_A_ZEIGE);
    SendMessageW(c, CB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < f.gruppen.size(); ++i) {
        const std::wstring t = i == 0 ? std::wstring(L"All") : Breit(f.gruppen[i]);
        SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.c_str()));
    }
    if (f.zeige >= static_cast<int>(f.gruppen.size())) f.zeige = 0;
    SendMessageW(c, CB_SETCURSEL, static_cast<WPARAM>(f.zeige), 0);
}

void Fuelle(Fenster& f) {
    HWND lb = GetDlgItem(f.h, IDC_A_LISTE);
    f.sichtbar.clear();
    const auto& anims = GetXfbinInterface()->Anims();
    if (f.gruppen.empty()) f.gruppen.push_back(std::string());
    const std::vector<std::string> woerter = Suchwoerter(f.h);
    const std::string& gruppe = f.gruppen[static_cast<size_t>(std::max(0, f.zeige))];
    for (size_t i = 0; i < anims.size(); ++i) {
        const std::string& g = Gruppe(static_cast<int>(i));
        if (!g.empty() && g[0] == '#') continue;   // nachgeladene Effekt-Animation
        if (!gruppe.empty() && g != gruppe) continue;
        const std::string k = xfbin::Klein(anims[i].name + " " + g);
        bool alle = true;
        for (const std::string& w : woerter) if (k.find(w) == std::string::npos) { alle = false; break; }
        if (alle) f.sichtbar.push_back(static_cast<int>(i));
    }
    SendMessageW(lb, WM_SETREDRAW, FALSE, 0);
    SendMessageW(lb, LB_SETCOUNT, static_cast<WPARAM>(f.sichtbar.size()), 0);
    SendMessageW(lb, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
    SendMessageW(lb, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(lb, nullptr, TRUE);
    InvalidateRect(GetDlgItem(f.h, IDC_A_KOPF), nullptr, FALSE);
    if (SzenenFigur() == nullptr) {
        Status(f, L"No character imported in this session - open \"Import Naruto Storm\" and import one first.", true);
    } else {
        size_t eigene = 0;
        for (size_t i = 0; i < anims.size(); ++i) { const std::string& g = Gruppe(static_cast<int>(i)); if (g.empty() || g[0] != '#') ++eigene; }
        Status(f, std::to_wstring(f.sichtbar.size()) + L" of " + std::to_wstring(eigene) +
                      L" animations. Double-click loads one; \"All to timeline\" loads the whole list as sequences.");
    }
}

void FuelleSequenzen(Fenster& f) {
    HWND c = GetDlgItem(f.h, IDC_A_SEQUENZ);
    SendMessageW(c, CB_RESETCONTENT, 0, 0);
    if (f.sequenzen.empty()) {
        SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"(no sequences - use \"All to timeline\")"));
        SendMessageW(c, CB_SETCURSEL, 0, 0);
        EnableWindow(c, FALSE);
        return;
    }
    int ende = 0;
    for (const Sequenz& s : f.sequenzen) ende = std::max(ende, s.ende);
    const std::wstring alle = L"Whole timeline  (0 - " + std::to_wstring(ende) + L")";
    SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(alle.c_str()));
    for (size_t i = 0; i < f.sequenzen.size(); ++i) {
        const Sequenz& s = f.sequenzen[i];
        const std::wstring t = std::to_wstring(i + 1) + L"   " + Breit(s.name) + L"  (" + std::to_wstring(s.start) + L" - " + std::to_wstring(s.ende) + L")";
        SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.c_str()));
    }
    SendMessageW(c, CB_SETCURSEL, 0, 0);
    EnableWindow(c, TRUE);
}

void ZeigeSequenz(Fenster& f) {
    if (f.sequenzen.empty()) return;
    const LRESULT i = SendDlgItemMessageW(f.h, IDC_A_SEQUENZ, CB_GETCURSEL, 0, 0);
    if (i == CB_ERR) return;
    if (i == 0) {
        int ende = 0;
        for (const Sequenz& s : f.sequenzen) ende = std::max(ende, s.ende);
        ZeigeBereich(0, ende);
        return;
    }
    const Sequenz& s = f.sequenzen[static_cast<size_t>(i - 1)];
    ZeigeBereich(s.start, s.ende);
    Status(f, L"Sequence " + std::to_wstring(i) + L": " + Breit(s.name) + L"  (" + std::to_wstring(s.start) + L" - " + std::to_wstring(s.ende) + L")");
}

AnimOptionen Optionen(const Fenster& f) {
    AnimOptionen o;
    BOOL ok = FALSE;
    const int abstand = static_cast<int>(GetDlgItemInt(f.h, IDC_A_ABSTAND, &ok, FALSE));
    o.abstand = (ok && abstand >= 0) ? std::min(abstand, 1000) : 10;
    o.notiz = f.notiz;
    o.ruhe = f.ruhe;
    o.sicht = f.sicht;
    o.material = f.material;
    o.wc3 = f.wc3;
    return o;
}

// ------------------------------------------------------------
//  Laden
// ------------------------------------------------------------
void Laden(Fenster& f) {
    const LRESULT sel = SendDlgItemMessageW(f.h, IDC_A_LISTE, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR || static_cast<size_t>(sel) >= f.sichtbar.size()) { Status(f, L"Select an animation first."); return; }
    std::wstring bericht;
    HCURSOR alt = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    const bool ok = WendeAnimAn(f.sichtbar[static_cast<size_t>(sel)], Optionen(f), bericht);
    SetCursor(alt);
    Status(f, bericht, !ok);
    if (ok) {
        f.sequenzen.clear();
        FuelleSequenzen(f);
    }
}

void AlleLaden(Fenster& f) {
    if (f.sichtbar.empty()) { Status(f, L"No animations in the list."); return; }
    const AnimOptionen o = Optionen(f);
    if (f.sichtbar.size() > 40) {
        wchar_t frage[400];
        swprintf_s(frage, L"Load %zu animations one after another (gap %d frames)?\n\nThe timeline starts at frame 0 with the bind pose. Esc cancels.",
                   f.sichtbar.size(), o.abstand);
        if (MessageBoxW(f.h, frage, L"XFBIN Animations", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    }
    SchreibeEinstellung(L"Abstand", std::to_wstring(o.abstand));
    Status(f, L"Setting keys for " + std::to_wstring(f.sichtbar.size()) + L" animations... (Esc cancels)");
    HCURSOR alt = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    std::wstring bericht;
    std::vector<Sequenz> plan;
    const bool gut = WendeFolgeAn(f.sichtbar, o, plan, bericht);
    SetCursor(alt);
    Status(f, bericht, !gut);
    if (gut) {
        f.sequenzen = plan;
        FuelleSequenzen(f);
    }
}

// ------------------------------------------------------------
//  Zeichnen
// ------------------------------------------------------------
struct Spalten { int name, bilder, gruppe, ende; };

Spalten SpaltenFuer(const RECT& r, int rand) {
    const int b = static_cast<int>(r.right - r.left) - 2 * rand;
    Spalten s{};
    s.name = r.left + rand;
    s.gruppe = r.left + rand + b * 62 / 100;
    s.bilder = s.gruppe - b * 12 / 100;
    s.ende = r.right - rand;
    return s;
}

void Text(HDC dc, const std::wstring& t, int links, int rechts, const RECT& r, UINT ausrichtung) {
    RECT z = { links, r.top, rechts, r.bottom };
    DrawTextW(dc, t.c_str(), -1, &z, ausrichtung | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
}

void ZeichneKopf(const Fenster& f, const DRAWITEMSTRUCT& d) {
    HDC dc = d.hDC;
    const RECT r = d.rcItem;
    FillRect(dc, &r, f.pal.pinselGrund);
    const int rand = std::max(4, f.zeilenHoehe / 3);
    const Spalten s = SpaltenFuer(r, rand);
    HGDIOBJ alt = SelectObject(dc, f.fontFett != nullptr ? f.fontFett : f.fontNormal);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, f.pal.dim);
    Text(dc, L"Name", s.name, s.bilder - rand, r, DT_LEFT);
    Text(dc, L"Frames", s.bilder, s.gruppe - 2 * rand, r, DT_RIGHT);
    Text(dc, L"Group", s.gruppe, s.ende, r, DT_LEFT);
    SelectObject(dc, alt);
}

void ZeichneZeile(const Fenster& f, const DRAWITEMSTRUCT& d) {
    HDC dc = d.hDC;
    const RECT r = d.rcItem;
    const bool sel = (d.itemState & ODS_SELECTED) != 0;
    HBRUSH hg = CreateSolidBrush(sel ? f.pal.auswahl : f.pal.feld);
    FillRect(dc, &r, hg);
    DeleteObject(hg);
    const auto& anims = GetXfbinInterface()->Anims();
    if (d.itemID == static_cast<UINT>(-1) || d.itemID >= f.sichtbar.size()) return;
    const int idx = f.sichtbar[d.itemID];
    if (idx < 0 || static_cast<size_t>(idx) >= anims.size()) return;
    const xfbin::Anm& a = anims[static_cast<size_t>(idx)];
    const int rand = std::max(4, f.zeilenHoehe / 3);
    const Spalten s = SpaltenFuer(r, rand);
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ alt = SelectObject(dc, f.fontNormal);
    SetTextColor(dc, sel ? f.pal.auswahlText : f.pal.feldText);
    Text(dc, Breit(a.name) + (a.loopFlag ? L"   (loop)" : L""), s.name, s.bilder - rand, r, DT_LEFT);
    SetTextColor(dc, sel ? f.pal.auswahlDim : f.pal.feldDim);
    Text(dc, std::to_wstring(static_cast<int>(std::lround(a.frameCount))), s.bilder, s.gruppe - 2 * rand, r, DT_RIGHT);
    Text(dc, Breit(Gruppe(idx)), s.gruppe, s.ende, r, DT_LEFT);
    SelectObject(dc, alt);
    if (!sel) xfui::Trennlinie(f.pal, dc, r, rand);
}

void SchalteUm(Fenster& f, bool& wert, const wchar_t* schluessel, int knopf, const wchar_t* an, const wchar_t* aus) {
    wert = !wert;
    SchreibeEinstellung(schluessel, wert ? L"1" : L"0");
    InvalidateRect(GetDlgItem(f.h, knopf), nullptr, TRUE);
    Status(f, wert ? an : aus);
}

INT_PTR Verarbeite(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    Fenster* f = Zustand(h);
    switch (msg) {
    case WM_INITDIALOG: {
        f = reinterpret_cast<Fenster*>(lp);
        f->h = h;
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(f));
        f->pal.Baue(&ThemeFarbe);
        f->fontNormal = reinterpret_cast<HFONT>(SendMessageW(h, WM_GETFONT, 0, 0));
        LOGFONTW lf{};
        if (f->fontNormal != nullptr && GetObjectW(f->fontNormal, sizeof(lf), &lf) == sizeof(lf)) {
            lf.lfWeight = FW_SEMIBOLD;
            f->fontFett = CreateFontIndirectW(&lf);
        }
        f->zeilenHoehe = xfui::ZeilenHoehe(h);
        xfui::DunkleTitelleiste(h, f->pal.dunkel);
        if (f->pal.dunkel) {
            SetWindowTheme(GetDlgItem(h, IDC_A_LISTE), L"DarkMode_Explorer", nullptr);
            for (int id : { IDC_A_ZEIGE, IDC_A_SEQUENZ }) SetWindowTheme(GetDlgItem(h, id), L"DarkMode_CFD", nullptr);
        }
        SendDlgItemMessageW(h, IDC_A_LISTE, LB_SETITEMHEIGHT, 0, f->zeilenHoehe + 6);
        SetWindowTextW(h, (std::wstring(L"XFBIN Animations ") + XFBINIMPORT_VERSION_STR).c_str());
        const std::wstring figur = SzenenFigurName();
        SetDlgItemTextW(h, IDC_A_FIGUR, figur.empty() ? L"(no character imported)" : figur.c_str());
        const std::wstring abstand = LiesEinstellung(L"Abstand");
        SetDlgItemInt(h, IDC_A_ABSTAND, abstand.empty() ? 10 : static_cast<UINT>(_wtoi(abstand.c_str())), FALSE);
        f->notiz = LiesEinstellung(L"Notizspur") != L"0";
        f->ruhe = LiesEinstellung(L"Ruhekeys") != L"0";
        f->sicht = LiesEinstellung(L"Sichtbarkeit") != L"0";
        f->material = LiesEinstellung(L"Materialanim") != L"0";
        f->wc3 = LiesEinstellung(L"WC3") == L"1";
        SendDlgItemMessageW(h, IDC_A_SUCHE, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"e.g.  run  or  skl1"));
        LiesSequenzen(f->sequenzen);
        FuelleSequenzen(*f);
        FuelleGruppen(*f);
        Fuelle(*f);
        TestFotoStarten(h);
        return TRUE;
    }
    case WM_TIMER:
        if (f != nullptr && wp == kTestFotoTimer) {
            SendDlgItemMessageW(h, IDC_A_LISTE, LB_SETCURSEL, 0, 0);
            UpdateWindow(h);
            TestFotoMachen(h, L"animationen");
            EndDialog(h, 1);
        }
        return TRUE;
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* m = reinterpret_cast<MEASUREITEMSTRUCT*>(lp);
        const int zh = (f != nullptr) ? f->zeilenHoehe : xfui::ZeilenHoehe(h);
        m->itemHeight = static_cast<UINT>(m->CtlType == ODT_COMBOBOX ? zh + 4 : zh + 6);
        return TRUE;
    }
    case WM_DRAWITEM: {
        if (f == nullptr) break;
        const DRAWITEMSTRUCT* d = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
        switch (d->CtlID) {
        case IDC_A_LISTE: ZeichneZeile(*f, *d); return TRUE;
        case IDC_A_KOPF: ZeichneKopf(*f, *d); return TRUE;
        case IDC_A_STATUS: xfui::ZeichneStatus(f->pal, f->fontNormal, *d, f->status, f->statusFehler); return TRUE;
        case IDC_A_ZEIGE:
        case IDC_A_SEQUENZ: xfui::ZeichneAuswahlfeld(f->pal, f->fontNormal, *d); return TRUE;
        case IDC_A_NOTIZ: xfui::ZeichneKnopf(f->pal, f->fontNormal, *d, f->notiz); return TRUE;
        case IDC_A_RUHE: xfui::ZeichneKnopf(f->pal, f->fontNormal, *d, f->ruhe); return TRUE;
        case IDC_A_SICHT: xfui::ZeichneKnopf(f->pal, f->fontNormal, *d, f->sicht); return TRUE;
        case IDC_A_MATERIAL: xfui::ZeichneKnopf(f->pal, f->fontNormal, *d, f->material); return TRUE;
        case IDC_A_WC3: xfui::ZeichneKnopf(f->pal, f->fontNormal, *d, f->wc3); return TRUE;
        case IDC_A_LADEN: xfui::ZeichneKnopf(f->pal, f->fontNormal, *d, true); return TRUE;
        default: xfui::ZeichneKnopf(f->pal, f->fontNormal, *d, false); return TRUE;
        }
    }
    case WM_COMMAND: {
        if (f == nullptr) break;
        const int code = HIWORD(wp);
        switch (LOWORD(wp)) {
        case IDC_A_SUCHE:
            if (code == EN_CHANGE) Fuelle(*f);
            return TRUE;
        case IDC_A_ZEIGE:
            if (code == CBN_SELCHANGE) {
                f->zeige = static_cast<int>(SendDlgItemMessageW(h, IDC_A_ZEIGE, CB_GETCURSEL, 0, 0));
                Fuelle(*f);
            }
            return TRUE;
        case IDC_A_SEQUENZ:
            if (code == CBN_SELCHANGE) ZeigeSequenz(*f);
            return TRUE;
        case IDC_A_NOTIZ:
            SchalteUm(*f, f->notiz, L"Notizspur", IDC_A_NOTIZ, L"Note track on: sequence names on the scene root plus NeoDexSequenceData (WhiteoutDex).",
                      L"Note track off.");
            return TRUE;
        case IDC_A_RUHE:
            SchalteUm(*f, f->ruhe, L"Ruhekeys", IDC_A_RUHE, L"Rest keys on: bones an animation does not move get bind-pose keys at both ends, so sequences do not bleed into each other.",
                      L"Rest keys off.");
            return TRUE;
        case IDC_A_SICHT:
            SchalteUm(*f, f->sicht, L"Sichtbarkeit", IDC_A_SICHT, L"Visibility on: step keys per sequence from the opacity data (weapons and effects appear and vanish).",
                      L"Visibility off: everything stays visible.");
            return TRUE;
        case IDC_A_MATERIAL:
            SchalteUm(*f, f->material, L"Materialanim", IDC_A_MATERIAL, L"Material on: UV scrolling, tiling and opacity of the materials are keyed.",
                      L"Material animation off.");
            return TRUE;
        case IDC_A_WC3:
            SchalteUm(*f, f->wc3, L"WC3", IDC_A_WC3,
                      L"WC3 mode on: the character stays on the spot (jumps keep their height) and effects get hard on/off visibility per sequence - made for Warcraft III.",
                      L"WC3 mode off: everything as the game does it - the character travels, effects fade, scale and scroll like in the game.");
            return TRUE;
        case IDC_A_LISTE:
            if (code == LBN_DBLCLK) Laden(*f);
            return TRUE;
        case IDC_A_LADEN: Laden(*f); return TRUE;
        case IDC_A_ALLE: AlleLaden(*f); return TRUE;
        case IDOK: Laden(*f); return TRUE;
        case IDCANCEL: EndDialog(h, 1); return TRUE;
        default: break;
        }
        break;
    }
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        if (f != nullptr && f->pal.pinselGrund != nullptr) {
            SetTextColor(reinterpret_cast<HDC>(wp), f->pal.text);
            SetBkColor(reinterpret_cast<HDC>(wp), f->pal.grund);
            return reinterpret_cast<INT_PTR>(f->pal.pinselGrund);
        }
        break;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        if (f != nullptr && f->pal.pinselFeld != nullptr) {
            SetTextColor(reinterpret_cast<HDC>(wp), f->pal.feldText);
            SetBkColor(reinterpret_cast<HDC>(wp), f->pal.feld);
            return reinterpret_cast<INT_PTR>(f->pal.pinselFeld);
        }
        break;
    case WM_PAINT: {
        if (f == nullptr) break;
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        for (int id : { IDC_A_SUCHE, IDC_A_ABSTAND, IDC_A_LISTE }) xfui::Kante(h, dc, id, f->pal.linie);
        EndPaint(h, &ps);
        return TRUE;
    }
    case WM_DESTROY:
        if (f != nullptr) {
            if (f->fontFett != nullptr) DeleteObject(f->fontFett);
            f->pal.Frei();
        }
        break;
    default:
        break;
    }
    return FALSE;
}

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

int OeffneAnimFenster(HWND eltern) {
    Fenster f;
    const INT_PTR r = DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_ANIMATIONEN), eltern != nullptr ? eltern : GetCOREInterface()->GetMAXHWnd(),
                                      &DlgProc, reinterpret_cast<LPARAM>(&f));
    return r == -1 ? -1 : 1;
}

} // namespace xfi
