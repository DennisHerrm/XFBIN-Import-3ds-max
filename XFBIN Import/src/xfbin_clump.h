// ============================================================
//  XFBIN Clump / Coord - Skelett
//
//  Stufe 1: wertet nuccChunkClump und nuccChunkCoord aus und
//  baut daraus die Knochenhierarchie samt Weltmatrizen.
//
//  Wie xfbin_reader haengt diese Uebersetzungseinheit NICHT am
//  3ds Max SDK. Die Matrizen werden deshalb hier selbst
//  gerechnet - in derselben Konvention, die Max benutzt, damit
//  das Ergebnis 1:1 in eine Matrix3 kippt.
//
//  ------------------------------------------------------------
//  MATRIXKONVENTION - bitte einmal lesen
//  ------------------------------------------------------------
//  Blender/mathutils rechnet mit SPALTENvektoren: v' = M * v,
//  und verkettet "Elternteil zuerst links": world = parent @ local.
//
//  3ds Max rechnet mit ZEILENvektoren: v' = v * M. Dadurch dreht
//  sich die Reihenfolge jeder Verkettung um:
//      Blender  world = parent @ local
//      Max      world = local * parent
//
//  Die Rotation im nuccChunkCoord ist ein Euler-Tripel in GRAD,
//  das der Blender-Importer mit der Reihenfolge 'ZYX' liest -
//  also Z zuerst, dann Y, dann X. In Spaltenschreibweise ergibt
//  das R = Rx * Ry * Rz, in Zeilenschreibweise entsprechend
//  R = Rz * Ry * Rx. Genau so steht es unten in MakeRotation().
//
//  Die lokale Matrix ist Blenders Matrix.LocRotScale(p, r, s),
//  also (Spalten) T * R * S - Skalierung zuerst. In Zeilen:
//  S * R * T.
// ============================================================

#pragma once

#include "xfbin_reader.h"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace xfbin {

// ------------------------------------------------------------
//  Affine 4x3-Matrix in ZEILENvektor-Konvention.
//  Zeilen 0..2 sind die Basisvektoren, Zeile 3 die Translation -
//  dieselbe Anordnung wie Matrix3 im Max SDK. Damit laesst sich
//  das Ergebnis ohne Umsortieren uebernehmen.
// ------------------------------------------------------------
struct Mat43 {
    // Bewusst double, obwohl Max' Matrix3 float ist.
    //
    // Die Weltmatrizen entstehen durch Verkettung ueber die
    // Hierarchie - bei diesem Rig bis zu 14 Ebenen tief. In float
    // gerechnet weicht das Ergebnis in der sechsten
    // Nachkommastelle von einer double-Rechnung ab. Praktisch
    // egal, aber es macht den Abgleich gegen die Python-Referenz
    // unscharf: statt "identisch" haette man nur noch "nah dran",
    // und damit kein brauchbares Pruefkriterium mehr.
    // Umgewandelt wird erst beim Uebergang in die Matrix3.
    double m[4][3];

    static Mat43 Identity();
    static Mat43 Translation(double x, double y, double z);
    static Mat43 Scale(double x, double y, double z);

    // Einzelachsen, Winkel in GRAD, Rechte-Hand-Regel.
    static Mat43 RotationX(double deg);
    static Mat43 RotationY(double deg);
    static Mat43 RotationZ(double deg);
};

// Zeilenvektor-Verkettung: das Ergebnis wendet a VOR b an.
Mat43 operator*(const Mat43& a, const Mat43& b);

// Euler-Tripel in Grad -> Rotationsmatrix, Reihenfolge ZYX
// (Z zuerst). Siehe Konventionsblock oben.
Mat43 MakeRotation(double rxDeg, double ryDeg, double rzDeg);

