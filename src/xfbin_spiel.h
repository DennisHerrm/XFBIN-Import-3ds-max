// ============================================================
//  XFBIN Spiel - installierte Storm-Spiele finden, ihre Archive
//  zu einem Dateibestand zusammenlegen und daraus den
//  Figurenkatalog bauen.
//
//  Ohne Max SDK, wie die Parser - xfbindump benutzt dieselbe
//  Ebene ("xfbindump --spiel <ordner> --katalog").
//
//  ------------------------------------------------------------
//  DATEIBESTAND
//  ------------------------------------------------------------
//  Ein Spiel verteilt seine Dateien auf viele CPKs, und manche
//  Datei steht mehrfach darin: Storm 4 legt Korrekturen in
//  data/patch/12/, Storm 3 bringt einen dlc-Ordner mit neuerer
//  Fassung. Jeder Pfad zeigt deshalb auf eine LISTE von Quellen,
//  hoechster Rang zuerst - laesst sich die neueste nicht lesen,
//  kommt die naechste dran.
//
//  Statt eines Spiels geht auch ein Ordner mit losen, bereits
//  ausgepackten .xfbin - dann gilt der Pfad ab "data/".
//
//  ------------------------------------------------------------
//  FIGUREN
//  ------------------------------------------------------------
//  Alles steht in data/spc und ist nach einem vierstelligen Code
//  benannt - "1nrt" ist Naruto aus Teil 1:
//
//    1nrtbod1        Modell (Kostuem 1; bod2 = Kostuem 2 ...)
//    1nrtbod1c/l/s/i Animationen: Grundbewegung, Kombos, Support,
//                    Intro und Sieg
//    1nrtskl1, spl1, awa   Jutsus, Ultimate, Erwachen
//
//  Eine Figur bekommt nur Dateien, die mit IHREM Code beginnen.
//  Fremde Finisher ("1garspl1_fin_1nrt" - Gaaras Ultimate mit
//  Naruto als Opfer) gehoeren nicht dazu.
// ============================================================

#pragma once

#include "xfbin_cpk.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace xfbin {

struct SpielOrt {
    std::wstring name;      // Anzeigename, z.B. "Ultimate Ninja STORM 4"
    std::wstring ordner;    // Installationsordner
};

// Installierte Storm-Spiele aus allen Steam-Bibliotheken.
std::vector<SpielOrt> FindeSpiele();

// Passt der Ordner? Ein Spiel (CPKs) oder ausgepackte Daten.
bool IstSpielOrdner(const std::wstring& ordner);

// Anzeigename aus dem Ordnernamen.
std::wstring SpielName(const std::wstring& ordner);

class Spieldaten {
public:
    // Archive suchen und den Bestand aufbauen. Liest nur die
    // Inhaltsverzeichnisse - ein paar hundert Millisekunden.
    bool Oeffne(const std::wstring& ordner, std::string& error);

    const std::wstring& Ordner() const { return ordner_; }
    size_t ArchivZahl() const { return archive_.size(); }
    size_t DateiZahl()  const { return index_.size(); }

    bool Gibt(const std::string& pfad) const;
    bool Lies(const std::string& pfad, std::vector<uint8_t>& out, std::string& error) const;

    // Alle Pfade (klein geschrieben, "/"), sortiert.
    std::vector<std::string> Pfade() const;

private:
    struct Quelle {
        int          archiv  = -1;   // Index in archive_, -1 = lose Datei
        size_t       eintrag = 0;
        std::wstring datei;          // bei losen Dateien
    };
    std::wstring                                ordner_;
    std::vector<std::unique_ptr<CpkArchive>>    archive_;
    std::map<std::string, std::vector<Quelle>>  index_;   // Rang absteigend
};

// ------------------------------------------------------------
//  Katalog
// ------------------------------------------------------------
enum FigurArt {
    kArtFigur  = 0,   // spielbar (Code beginnt mit 1, 2 oder 3)
    kArtAndere = 1,   // Bosse, Gegner, Sonderformen
    kArtAlle   = 2,
};

struct FigurEintrag {
    std::string code;                // "1nrt"
    int         kostuem = 1;         // die N aus bodN
    std::string modell;              // "data/spc/1nrtbod1.xfbin"
    std::vector<std::string> anims;  // eigene Animationsdateien
    std::string gemeinsam;           // geteilte Reaktionen, z.B. data/spc/1cmnbod1.xfbin
    std::string name;                // "Naruto Uzumaki"
    std::string zusatz;              // "Part I", "Costume 2" ...
    int         art = kArtFigur;
    int         reihenfolge = 9999;  // Platz in characode.bin
};

struct Katalog {
    std::vector<FigurEintrag> figuren;
};

Katalog BaueKatalog(const Spieldaten& daten);

// Name des Skelett-Clumps einer Figur, so wie er im Modell und in
// den Animationen steht: "1nrtbod1".
std::string ClumpName(const FigurEintrag& f);

// Gruppe einer Animationsdatei fuer die Anzeige ("Basic", "Combos" ...).
std::string AnimGruppe(const FigurEintrag& f, const std::string& pfad);

std::string Klein(std::string s);
std::string DateiName(const std::string& pfad);

} // namespace xfbin
