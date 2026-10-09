// ============================================================
//  XFBIN CPK - die Archive der Ninja-Storm-Spiele lesen
//
//  Die Spiele legen ihre .xfbin nicht lose ab, sondern in CRI-CPK-
//  Archiven (data1.cpk, dataRegion.cpk ...). Diese Ebene oeffnet
//  ein Archiv, liest das Inhaltsverzeichnis und holt einzelne
//  Dateien fertig ausgepackt heraus. Wie die Parser-Ebenen ohne
//  Bindung an das 3ds Max SDK - xfbindump benutzt sie genauso.
//
//  ------------------------------------------------------------
//  AUFBAU
//  ------------------------------------------------------------
//  Alle Tabellen sind CRI-"@UTF"-Tabellen, big-endian:
//
//    0x0000  "CPK " + 12 Bytes, dann @UTF  Kopf (TocOffset, ...)
//    TocOffset  "TOC " + 12 Bytes, dann @UTF  eine Zeile je Datei:
//               DirName, FileName, FileSize, ExtractSize, FileOffset
//
//  FileOffset zaehlt ab min(TocOffset, ContentOffset, 0x800).
//
//  ------------------------------------------------------------
//  WAS IN EINEM EINTRAG STECKEN KANN
//  ------------------------------------------------------------
//  Je nach Spiel liegt eine Datei in einer von mehreren Huellen,
//  teils ineinander. Read() schaelt sie der Reihe nach ab, bis
//  NUCC-Daten uebrig sind:
//
//    verschluesselt  Storm 4 und Connections: XOR mit einem
//                    xorshift128-Schluesselstrom ab Dateianfang
//    CRILAYLA        CRIs LZ-Kompression, rueckwaerts gelesen
//    CPK im CPK      Storm 3: jede gepackte Datei ist ein eigenes
//                    kleines CPK. Ohne Namen (ITOC), die Datei ist
//                    in Stuecke geteilt, jedes fuer sich gepackt.
// ============================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace xfbin {

struct CpkEntry {
    std::string dir;              // "data/spc"
    std::string name;             // "1nrtbod1.xfbin"
    uint64_t    offset      = 0;  // absolut im Archiv
    uint64_t    size        = 0;  // gespeicherte Groesse
    uint64_t    extractSize = 0;  // Groesse ausgepackt

    std::string Path() const { return dir.empty() ? name : dir + "/" + name; }
};

class CpkArchive {
public:
    // Liest Kopf und Inhaltsverzeichnis. Die Nutzdaten bleiben
    // auf der Platte; Read() holt sie bei Bedarf.
    bool Open(const std::wstring& path, std::string& error);

    const std::wstring&          Path()    const { return path_; }
    const std::vector<CpkEntry>& Entries() const { return entries_; }

    // Eintrag lesen und alle Huellen abschaelen. out enthaelt
    // danach die eigentliche Datei.
    bool Read(const CpkEntry& entry, std::vector<uint8_t>& out,
              std::string& error) const;

    // Nur die gespeicherten Bytes, ohne Auspacken - fuer Diagnose.
    bool ReadRaw(const CpkEntry& entry, std::vector<uint8_t>& out,
                 std::string& error) const;

private:
    std::wstring          path_;
    std::vector<CpkEntry> entries_;
};

// Huellen von einem Puffer abschaelen: Entschluesseln, CRILAYLA,
// CPK im CPK - so lange, bis nichts mehr davon erkennbar ist.
bool UnwrapPayload(std::vector<uint8_t>& data, std::string& error);

// Einzelne Schritte, fuer Tests.
bool DecompressCrilayla(const uint8_t* data, size_t size,
                        std::vector<uint8_t>& out, std::string& error);
bool LooksEncrypted(const uint8_t* data, size_t size);
void DecryptStormData(uint8_t* data, size_t size);

} // namespace xfbin
