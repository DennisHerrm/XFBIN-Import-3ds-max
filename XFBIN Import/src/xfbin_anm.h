// ============================================================
//  XFBIN ANM - Animationen
//
//  Stufe 5: wertet nuccChunkAnm aus. Wie die anderen
//  Parser-Ebenen ohne Bindung an das 3ds Max SDK.
//
//  ------------------------------------------------------------
//  ZEIT
//  ------------------------------------------------------------
//  Die Zeitachse laeuft in HUNDERTSTEL-FRAMES. frame_size ist in
//  allen bisher gesehenen Dateien 100. Kurven mit fester
//  Schrittweite tragen keinen eigenen Zeitwert - ihr Key i liegt
//  implizit auf i * frame_size. Kurven mit dem Zusatz "linear"
//  bringen pro Key einen eigenen Zeitwert mit.
//
//  ------------------------------------------------------------
//  QUANTISIERUNG
//  ------------------------------------------------------------
//  Mehrere Formate speichern int16 statt float, aber mit
//  UNTERSCHIEDLICHEN Teilern:
//
//    Quaternionen und Deckkraft   / 0x8000
//    Skalierung                   / 0x1000
//    Farben (uint8)               / 0xFF
//
//  Wer hier einen Teiler verwechselt, bekommt eine Animation,
//  die fast stimmt - und sucht lange.
//
//  ------------------------------------------------------------
//  AUSRICHTUNG
//  ------------------------------------------------------------
//  Nach JEDER Kurve wird auf 4 Bytes ausgerichtet, nicht nur am
//  Ende eines Eintrags.
// ============================================================

#pragma once

#include "xfbin_reader.h"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace xfbin {

// Formatkennungen, wie sie im Kurvenkopf stehen.
enum AnmCurveFormat : uint16_t {
    kVector3Fixed              = 5,
    kVector3Linear             = 6,
    kVector3Bezier             = 7,
    kEulerXyzFixed             = 8,
    kEulerInterpolate          = 9,
    kQuaternionLinear          = 10,
    kFloatFixed                = 11,
    kFloatLinear               = 12,
    kVector2Fixed              = 13,
    kVector2Linear             = 14,
    kOpacityI16Tbl             = 15,
    kScaleI16Tbl               = 16,
    kQuaternionI16Tbl          = 17,
    kColorRgbTbl               = 20,
    kVector3Tbl                = 21,
    kFloatTblNi                = 22,
    kQuaternionTbl             = 23,
    kFloatTbl                  = 24,
    kVector3I16Linear          = 25,
    kVector3TblNoInterp        = 26,
    kQuaternionI16TblNoInterp  = 27,
    kOpacityI16TblNoInterp     = 29,
};

// Was ein Eintrag beschreibt.
enum AnmEntryFormat : uint16_t {
    kEntryBone       = 1,
    kEntryCamera     = 2,
    kEntryMaterial   = 4,
    kEntryLightDirc  = 5,
    kEntryLightPoint = 6,
    kEntryAmbient    = 8,
    kEntryMorph      = 9,
};

// Ein Key. frame ist bereits in ganzen Frames umgerechnet
// (Rohwert / frame_size), frameRaw behaelt die Hundertstel.
struct AnmKey {
    int    frameRaw = 0;
    double frame    = 0.0;
    int    count    = 0;             // belegte Werte in value
    double value[4] = { 0, 0, 0, 0 };
};

struct AnmCurve {
    uint16_t curveIndex   = 0;
    uint16_t curveFormat  = 0;
    uint16_t keyframeCount = 0;
    int16_t  curveFlags   = 0;

    // Nach welcher Groesse die Kurve steuert. Aus curveIndex und
    // curveFormat abgeleitet - siehe BoneChannelOf().
    std::vector<AnmKey> keys;

    bool unsupported = false;        // Format nicht implementiert
};

// Kanal eines Bone-Eintrags.
enum AnmBoneChannel {
    kChannelUnknown = -1,
    kChannelLocation = 0,
    kChannelRotationQuat = 1,
    kChannelScale = 2,
    kChannelOpacity = 3,
    kChannelRotationEuler = 4,
};

