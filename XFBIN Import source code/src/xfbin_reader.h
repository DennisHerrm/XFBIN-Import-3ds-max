// ============================================================
//  XFBIN Container Reader - Header
//
//  Stufe 0: Header, Chunk-Tabelle und Page-/Chunk-Struktur.
//  Die Nutzdaten der einzelnen Chunks (Clump, Coord, Model/NUD,
//  Anm, ...) werden hier NOCH NICHT ausgewertet - sie liegen als
//  roher Byte-Block in XfbinChunk::data bereit.
//
//  WICHTIG: Diese Uebersetzungseinheit haengt bewusst NICHT am
//  3ds Max SDK. Nur <cstdint>, <string>, <vector>, <iosfwd>.
//  Dadurch laesst sie sich
//    - im Plugin verwenden,
//    - als eigenstaendiges CLI-Werkzeug (xfbindump) bauen,
//    - auf einem anderen Rechner gegen die Python-Lib diffen.
//  Bitte nicht mit Max-Typen "verbessern".
// ============================================================

#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace xfbin {

// ------------------------------------------------------------
//  Strings im XFBIN sind cp932 (Shift-JIS), nicht UTF-8.
//  Auf dieser Ebene werden sie NICHT dekodiert, sondern als rohe
//  Bytes gehalten. Grund: die Dekodierung braucht eine
//  Codepage-Tabelle (unter Windows MultiByteToWideChar mit 932),
//  und die soll den Parser nicht plattformabhaengig machen.
//  Die Plugin-Seite dekodiert beim Uebergang nach MCHAR.
// ------------------------------------------------------------
using RawString = std::string;

struct XfbinHeader {
    uint32_t nuccId        = 0;
    uint32_t chunkTableSize = 0;
    uint32_t minPageSize   = 0;
    uint16_t nuccId2       = 0;
    uint16_t unk           = 0;
};

struct ChunkMap {
    uint32_t typeIndex = 0;
    uint32_t pathIndex = 0;
    uint32_t nameIndex = 0;
};

struct ChunkMapReference {
    uint32_t nameIndex = 0;
    uint32_t mapIndex  = 0;
};

struct ChunkTable {
    std::vector<RawString> types;
    std::vector<RawString> paths;
    std::vector<RawString> names;
    std::vector<ChunkMap>          maps;
    std::vector<ChunkMapReference> references;
    std::vector<uint32_t>          mapIndices;
};

// Ein Chunk innerhalb einer Page.
struct XfbinChunk {
    // Aufgeloest ueber maps[mapIndices[pageStart + localMapIndex]]
    const RawString* type = nullptr;   // zeigt in ChunkTable::types
    const RawString* path = nullptr;
    const RawString* name = nullptr;

    uint32_t localMapIndex = 0;   // page-relativ, so wie in der Datei
    uint32_t globalMapIndex = 0;  // nach der Aufloesung
    uint16_t version = 0;         // im Python-Code "nuccId" - steuert das Layout
    uint16_t unk     = 0;         // bei manchen Anm-Chunks != 0

    uint64_t dataOffset = 0;      // Offset im Dateipuffer (fuer Diagnose)
    std::vector<uint8_t> data;    // Nutzdaten, noch nicht ausgewertet
};

struct XfbinPage {
    std::vector<XfbinChunk> chunks;

    uint32_t pageSize      = 0;   // aus dem nuccChunkPage
    uint32_t referenceSize = 0;

    // Ausschnitte der globalen Tabellen, die zu dieser Page gehoeren.
    std::vector<uint32_t>          pageMapIndices;
    std::vector<ChunkMapReference> pageReferences;
};

struct XfbinFile {
    XfbinHeader            header;
    ChunkTable             table;
    std::vector<XfbinPage> pages;

    // Bequemlichkeit fuer die spaeteren Stufen.
    size_t TotalChunks() const;
    // Anzahl Chunks eines Typs, z.B. "nuccChunkModel".
    size_t CountOfType(const char* typeName) const;
};

// ------------------------------------------------------------
//  Ergebnis eines Leseversuchs.
//  Kein Exception-Werfen nach aussen: das Plugin laeuft im
//  Max-Prozess, und eine durchgereichte Exception dort ist ein
//  Absturz, kein Fehlerdialog.
// ------------------------------------------------------------
struct ReadResult {
    bool        ok = false;
    std::string error;      // leer, wenn ok
    std::string warnings;   // nicht-fatale Hinweise, "\n"-getrennt
};

// Liest eine XFBIN-Datei vollstaendig ein.
ReadResult ReadXfbinFile(const std::string& path, XfbinFile& out);

// Gleiche Funktion auf einem bereits geladenen Puffer.
ReadResult ReadXfbinBuffer(const uint8_t* data, size_t size, XfbinFile& out);

// ------------------------------------------------------------
//  Text-Dump zum Gegenpruefen gegen die Python-Lib.
//
//  Das Format ist bewusst zeilenweise und deterministisch, damit
//  ein simples diff genuegt. Nicht-ASCII-Bytes werden als \xNN
//  ausgegeben - dadurch bleibt der Dump unabhaengig von der
//  Codepage des Terminals und trotzdem exakt vergleichbar.
// ------------------------------------------------------------
void WriteDump(const XfbinFile& file, const std::string& label,
               std::ostream& out, bool includeTables = true);

// Kurzfassung: nur Zaehlwerte und Chunk-Typ-Verteilung.
std::string MakeSummary(const XfbinFile& file);

} // namespace xfbin
