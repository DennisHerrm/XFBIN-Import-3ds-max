// ============================================================
//  XFBIN Texturen und Materialien
//
//  Stufe 4: wertet nuccChunkTexture (mit eingebettetem
//  NUT-Container) und nuccChunkMaterial aus und schreibt die
//  Texturen als DDS heraus.
//
//  Wie die anderen Parser-Ebenen ohne Bindung an das Max SDK.
//
//  ------------------------------------------------------------
//  WARUM DDS UND KEINE UMWANDLUNG
//  ------------------------------------------------------------
//  Ein NUT enthaelt genau die Pixeldaten, die auch in einer
//  DDS-Datei stehen - nur ohne deren Kopf. Fuer DXT1, DXT3 und
//  DXT5 muss gar nichts gerechnet werden: Kopf davor, Daten
//  dahinter, fertig.
//
//  Bei den unkomprimierten Formaten kommt eine einzige
//  Zusatzarbeit dazu: NUT legt sie in Big-Endian ab, DDS
//  erwartet Little-Endian. Also 16- oder 32-bitweise drehen, je
//  nach Format.
//
//  Der Weg ueber DDS spart damit einen kompletten
//  DXT-Dekomprimierer und deckt trotzdem alle Formate ab, die
//  das Format kennt.
// ============================================================

#pragma once

#include "xfbin_reader.h"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace xfbin {

// Eine Textur innerhalb eines NUT-Containers.
struct NutTexture {
    uint16_t width  = 0;
    uint16_t height = 0;
    uint8_t  pixelFormat  = 0;
    uint8_t  mipmapCount  = 1;
    bool     isCubeMap    = false;

    // Alle Mipmap-Ebenen hintereinander, so wie sie in die
    // DDS-Datei gehoeren.
    std::vector<uint8_t> data;

    // Groesse der einzelnen Ebenen. Leer bei nur einer Ebene.
    std::vector<uint32_t> mipmapSizes;
};

struct XfbinTexture {
    RawString name;
    uint16_t  width  = 0;      // aus dem Chunk-Kopf, nicht aus dem NUT
    uint16_t  height = 0;
    std::vector<NutTexture> textures;

    size_t pageIndex     = 0;
    uint32_t localMapIndex = 0;
};

struct MaterialTextureGroup {
    int32_t unk = 0;
    std::vector<RawString> textureNames;
};

struct XfbinMaterial {
    RawString name;

    uint8_t alpha = 255;
    float   glare = 0.0f;
    uint32_t flags = 0;

    float uv0[4] = { 0, 0, 0, 0 };
    float uv1[4] = { 0, 0, 0, 0 };
    float uv2[4] = { 0, 0, 0, 0 };
    float uv3[4] = { 0, 0, 0, 0 };
    float blendRate = 0.0f;
    float blendType = 0.0f;
    float fallOff   = 0.0f;
    float outlineId = 0.0f;

    std::vector<MaterialTextureGroup> groups;

    size_t   pageIndex     = 0;
    uint32_t localMapIndex = 0;

    // Erste Textur der ersten Gruppe - im Regelfall die
    // Farbtextur. Leer, wenn das Material keine hat.
    RawString DiffuseTexture() const;
};

bool ParseTextures(const XfbinFile& file, std::vector<XfbinTexture>& out,
                   std::string& error, std::string& warnings);

bool ParseMaterials(const XfbinFile& file, std::vector<XfbinMaterial>& out,
                    std::string& error, std::string& warnings);

// Aus den page-lokalen Materialindizes eines Modells die Namen
// der zugehoerigen Materialien machen. Die Reihenfolge ist die
// des Modells - Index i gehoert zur Material-ID i der Submeshes.
std::vector<RawString> ResolveModelMaterials(const XfbinFile& file,
                                             size_t pageIndex,
                                             const std::vector<uint32_t>& materialIndices);

// ------------------------------------------------------------
//  DDS schreiben.
//
//  Gibt false zurueck und fuellt why, wenn das Pixelformat
//  unbekannt ist oder die Datei nicht schreibbar war.
// ------------------------------------------------------------
bool WriteDds(const NutTexture& tex, const std::string& path, std::string& why);

// Schreibt alle Texturen in ein Verzeichnis. Der Dateiname ist
// der Chunk-Name; enthaelt ein Chunk mehrere Texturen, wird
// _0, _1 usw. angehaengt.
//
// Rueckgabe: Anzahl geschriebener Dateien. names bekommt die
// erzeugten Dateinamen, in derselben Reihenfolge wie textures.
int ExportTextures(const std::vector<XfbinTexture>& textures,
                   const std::string& directory,
                   std::vector<std::string>& fileNames,
                   std::string& warnings);

std::string MakeTextureSummary(const std::vector<XfbinTexture>& textures,
                               const std::vector<XfbinMaterial>& materials);

} // namespace xfbin
