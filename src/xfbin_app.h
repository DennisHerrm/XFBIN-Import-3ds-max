// ============================================================
//  xfbin_app.h - die C++-Oberflaeche und der Ablauf dahinter
//
//  Bis 2.x steuerte ein MaxScript-Fenster (XfbinImport.mcr) den
//  Import Schritt fuer Schritt ueber XfbinCpp.*. Jetzt stehen
//  Fenster und Ablauf in C++ und lesen direkt aus den Spielen -
//  Aufbau wie beim Dragon Ball Legends und Bleach Import:
//
//    xfbin_fenster.cpp      Figurenfenster: Spiel, Reiter, Suche,
//                           Liste, Import
//    xfbin_animfenster.cpp  Animationsfenster: Liste der eigenen
//                           Animationen, Laden, Zeitleiste, WC3
//    xfbin_ablauf.cpp       Einstellungen, Protokoll, Import einer
//                           Figur, Animationen und Sequenzen
//
//  Die Bausteine darunter (Skelett, Meshes, Skin, Texturen,
//  Keys) bleiben die erprobten Funktionen von
//  XfbinImportInterface.
// ============================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "xfbin_spiel.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

class XfbinImportInterface;

namespace xfi {

std::wstring Breit(const std::string& s);
std::string  Utf8(const std::wstring& w);

// Ablage unter %LOCALAPPDATA%\XfbinImport - Einstellungen,
// Protokolle, ausgepackte Texturen.
std::wstring Ablage();
std::wstring LiesEinstellung(const std::wstring& schluessel);
void SchreibeEinstellung(const std::wstring& schluessel, const std::wstring& wert);

void LogNeu(const char* titel);
void Log(const char* format, ...);

uint32_t ThemeFarbe(int welche);

// Spieldaten und Katalog fuer einen Ordner - einmal gelesen und
// gehalten, bis ein anderer Ordner kommt.
bool Daten(const std::wstring& ordner, const xfbin::Spieldaten*& daten,
           const xfbin::Katalog*& katalog, std::string& fehler);

// Bevorzugter Ordner: der zuletzt benutzte, sonst das neueste
// gefundene Storm-Spiel.
std::wstring StartOrdner();

// ------------------------------------------------------------
//  Import einer Figur
// ------------------------------------------------------------
struct ImportOptionen {
    bool  texturen   = true;
    bool  reaktionen = false;   // geteilte Treffer-/Fallanimationen (1cmnbod1)
    bool  effekte    = true;    // Effektmodelle, die die Animationen ansprechen
    bool  wc3        = false;   // fuer WhiteoutDex/Warcraft III: je Material ein Mesh, Blick nach -Y
    bool  leeren     = false;   // Szene vorher leeren
    bool  ebenen     = true;
    bool  medit      = true;
    bool  lodWeg     = true;
    bool  normalen   = true;
    bool  skin       = true;
    int   knochenModus   = 1;     // 0 = Punkt-Helfer, 1 = Bone-Objekte
    float knochenGroesse = 0.0f;  // 0 = blosse Linien
    float massstab       = 1.0f;  // Zentimeter 1:1
};

bool ImportiereFigur(const xfbin::Spieldaten& daten, const xfbin::FigurEintrag& f,
                     const ImportOptionen& o, std::wstring& bericht);

// Welche Figur steht gerade in der Szene (zuletzt importiert)?
// Leer, wenn keine.
const xfbin::FigurEintrag* SzenenFigur();
std::wstring SzenenFigurName();

// ------------------------------------------------------------
//  Animationen
// ------------------------------------------------------------
struct Sequenz {
    std::string name;
    int start = 0;
    int ende  = 0;
};

struct AnimOptionen {
    int   abstand  = 10;
    bool  notiz    = true;    // Notizspur + NeoDexSequenceData
    bool  ruhe     = true;    // Ruhelage-Keys an beiden Enden
    bool  sicht    = true;    // Sichtbarkeit je Sequenz
    bool  material = true;    // UV-/Deckkraft-Animation
    bool  wc3      = false;   // auf der Stelle (Warcraft III)
    float massstab = 1.0f;
};

bool WendeAnimAn(int index, const AnimOptionen& o, std::wstring& bericht);
bool WendeFolgeAn(const std::vector<int>& indizes, const AnimOptionen& o,
                  std::vector<Sequenz>& plan, std::wstring& bericht);
bool LiesSequenzen(std::vector<Sequenz>& aus);
void ZeigeBereich(int startBild, int endeBild);

// ------------------------------------------------------------
//  Effekte (xfbin_effekte.cpp) - Partikel, gestartete Effekt-
//  Animationen, Sichtbarkeit
// ------------------------------------------------------------
struct Abschnitt { int index; int start; int ende; };

struct EffektVorbereitung {
    std::set<std::string>    clumps;     // zu bauende Effekt-Clumps
    std::set<std::string>    sprites;    // Billboards/Clumps der Schwaerme
    std::vector<std::string> dateien;    // Effektdateien, in denen sie stehen
    size_t animsFx = 0;
    size_t emitter = 0;
};

struct EffektFenster {
    std::map<ULONG, std::vector<std::pair<double, double>>> fenster;   // Knoten -> sichtbar [a, b)
};

void EffekteZuruecksetzen();
void EffekteVorbereiten(XfbinImportInterface& x, const xfbin::Spieldaten& d, const std::vector<std::string>& animDateien,
                        EffektVorbereitung& v);
std::vector<ULONG> EffekteAnbinden(XfbinImportInterface& x, const xfbin::Spieldaten& d, const std::wstring& texOrdner, float massstab,
                                   std::set<std::string>& effektClumps, std::string& log);
void EffektKeysLoeschen();
void SpawnsSetzen(XfbinImportInterface& x, int index, double start, double ende, float massstab, EffektFenster& f, int tiefe = 0);
size_t EffektSichtbarkeit(XfbinImportInterface& x, const std::vector<Abschnitt>& abschnitte, const EffektFenster& f,
                          const std::set<std::string>& effektClumps, bool wc3, bool bindPose);

// ------------------------------------------------------------
//  Fenster
// ------------------------------------------------------------
int OeffneFenster();
int OeffneAnimFenster(HWND eltern);

// Datei -> Importieren: eine .xfbin oder .cpk oeffnet das Fenster
// mit ihrem Spiel.
int ImportiereEingang(const wchar_t* pfad);

// Selbsttest: XFBINIMPORT_TESTFOTO=<pfad> laesst die Fenster sich
// nach vier Sekunden selbst als BMP ablichten und schliessen.
constexpr UINT_PTR kTestFotoTimer = 0x7F01;
void TestFotoStarten(HWND h);
bool TestFotoMachen(HWND h, const wchar_t* name);

} // namespace xfi