// Welchen Kanal eine Kurve eines Bone-Eintrags steuert.
//
// Der Zusammenhang ist nicht ganz geradlinig: normalerweise
// zaehlt curveIndex (0 = Position, 1 = Rotation, 2 = Skalierung,
// 3 = Deckkraft). Bei den beiden Euler-Formaten wird stattdessen
// curveIndex + 10 gerechnet, sodass aus der 1 eine 11 wird -
// so unterscheidet die Datei Euler- von Quaternion-Rotation.
AnmBoneChannel BoneChannelOf(uint16_t curveIndex, uint16_t curveFormat);

// ------------------------------------------------------------
//  Kanaele eines MATERIAL-Eintrags
//
//  Andere Bedeutung als bei Bones: hier zaehlt der curveIndex
//  eine feste Liste ab. Die Zuordnung stammt aus
//  create_material_curves in anm.py.
//
//  In den Testdaten sind fast alle Kurven einwertig, also
//  konstant. Wirklich animiert sind der Offset der ersten
//  UV-Ebene (U0/V0) und die Deckkraft - klassisches UV-Scrollen
//  fuer Augen, Haare und Effektflaechen.
// ------------------------------------------------------------
enum AnmMaterialChannel {
    kMatUnknown = -1,
    kMatU0Offset = 0,
    kMatV0Offset = 1,
    kMatU1Offset = 2,
    kMatV1Offset = 3,
    kMatU2Offset = 4,
    kMatV2Offset = 5,
    kMatU3Offset = 6,
    kMatV3Offset = 7,
    kMatU0Scale  = 8,
    kMatV0Scale  = 9,
    kMatU1Scale  = 10,
    kMatV1Scale  = 11,
    kMatBlendRate1 = 12,
    kMatBlendRate2 = 13,
    kMatFalloff  = 14,
    kMatGlare    = 15,
    kMatAlpha    = 16,
    kMatOutlineId = 17,
    kMatU2Scale  = 18,
    kMatV2Scale  = 19,
    kMatU3Scale  = 20,
    kMatV3Scale  = 21,
};

AnmMaterialChannel MaterialChannelOf(uint16_t curveIndex);

struct AnmEntry {
    int16_t  clumpIndex  = 0;        // -1 = Kamera / Licht / Ambient
    uint16_t boneIndex   = 0;
    uint16_t entryFormat = 0;
    std::vector<AnmCurve> curves;

    // Aufgeloest ueber die Referenztabelle der Page.
    RawString targetName;            // Name des Ziel-Chunks
    RawString clumpName;
};

struct AnmClumpRef {
    uint32_t  clumpIndex = 0;
    RawString clumpName;
    std::vector<RawString> boneNames;
    std::vector<RawString> modelNames;
};

struct AnmCoordParent {
    int16_t  parentClump = 0;
    uint16_t parentCoord = 0;
    int16_t  childClump  = 0;
    uint16_t childCoord  = 0;
};

struct Anm {
    RawString name;
    uint16_t  version = 0;

    uint32_t frameCountRaw = 0;      // in Hundertstel-Frames
    uint32_t frameSize     = 100;
    double   frameCount    = 0.0;    // in Frames

    uint16_t loopFlag        = 0;
    uint16_t otherEntryCount = 0;
    uint16_t otherIndexCount = 0;

    std::vector<AnmClumpRef>    clumps;
    std::vector<AnmCoordParent> coordParents;
    std::vector<AnmEntry>       entries;

    size_t KeyframeCount() const;

    // Mindestens ein Bone-Eintrag? Post-process-/Kamera-/Licht-only
    // Clips (Blur, Glare, DOF, ColorFilter, ...) haben oft keine und
    // duerfen in der Sequenz nicht wie Skelett-Animationen behandelt
    // werden - sonst knallt buildMaterialAnim/buildVisibility an
    // Zielen, die in der Max-Szene nicht existieren.
    bool HasBoneEntries() const;
};

bool ParseAnims(const XfbinFile& file, std::vector<Anm>& out,
                std::string& error, std::string& warnings);

void WriteAnimDump(const std::vector<Anm>& anims, const std::string& label,
                   std::ostream& out, bool includeKeys);

std::string MakeAnimSummary(const std::vector<Anm>& anims);

} // namespace xfbin
