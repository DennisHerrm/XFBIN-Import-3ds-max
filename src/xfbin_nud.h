// ============================================================
//  XFBIN NUD - Meshes
//
//  Stufe 2: wertet nuccChunkModel und den darin eingebetteten
//  NUD-Block aus. Wie die anderen Parser-Ebenen ohne Bindung an
//  das 3ds Max SDK.
//
//  Referenz: br_nud.py und nud.py aus xfbin_lib, die ihrerseits
//  auf der NUD-Implementierung von Smash Forge beruhen.
//
//  ------------------------------------------------------------
//  DAS VERTEXFORMAT - die Stelle, die man dreimal falsch macht
//  ------------------------------------------------------------
//  Ein NUD-Mesh beschreibt sein Vertexlayout ueber zwei Bytes:
//
//    vertexFlags & 0x0F   Vertex-Typ   (Position, Normale, Tangenten)
//    vertexFlags & 0xF0   Bone-Typ     (Gewichte, oder keine)
//    uvColorFlags >> 4    Anzahl UV-Kanaele
//    uvColorFlags & 0x0F  UV-/Farbformat
//
//  Bit 0x04 im Vertex-Typ schaltet den GESAMTEN Block zwischen
//  Voll- und Halbgleitkomma um - und dreht dabei die Bedeutung
//  von 0x01 und 0x02 um:
//
//    ohne 0x04 (voll):  0x01 = Normalen, 0x02 = Tangenten
//    mit  0x04 (halb):  0x02 = Normalen, 0x01 = Tangenten
//
//  Und der Fallstrick, der die meiste Zeit kostet:
//
//    Bei GESKINNTEN Meshes (Bone-Typ != 0) liegen Position,
//    Normale und Gewichte im vertAddClump, waehrend UVs und
//    Farben getrennt davon im vertClump stehen. Bei
//    ungeskinnten Meshes steht alles zusammen im vertClump,
//    verschachtelt in einem einzigen Datensatz.
//
//  Innerhalb des UV-/Farbblocks kommt die FARBE ZUERST, danach
//  die UV-Kanaele.
// ============================================================

#pragma once

#include "xfbin_reader.h"

#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace xfbin {

struct Vec2 { float x = 0.0f, y = 0.0f; };
struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };

// Vier Farbkanaele als Byte - so liegen sie in der Datei bei
// uvColorType & 2. Die Half-Float-Variante wird beim Lesen auf
// denselben Bereich 0..255 gebracht.
struct Color4 { uint8_t r = 255, g = 255, b = 255, a = 255; };

struct NudMesh {
    uint8_t vertexFlags  = 0;
    uint8_t uvColorFlags = 0;
    uint8_t faceFlag     = 0;

    int uvCount = 0;

    // Struktur von Arrays statt Array von Strukturen: die Daten
    // gehen als Block weiter an Max, und pro Vertex ein Objekt
    // anzulegen waere bei 22.000 Vertices reine Verschwendung.
    std::vector<Vec3>   position;
    std::vector<Vec3>   normal;        // leer, wenn nicht vorhanden
    std::vector<Color4> color;         // leer, wenn nicht vorhanden
    std::vector<std::vector<Vec2>> uv; // uv[kanal][vertex]

    std::vector<std::array<uint32_t, 4>> boneIds;
    std::vector<std::array<float, 4>>    boneWeights;

    // Aufgeloeste Dreiecke, drei Indizes je Dreieck.
    std::vector<uint32_t> triangles;

    // Wie viele Streifen die Datei enthielt und wie viele
    // entartete Dreiecke dabei weggefallen sind - reine
    // Diagnose, sagt aber sofort, ob der Streifenaufloeser
    // richtig laeuft.
    int stripCount     = 0;
    int degenerateDropped = 0;

    bool HasBones()  const { return !boneIds.empty(); }
    bool HasColor()  const { return !color.empty(); }
    bool HasNormal() const { return !normal.empty(); }

    size_t VertexCount()   const { return position.size(); }
    size_t TriangleCount() const { return triangles.size() / 3; }
};

struct NudMeshGroup {
    RawString name;
    uint16_t  boneFlags  = 0;
    int16_t   singleBind = -1;
    std::vector<NudMesh> meshes;
};

struct NudModel {
    RawString name;
    uint16_t  version = 0;

    // Herkunft - siehe Clump::pageIndex.
    size_t   pageIndex     = 0;
    uint32_t localMapIndex = 0;

    uint16_t riggingFlag = 0;
    uint16_t attributes  = 0;
    uint32_t clumpIndex  = 0;
    uint32_t hitIndex    = 0;
    uint32_t meshBoneIndex = 0;      // Index in die Coord-Liste des Clumps

    uint8_t  renderLayer = 0;
    uint8_t  lightModeId = 0;

    bool  hasBoundingBox = false;
    float boundingBox[6] = { 0, 0, 0, 0, 0, 0 };
    float boundingSphere[4] = { 0, 0, 0, 0 };

    // Bone-Bereich aus dem NUD-Kopf. Wird in Stufe 3 gebraucht.
    uint16_t boneStart = 0;
    uint16_t boneEnd   = 0;

    std::vector<NudMeshGroup> groups;
    std::vector<uint32_t>     materialIndices;

    size_t VertexCount() const;
    size_t TriangleCount() const;
};

// ------------------------------------------------------------
//  Alle nuccChunkModel einer eingelesenen Datei auswerten.
//  Modelle ohne NUD-Block (etwa nuccChunkModelPrimitiveBatch)
//  werden uebersprungen und in warnings gemeldet.
// ------------------------------------------------------------
bool ParseModels(const XfbinFile& file, std::vector<NudModel>& out,
                 std::string& error, std::string& warnings);

// Halbgleitkomma nach float. Oeffentlich, weil Stufe 5 sie
// ebenfalls braucht.
float HalfToFloat(uint16_t h);

void WriteMeshDump(const std::vector<NudModel>& models, const std::string& label,
                   std::ostream& out, bool includeVertices);

std::string MakeMeshSummary(const std::vector<NudModel>& models);

} // namespace xfbin