// ------------------------------------------------------------
//  Quaternion aus einer Animationskurve -> Zeilenvektor-Matrix.
//
//  Die Reihenfolge in der Datei ist (x, y, z, w).
//
//  Warum die Matrix nach der GEWOHNTEN Spaltenformel gefuellt
//  wird, obwohl Mat43 eine Zeilenvektor-Matrix ist - hergeleitet,
//  nicht geraten:
//
//    Der Blender-Importer bildet die lokale Rotation eines Bones
//    als conj(q) ab; das steht so in convert_anm_values, und der
//    Umweg ueber die Bind-Rotation in
//    convert_anm_values_tranformed kuerzt sich zu genau dem
//    weg (bind * conj(bind) * conj(q) = conj(q)).
//
//    Blender rechnet in Spaltenvektoren. Gesucht ist also
//    Row = ColMat(conj(q))^T.
//
//    Fuer Rotationen gilt ColMat(conj(q)) = ColMat(q)^T, also
//    Row = (ColMat(q)^T)^T = ColMat(q).
//
//  Ergebnis: die Spaltenformel, Feld fuer Feld in die
//  Zeilenmatrix geschrieben.
//
//  Der Umschalter fuer die Konvention sitzt NICHT hier, sondern
//  beim Setzen des Rotations-Controllers - dort gehoert er hin,
//  weil er dann auch fuer die Euler-Kurven gilt.
// ------------------------------------------------------------
Mat43 MakeRotationFromQuat(double x, double y, double z, double w);

// Vollstaendige lokale Matrix eines Coord-Knotens.
Mat43 MakeLocal(const float pos[3], const float rot[3], const float scl[3]);

// ------------------------------------------------------------
//  Ein Knoten aus nuccChunkCoord.
// ------------------------------------------------------------
struct CoordNode {
    RawString name;          // rohe cp932-Bytes, wie in der Datei

    float position[3] = { 0.0f, 0.0f, 0.0f };   // Zentimeter
    float rotation[3] = { 0.0f, 0.0f, 0.0f };   // Euler, GRAD
    float scale[3]    = { 1.0f, 1.0f, 1.0f };
    float opacity     = 1.0f;
    uint16_t flags    = 0;

    int  parent = -1;                 // Index in Clump::nodes, -1 = Wurzel
    std::vector<int> children;

    Mat43 local;                      // relativ zum Elternteil
    Mat43 world;                      // absolut

    // Vorzeichen der Skalierung getrennt merken.
    // Der Blender-Importer macht das genauso: eine negative
    // Skalierung direkt anzuwenden zerstoert die Rotation, und
    // Max geht damit noch unfreundlicher um als Blender. Fuer
    // einen spaeteren Export muss das Vorzeichen aber erhalten
    // bleiben.
    int scaleSigns[3] = { 1, 1, 1 };
};

// ------------------------------------------------------------
//  Ein nuccChunkClump samt aufgeloester Hierarchie.
// ------------------------------------------------------------
struct Clump {
    RawString name;

    // Herkunft, damit ein Modell "seinen" Clump wiederfindet:
    // nuccChunkModel::clumpIndex ist ein page-LOKALER Chunkindex.
    // Ohne diese beiden Felder muesste man annehmen, dass es nur
    // einen Clump gibt - was fuer Stage-Dateien nicht stimmt.
    size_t   pageIndex     = 0;
    uint32_t localMapIndex = 0;

    uint32_t field00 = 0;             // 2 = mit Bounding Box
    bool     hasBoundingBox = false;
    float    boundingBox[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    int modelGroupCount = 0;          // coordFlag0 - LOD-Gruppen
    int extraGroupCount = 0;          // coordFlag1

    std::vector<CoordNode> nodes;
    std::vector<int>       roots;     // Indizes der Wurzelknoten

    // Reihenfolge, in der die Knoten von den Wurzeln aus erreicht
    // werden. Elternteil steht immer vor seinen Kindern - damit
    // kann man stumpf durchlaufen und braucht keine Rekursion,
    // wenn man Knoten in der Szene anlegt.
    std::vector<int> depthFirst;

    int Depth(int nodeIndex) const;
};

// ------------------------------------------------------------
//  Alle Clumps einer bereits eingelesenen Datei auswerten.
//  Gibt false zurueck, wenn ein Clump nicht lesbar war; die
//  uebrigen bleiben trotzdem in out.
// ------------------------------------------------------------
bool ParseClumps(const XfbinFile& file, std::vector<Clump>& out,
                 std::string& error, std::string& warnings);

// ------------------------------------------------------------
//  Text-Dump der Skelette, zeilenweise und deterministisch -
//  zum Diffen gegen tools/pydump_bones.py.
//
//  Zahlen mit fester Nachkommastellenzahl, damit sich zwei
//  Implementierungen ueberhaupt vergleichen lassen.
// ------------------------------------------------------------
void WriteBoneDump(const std::vector<Clump>& clumps, const std::string& label,
                   std::ostream& out);

std::string MakeBoneSummary(const std::vector<Clump>& clumps);

} // namespace xfbin
