// ============================================================
//  XFBIN Import Plugin - Header
//  Version 0.1.0  (Stufe 0: Container lesen und dumpen)
//
//  MaxScript-API - alles unter XfbinCpp.*:
//
//    XfbinCpp.open        <path>              -> int   Anzahl Chunks, -1 = Fehler
//    XfbinCpp.close()                         -> int   1
//    XfbinCpp.isOpen()                        -> int   0/1
//    XfbinCpp.dump        <outPath> <tables>  -> int   1 = geschrieben
//
//    XfbinCpp.pageCount()                     -> int
//    XfbinCpp.chunkCount()                    -> int
//    XfbinCpp.countOfType <typeName>          -> int
//    XfbinCpp.namesOfType <typeName>          -> string  "\n"-getrennt
//    XfbinCpp.summary()                       -> string
//
//  Stufe 1 - Skelett:
//    XfbinCpp.parseSkeleton()                 -> int   Bone-Anzahl, -1 = Fehler
//    XfbinCpp.clumpCount()                    -> int
//    XfbinCpp.boneCount()                     -> int
//    XfbinCpp.boneSummary()                   -> string
//    XfbinCpp.boneDump    <outPath>           -> int
//    XfbinCpp.buildSkeleton <mode> <scale>    -> int   angelegte Knoten
//        mode  0 = Point-Helper (Standard), 1 = Bone-Objekte
//        scale 1.0 = Zentimeter 1:1 uebernehmen
//
//  Stufe 2 - Meshes:
//    XfbinCpp.parseMeshes()                   -> int   Vertexanzahl, -1 = Fehler
//    XfbinCpp.modelCount()                    -> int
//    XfbinCpp.meshSummary()                   -> string
//    XfbinCpp.meshDump <outPath> <withVerts>  -> int
//    XfbinCpp.buildMeshes <skipLod> <normals> <scale> -> int  angelegte Objekte
//        skipLod  1 = Modelle mit "_lod" im Namen auslassen
//        normals  1 = explizite Vertexnormalen setzen
//
//  Stufe 3 - Skinning:
//    XfbinCpp.buildMeshesSkinned <skipLod> <normals> <skin> <scale>
//                                             -> int  angelegte Objekte
//        skin 1 = Skin-Modifier mit Gewichten auf geskinnte Modelle
//
//  buildMeshes ruft intern buildMeshesSkinned mit skin=1 auf; die
//  alte Signatur bleibt damit gueltig.
//
//  Stufe 5 - Animationen:
//    XfbinCpp.parseAnims()                    -> int   Keyframes, -1 = Fehler
//    XfbinCpp.parseAnimsAppend()              -> int   anhaengen statt ersetzen
//    XfbinCpp.clearAnims()                    -> int   geladene Animationen verwerfen
//    XfbinCpp.animCount()                     -> int
//    XfbinCpp.animName    <index>             -> string  (0-basiert)
//    XfbinCpp.animSummary()                   -> string
//    XfbinCpp.animDump    <outPath> <withKeys> -> int
//    XfbinCpp.buildAnim   <index> <scale>     -> int   gesetzte Keys
//    XfbinCpp.buildAnimEx <index> <mask> <scale> -> int
//        mask: Bit 1 = Position, 2 = Rotation, 4 = Skalierung.
//        Zum Eingrenzen: welcher Kanal verzerrt?
//    XfbinCpp.buildAnimAt <index> <startFrame> <mask> <scale> -> int
//        wie buildAnimEx, aber mit Zeitversatz. Fuer den
//        Sequenzmodus, der alle Animationen hintereinanderlegt.
//    XfbinCpp.animFrames  <index>             -> float  Laenge in Frames
//    XfbinCpp.animHasBones <index>            -> int    0 = reiner Effektclip
//    XfbinCpp.buildBindPoseKey <frame>        -> int    Bones mit Key
//    XfbinCpp.buildIdleKeys <index> <start> <end> -> int
//        Ruhelage-Keys an beiden Enden fuer alles, was diese
//        Animation nicht anfasst. Macht eine Sequenz in sich
//        abgeschlossen.
//    XfbinCpp.buildMaterialAnim <index> <start> <end> -> int
//        UV-Offset, Kachelung und Deckkraft der Materialien.
//    XfbinCpp.buildVisibility <index> <start> <end> -> int
//        Sichtbarkeit der Objekte aus der Deckkraft-Kurve. Was
//        eine Animation gar nicht anspricht, wird ausgeblendet.
//    XfbinCpp.sceneRootName()                 -> string Name des Wurzel-Bones
//
//  Instanzen - ein Charakter kann dasselbe Modell mehrfach
//  tragen (zwei gleiche Waffen):
//    XfbinCpp.fileClumpName()                 -> string
//        Clump-Name der GEOEFFNETEN DATEI (nicht der Szene)
//    XfbinCpp.requiredInstances <clumpName>   -> int
//    XfbinCpp.buildSkeletonN <mode> <scale> <copies>          -> int
//    XfbinCpp.buildMeshesN <skipLod> <normals> <skin> <scale> <copies> -> int
//        copies = 0 laesst das Plugin je Clump selbst nachsehen,
//        wie viele Exemplare die Animationen erwarten. Das ist der
//        Normalfall - eine Datei kann Skelette mit ganz
//        unterschiedlichem Bedarf enthalten.
//    XfbinCpp.setBoneSize <groesse>           -> int
//        Breite und Hoehe der Bone-Objekte beim Anlegen.
//        0 = blosse Linien, negativ = Max' Standardwert.
//    XfbinCpp.setQuatMode <0|1>               -> int
//        0 = Rotation direkt uebernehmen (Standard), 1 = konjugieren
//        wirkt auf Quaternion- UND Euler-Kurven
//
//  Stufe 4 - Texturen und Materialien:
//    XfbinCpp.parseTextures()                 -> int   Anzahl Texturen
//    XfbinCpp.textureCount()                  -> int
//    XfbinCpp.materialCount()                 -> int
//    XfbinCpp.textureSummary()                -> string
//    XfbinCpp.exportTextures <dir>            -> int   geschriebene DDS
//    XfbinCpp.buildMaterials <dir>            -> int   zugewiesene Objekte
//
//  Szenenzustand - ueberlebt einen Dateiwechsel:
//    XfbinCpp.sceneBoneCount()                -> int
//    XfbinCpp.sceneClumpName()                -> string
//    XfbinCpp.clearScene()                    -> int
//    XfbinCpp.pruneScene()                    -> int
//    XfbinCpp.pendingFolder()                 -> string
//        Ordner aus Max' Import-Dialog, einmalig abholbar
//        wirft Eintraege weg, deren Knoten geloescht wurden
//    XfbinCpp.sceneReport()                   -> string
//        groesste Objekte am aktuellen Frame, mit Skalierung,
//        Elternteil und Skin-Bone-Anzahl
//
//  Der uebliche Ablauf ist zwei Dateien lang: erst das Modell
//  oeffnen und Bones plus Meshes anlegen, dann die
//  Animationsdatei oeffnen und Animationen setzen. Das Skelett
//  bleibt dabei erhalten - es steht ja in der Szene, nicht in
//  der Datei.
//
//    XfbinCpp.version()                       -> string
//    XfbinCpp.lastError()                     -> string  "" = kein Fehler
//    XfbinCpp.warnings()                      -> string
//    XfbinCpp.log()                           -> string
//    XfbinCpp.timings()                       -> string
//    XfbinCpp.setDebug    <0|1>               -> int
//
//  Schnelltest im Listener:
//
//    XfbinCpp.setDebug 1
//    XfbinCpp.open @"D:\xfbin\1hakbod1.xfbin"
//    format "%\n" (XfbinCpp.summary())
//    XfbinCpp.dump @"D:\xfbin\max_dump.txt" 1
//
//  Der geschriebene Dump ist zeilenweise mit dem Dump der
//  Python-Lib vergleichbar - siehe tools/pydump.py.
// ============================================================

#pragma once

#include <max.h>
#include <utilapi.h>
#include <ifnpub.h>
#include <hold.h>

#include "xfbin_reader.h"
#include "xfbin_clump.h"
#include "xfbin_nud.h"
#include "xfbin_anm.h"
#include "xfbin_tex.h"

#include <array>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>

// ============================================================
//  Class IDs
//
//  Frei gewaehlt und einmalig. Nicht aendern, sobald eine Szene
//  Daten dieses Plugins enthaelt.
// ============================================================

#define XFBINIMPORT_CLASS_ID     Class_ID(0x2C614F13, 0x5A0D7B22)

// Eigene Kennung fuer den Eintrag im Import-Dialog. Zwei Klassen
// in einer DLL brauchen zwei verschiedene.
#define XFBINIMPORT_SCENE_CLASS_ID Class_ID(0x5d3a71e4, 0x2c86f019)
#define XFBINIMPORT_INTERFACE_ID Interface_ID(0x2C614F13, 0x5A0D7B23)

#define XFBINIMPORT_VERSION_STR  _T("3.0.0")

// ============================================================
//  Function IDs
// ============================================================

enum XfbinImportFnID {
    fn_open = 0,
    fn_close,
    fn_isOpen,
    fn_dump,
    fn_pageCount,
    fn_chunkCount,
    fn_countOfType,
    fn_namesOfType,
    fn_summary,
    fn_version,
    fn_lastError,
    fn_warnings,
    fn_log,
    fn_timings,
    fn_setDebug,
    // --- Stufe 1 ---
    fn_parseSkeleton,
    fn_clumpCount,
    fn_boneCount,
    fn_boneSummary,
    fn_boneDump,
    fn_buildSkeleton,
    // --- Stufe 2 ---
    fn_parseMeshes,
    fn_modelCount,
    fn_meshSummary,
    fn_meshDump,
    fn_buildMeshes,
    // --- Stufe 3 ---
    fn_buildMeshesSkinned,
    // --- Stufe 5 ---
    fn_parseAnims,
    fn_parseAnimsAppend,
    fn_clearAnims,
    fn_animCount,
    fn_animName,
    fn_animSummary,
    fn_animDump,
    fn_buildAnim,
    fn_buildAnimEx,
    fn_buildAnimAt,
    fn_animFrames,
    fn_animHasBones,
    fn_buildBindPoseKey,
    fn_buildIdleKeys,
    fn_buildVisibility,
    fn_sceneRootName,
    fn_fileClumpName,
    fn_requiredInstances,
    fn_buildSkeletonN,
    fn_buildMeshesN,
    fn_setQuatMode,
    fn_setBoneSize,
    fn_sceneBoneCount,
    fn_sceneClumpName,
    fn_clearScene,
    fn_pruneScene,
    fn_pendingFolder,
    fn_sceneReport,
    fn_layerReport,
    fn_buildMaterialAnim,
    // --- Stufe 4 ---
    fn_parseTextures,
    fn_textureCount,
    fn_materialCount,
    fn_textureSummary,
    fn_exportTextures,
    fn_buildMaterials,
    // --- 3.0: C++-Fenster und Spieldaten ---
    fn_showDialog,
    fn_showAnimDialog,
    fn_importCharacter,
    fn_applyAnimation,
    fn_applySequence,
};

// ============================================================
//  RAII-Guards
//
//  Uebernommen aus AnimMerge - dort seit 5.x bewaehrt. In Stufe 0
//  noch ungenutzt, weil nichts in die Szene geschrieben wird. Sie
//  stehen hier trotzdem schon, damit ab Stufe 1 (Bones anlegen)
//  niemand auf die Idee kommt, die Aufrufe von Hand zu paaren.
// ============================================================

// Stellt sicher, dass EnableSceneRedraw() auch bei einer Exception
// oder einem fruehen return wieder aufgerufen wird. Ohne das bleibt
// 3ds Max im Fehlerfall eingefroren zurueck.
class SceneRedrawGuard {
public:
    explicit SceneRedrawGuard(Interface* ip) : ip_(ip) {
        if (ip_) ip_->DisableSceneRedraw();
    }
    ~SceneRedrawGuard() {
        if (ip_) ip_->EnableSceneRedraw();
    }
    SceneRedrawGuard(const SceneRedrawGuard&)            = delete;
    SceneRedrawGuard& operator=(const SceneRedrawGuard&) = delete;
private:
    Interface* ip_;
};

// Haelt das Undo-System waehrend der Batch-Operationen an.
// theHold zaehlt Suspend/Resume intern, verschachteln ist erlaubt.
// Ohne das legt Max pro Objekt einen Undo-Record an - bei 222 Bones
// und 19 Meshes ist das der Unterschied zwischen Sekunden und
// Minuten, und ein spaeteres Undo hinterlaesst eine halb
// zurueckgerollte Szene.
class HoldSuspendGuard {
public:
    HoldSuspendGuard()  { theHold.Suspend(); }
    ~HoldSuspendGuard() { theHold.Resume();  }
    HoldSuspendGuard(const HoldSuspendGuard&)            = delete;
    HoldSuspendGuard& operator=(const HoldSuspendGuard&) = delete;
};

// ============================================================
//  FPStaticInterface - XfbinCpp.xxx() in MaxScript
// ============================================================

class XfbinImportInterface : public FPStaticInterface {
public:
    DECLARE_DESCRIPTOR(XfbinImportInterface)

    BEGIN_FUNCTION_MAP
        FN_1(fn_open,        TYPE_INT,    Open,        TYPE_STRING)
        FN_0(fn_close,       TYPE_INT,    Close)
        FN_0(fn_isOpen,      TYPE_INT,    IsOpen)
        FN_2(fn_dump,        TYPE_INT,    Dump,        TYPE_STRING, TYPE_INT)
        FN_0(fn_pageCount,   TYPE_INT,    PageCount)
        FN_0(fn_chunkCount,  TYPE_INT,    ChunkCount)
        FN_1(fn_countOfType, TYPE_INT,    CountOfType, TYPE_STRING)
        FN_1(fn_namesOfType, TYPE_STRING, NamesOfType, TYPE_STRING)
        FN_0(fn_summary,     TYPE_STRING, GetSummary)
        FN_0(fn_version,     TYPE_STRING, GetVersion)
        FN_0(fn_lastError,   TYPE_STRING, GetLastError)
        FN_0(fn_warnings,    TYPE_STRING, GetWarnings)
        FN_0(fn_log,         TYPE_STRING, GetLog)
        FN_0(fn_timings,     TYPE_STRING, GetTimings)
        FN_1(fn_setDebug,    TYPE_INT,    SetDebug,    TYPE_INT)
        FN_0(fn_parseSkeleton,  TYPE_INT,    ParseSkeleton)
        FN_0(fn_clumpCount,     TYPE_INT,    ClumpCount)
        FN_0(fn_boneCount,      TYPE_INT,    BoneCount)
        FN_0(fn_boneSummary,    TYPE_STRING, GetBoneSummary)
        FN_1(fn_boneDump,       TYPE_INT,    BoneDump,      TYPE_STRING)
        FN_2(fn_buildSkeleton,  TYPE_INT,    BuildSkeleton, TYPE_INT, TYPE_FLOAT)
        FN_0(fn_parseMeshes,    TYPE_INT,    ParseMeshes)
        FN_0(fn_modelCount,     TYPE_INT,    ModelCount)
        FN_0(fn_meshSummary,    TYPE_STRING, GetMeshSummary)
        FN_2(fn_meshDump,       TYPE_INT,    MeshDump,      TYPE_STRING, TYPE_INT)
        FN_3(fn_buildMeshes,    TYPE_INT,    BuildMeshes,   TYPE_INT, TYPE_INT, TYPE_FLOAT)
        FN_4(fn_buildMeshesSkinned, TYPE_INT, BuildMeshesSkinned,
             TYPE_INT, TYPE_INT, TYPE_INT, TYPE_FLOAT)
        FN_0(fn_parseAnims,   TYPE_INT,    ParseAnims)
        FN_0(fn_parseAnimsAppend, TYPE_INT, ParseAnimsAppend)
        FN_0(fn_clearAnims,   TYPE_INT,    ClearAnims)
        FN_0(fn_animCount,    TYPE_INT,    AnimCount)
        FN_1(fn_animName,     TYPE_STRING, GetAnimName,   TYPE_INT)
        FN_0(fn_animSummary,  TYPE_STRING, GetAnimSummary)
        FN_2(fn_animDump,     TYPE_INT,    AnimDump,      TYPE_STRING, TYPE_INT)
        FN_2(fn_buildAnim,    TYPE_INT,    BuildAnim,     TYPE_INT, TYPE_FLOAT)
        FN_3(fn_buildAnimEx,  TYPE_INT,    BuildAnimEx,   TYPE_INT, TYPE_INT, TYPE_FLOAT)
        FN_4(fn_buildAnimAt,  TYPE_INT,    BuildAnimAt,
             TYPE_INT, TYPE_FLOAT, TYPE_INT, TYPE_FLOAT)
        FN_1(fn_animFrames,   TYPE_FLOAT,  AnimFrames,    TYPE_INT)
        FN_1(fn_animHasBones, TYPE_INT,    AnimHasBones,  TYPE_INT)
        FN_1(fn_buildBindPoseKey, TYPE_INT, BuildBindPoseKey, TYPE_FLOAT)
        FN_3(fn_buildIdleKeys, TYPE_INT, BuildIdleKeys,
             TYPE_INT, TYPE_FLOAT, TYPE_FLOAT)
        FN_3(fn_buildVisibility, TYPE_INT, BuildVisibility,
             TYPE_INT, TYPE_FLOAT, TYPE_FLOAT)
        FN_0(fn_sceneRootName, TYPE_STRING, GetSceneRootName)
        FN_0(fn_fileClumpName, TYPE_STRING, GetFileClumpName)
        FN_1(fn_requiredInstances, TYPE_INT, RequiredInstances, TYPE_STRING)
        FN_3(fn_buildSkeletonN, TYPE_INT, BuildSkeletonN,
             TYPE_INT, TYPE_FLOAT, TYPE_INT)
        FN_5(fn_buildMeshesN, TYPE_INT, BuildMeshesN,
             TYPE_INT, TYPE_INT, TYPE_INT, TYPE_FLOAT, TYPE_INT)
        FN_1(fn_setQuatMode,  TYPE_INT,    SetQuatMode,   TYPE_INT)
        FN_1(fn_setBoneSize,  TYPE_INT,    SetBoneSize,   TYPE_FLOAT)
        FN_0(fn_sceneBoneCount, TYPE_INT,    SceneBoneCount)
        FN_0(fn_sceneClumpName, TYPE_STRING, GetSceneClumpName)
        FN_0(fn_clearScene,     TYPE_INT,    ClearScene)
        FN_0(fn_pruneScene,     TYPE_INT,    PruneScene)
        FN_0(fn_pendingFolder,  TYPE_STRING, TakePendingFolder)
        FN_0(fn_sceneReport,    TYPE_STRING, GetSceneReport)
        FN_0(fn_layerReport,    TYPE_STRING, GetLayerReport)
        FN_3(fn_buildMaterialAnim, TYPE_INT, BuildMaterialAnim,
             TYPE_INT, TYPE_FLOAT, TYPE_FLOAT)
        FN_0(fn_parseTextures,   TYPE_INT,    ParseTextures)
        FN_0(fn_textureCount,    TYPE_INT,    TextureCount)
        FN_0(fn_materialCount,   TYPE_INT,    MaterialCount)
        FN_0(fn_textureSummary,  TYPE_STRING, GetTextureSummary)
        FN_1(fn_exportTextures,  TYPE_INT,    ExportTextures,  TYPE_STRING)
        FN_1(fn_buildMaterials,  TYPE_INT,    BuildMaterials,  TYPE_STRING)
        FN_0(fn_showDialog,      TYPE_INT,    ShowDialog)
        FN_0(fn_showAnimDialog,  TYPE_INT,    ShowAnimDialog)
        FN_4(fn_importCharacter, TYPE_STRING, ImportCharacter, TYPE_STRING, TYPE_STRING, TYPE_INT, TYPE_INT)
        FN_2(fn_applyAnimation,  TYPE_STRING, ApplyAnimation,  TYPE_INT, TYPE_INT)
        FN_2(fn_applySequence,   TYPE_STRING, ApplySequence,   TYPE_INT, TYPE_INT)
    END_FUNCTION_MAP

    // --- Von MaxScript aufrufbar ---
    int Open(const MCHAR* path);
    int Close();
    int IsOpen();
    int Dump(const MCHAR* outPath, int includeTables);

    int PageCount();
    int ChunkCount();
    int CountOfType(const MCHAR* typeName);
    const MCHAR* NamesOfType(const MCHAR* typeName);
    const MCHAR* GetSummary();

    const MCHAR* GetVersion();
    const MCHAR* GetLastError();
    const MCHAR* GetWarnings();
    const MCHAR* GetLog();
    const MCHAR* GetTimings();
    int SetDebug(int on);

    int ParseSkeleton();
    int ClumpCount();
    int BoneCount();
    const MCHAR* GetBoneSummary();
    int BoneDump(const MCHAR* outPath);
    int BuildSkeleton(int mode, float scale);

    int ParseMeshes();
    int ModelCount();
    const MCHAR* GetMeshSummary();
    int MeshDump(const MCHAR* outPath, int withVertices);
    int BuildMeshes(int skipLod, int explicitNormals, float scale);
    int BuildMeshesSkinned(int skipLod, int explicitNormals, int applySkin,
                           float scale);

    int ParseAnims();
    int ParseAnimsAppend();
    int ClearAnims();
    int AnimCount();
    const MCHAR* GetAnimName(int index);
    const MCHAR* GetAnimSummary();
    int AnimDump(const MCHAR* outPath, int withKeys);
    int BuildAnim(int index, float scale);
    int BuildAnimEx(int index, int channelMask, float scale);
    int BuildAnimAt(int index, float startFrame, int channelMask, float scale);
    float AnimFrames(int index);
    int AnimHasBones(int index);
    int BuildBindPoseKey(float frame);
    int BuildIdleKeys(int index, float startFrame, float endFrame);
    int BuildVisibility(int index, float startFrame, float endFrame);
    const MCHAR* GetSceneRootName();

    const MCHAR* GetFileClumpName();
    int RequiredInstances(const MCHAR* clumpName);

    // Rohfassung auf dem cp932-Namen. buildSkeletonN bestimmt die
    // Zahl damit je Clump selbst, statt sie von aussen zu bekommen.
    int RequiredInstancesRaw(const std::string& clumpName);
    int BuildSkeletonN(int mode, float scale, int copies);
    int BuildMeshesN(int skipLod, int explicitNormals, int applySkin,
                     float scale, int copies);
    int SetQuatMode(int mode);
    int SetBoneSize(float size);

    int SceneBoneCount();
    const MCHAR* GetSceneClumpName();
    int ClearScene();

    // Eintraege wegwerfen, deren Knoten oder Material es nicht
    // mehr gibt - nach einem Loeschen in der Szene.
    int PruneScene();

    // Ordner, den der Import-Dialog von Max uebergeben hat.
    // Wird beim Abholen geleert - er gilt fuer genau einen
    // Fensteraufbau.
    const MCHAR* TakePendingFolder();
    void SetPendingFolder(const std::wstring& dir);

    // Oeffnet die Oberflaeche - vom Import-Dialog aus gerufen.
    void RunMacro();

    // Beliebigen MAXScript-Text ausfuehren. Wird beim Start
    // gebraucht, um die eingebettete Oberflaeche einzulesen.
    void RunScriptText(const std::wstring& script);
    const MCHAR* GetSceneReport();

    // Zuordnung Knoten -> Clump als Text, fuer den Layerbau in
    // MaxScript. Eine Zeile je Knoten:
    //   <clump> TAB <instanz> TAB bone|mesh TAB <handle>
    const MCHAR* GetLayerReport();
    int BuildMaterialAnim(int index, float startFrame, float endFrame);

    int ParseTextures();
    int TextureCount();
    int MaterialCount();
    const MCHAR* GetTextureSummary();
    int ExportTextures(const MCHAR* directory);
    int BuildMaterials(const MCHAR* directory);

    // --- 3.0: die C++-Fenster, und dieselben Ablaeufe fuer Tests ---
    int ShowDialog();
    int ShowAnimDialog();
    // folder "" = zuletzt benutztes Spiel. flags: 1 Texturen,
    // 2 geteilte Reaktionen, 4 Szene leeren, 8 WC3-Modus,
    // 16 ohne Effekte. Liefert den Bericht,
    // bei Fehlern mit "ERROR: " davor.
    const MCHAR* ImportCharacter(const MCHAR* folder, const MCHAR* code, int costume, int flags);
    const MCHAR* ApplyAnimation(int index, int wc3);
    const MCHAR* ApplySequence(int gap, int wc3);

    // ------------------------------------------------------
    //  Fuer die C++-Fenster (nicht aus MaxScript)
    //
    //  Die Fenster lesen die Dateien direkt aus den Spiel-
    //  archiven - es gibt keinen Pfad, nur einen Puffer.
    // ------------------------------------------------------
    int OpenBuffer(const std::vector<uint8_t>& data, const std::wstring& label);

    // Animationen ab Index 'from' auf einen anderen Clump legen:
    // Clump-Namen fromClump -> toClump, Bone-Namen mit dem Anfang
    // fromPrefix -> toPrefix. Fuer die geteilten Reaktionen
    // ("1cmnbod1" -> "1nrtbod1", "1cmn00t0 pelvis" -> "1nrt00t0 pelvis").
    void RetargetAnims(size_t from, const std::string& fromClump, const std::string& toClump,
                       const std::string& fromPrefix, const std::string& toPrefix);

    // Animationen ab 'from' verwerfen, die keinen Bone von 'clump'
    // bewegen - etwa die Opferanimation in einer Jutsu-Datei, die
    // nur den Gegner anspricht. Liefert die Zahl der behaltenen.
    size_t KeepAnimsFor(size_t from, const std::string& clump);

    // Gruppe fuer die Anzeige ("Basic", "Jutsu" ...), je Animation.
    void TagAnims(size_t from, const std::string& tag);

    const std::vector<xfbin::Anm>&  Anims()    const { return anims_; }
    const std::vector<std::string>& AnimTags() const { return animTags_; }

    // WC3 "auf der Stelle": die Wurzelknochen behalten ihre
    // waagerechte Ruhelage (X/Y), die Hoehe bleibt animiert. Nur
    // fuer das erste Exemplar von 'clump' (die Figur selbst) -
    // Effekte und Doppelgaenger fliegen weiter.
    void SetInPlace(bool on, const std::string& clump = std::string()) { inPlace_ = on; inPlaceClump_ = clump; }

    // WC3: beim Mesh-Aufbau ein Objekt je Material (siehe BuildMeshesN).
    void SetSplitByMaterial(bool on) { splitByMaterial_ = on; }

    // Nur diese Clumps bauen (leer = alle). Sammeldateien wie
    // data/effect/1efcmn.xfbin enthalten ueber 300 Effekte; eine
    // Figur braucht davon eine Handvoll.
    void SetClumpFilter(const std::vector<std::string>& names) { clumpFilter_.clear(); clumpFilter_.insert(names.begin(), names.end()); }
    bool ClumpWanted(const std::string& name) const { return clumpFilter_.empty() || clumpFilter_.count(name) > 0; }

    // Clump-Namen, die die Animationen ab 'from' ansprechen.
    std::vector<std::string> AnimClumpNames(size_t from) const;

    // Meshes dieser Clumps mit dem Knoten ihres Mesh-Bones:
    // {Mesh-Handle, Bone-Handle, Clump-Name}. Fuer den WC3-Durchlauf
    // der Effekte - ihr Ein- und Ausblenden steckt in Deckkraft und
    // Skalierung dieses Bones.
    struct MeshBone { ULONG mesh = 0; ULONG bone = 0; std::string clump; };
    std::vector<MeshBone> MeshBones(const std::set<std::string>& clumps) const;

    // Sichtbarkeit eines Knotens durch diese Step-Keys ersetzen
    // (Zeit in Ticks, Wert 0 oder 1).
    void SetStepVisibility(ULONG handle, const std::vector<std::pair<TimeValue, float>>& keys);

    // Alle Keys der importierten Bones und Meshes loeschen
    // (Position, Rotation, Skalierung, Sichtbarkeit) - bevor eine
    // neue Animation oder Sequenz kommt.
    int ClearAnimKeys();

private:
    // --- Persistenter State zwischen den Aufrufen ---
    std::unique_ptr<xfbin::XfbinFile> file_;
    std::wstring loadedPath_;

    // Stufe 1. Bewusst getrennt vom Container: parseSkeleton()
    // laesst sich damit wiederholen, ohne die Datei neu zu lesen.
    std::vector<xfbin::Clump> clumps_;
    bool skeletonParsed_ = false;

    // ------------------------------------------------------
    //  Szenenzustand
    //
    //  Getrennt von clumps_, und das ist der Kern: clumps_
    //  beschreibt die GERADE GEOEFFNETE DATEI, sceneClumps_ das,
    //  was tatsaechlich in der Szene steht.
    //
    //  Bis 0.5.0 war das dasselbe - mit der Folge, dass das
    //  Oeffnen der Animationsdatei das Skelett "vergass",
    //  obwohl die 222 Bones weiter in der Szene standen.
    //  Genau der uebliche Ablauf (Modell laden, Bones bauen,
    //  Animationsdatei laden) war damit unmoeglich.
    //
    //  Handles statt INode*, weil sie stabil bleiben und sich
    //  in O(1) aufloesen lassen - dieselbe Ueberlegung wie in
    //  AnimMerge.
    // ------------------------------------------------------
    std::vector<xfbin::Clump>      sceneClumps_;
    std::vector<std::vector<ULONG>> boneHandles_;

    // Laufende Nummer je Clump-NAME. Ein Charakter kann dasselbe
    // Modell mehrfach tragen: der Anim-Container nennt "1haksbn1"
    // zweimal, mit verschiedenen Positionen - zwei gleiche
    // Waffen. Ueber den Namen allein liesse sich nicht
    // unterscheiden, welche Instanz gemeint ist.
    std::vector<int> sceneInstance_;

    std::vector<xfbin::NudModel> models_;
    bool meshesParsed_ = false;

    std::vector<xfbin::Anm> anims_;
    std::vector<std::string> animTags_;   // parallel zu anims_
    bool animsParsed_ = false;
    bool inPlace_ = false;
    bool splitByMaterial_ = false;
    std::string inPlaceClump_;
    std::set<std::string> clumpFilter_;

    // Gemeinsamer Teil von Open und OpenBuffer.
    int AdoptFile(std::unique_ptr<xfbin::XfbinFile> parsed, const xfbin::ReadResult& r,
                  const std::wstring& label);

    std::vector<xfbin::XfbinTexture>  textures_;
    std::vector<xfbin::XfbinMaterial> materials_;
    std::vector<std::string>          textureFiles_;   // parallel zu textures_
    bool texturesParsed_ = false;

    // Knoten, die buildMeshes angelegt hat, mit dem Index ihres
    // Modells in models_. buildMaterials braucht beides: den
    // Knoten zum Zuweisen und das Modell fuer die Material-IDs.
    struct MeshRef { ULONG handle = 0; size_t modelIndex = 0; int subIndex = -1; };   // subIndex >= 0: nur dieses Material
    std::vector<MeshRef> meshNodes_;

    // Alle jemals angelegten Mesh-Objekte, ueber Dateigrenzen
    // hinweg - mit dem Clump und dem Mesh-Bone, zu dem sie
    // gehoeren. meshNodes_ wird bei jedem buildMeshes geleert
    // und kennt nur die zuletzt geladene Datei; fuer die
    // Sichtbarkeit braucht es den ganzen Bestand.
    struct SceneMesh {
        ULONG              handle = 0;
        xfbin::RawString   clumpName;
        xfbin::RawString   boneName;
        int                instance = 0;
    };
    std::vector<SceneMesh> sceneMeshes_;

    // ------------------------------------------------------
    //  XFBIN-Materialname -> das daraus gebaute Max-Material
    //
    //  Gespeichert wird ein AnimHandle, KEIN Mtl*.
    //
    //  Bis 1.9.2 stand hier der rohe Zeiger. Nach einem
    //  "delete objects" und einem zweiten Import zeigte er ins
    //  Leere, und der naechste Zugriff war ein Absturz mit
    //  Leseadresse 0. Genau davor warnt die SDK-Dokumentation:
    //  AnimHandles sind keine Zeiger, sie bleiben gueltig, wenn
    //  das Objekt geloescht wird, und GetAnimByHandle liefert
    //  dann schlicht NULL.
    //
    //  Damit wird aus einem Absturz eine Fallunterscheidung:
    //  Material noch da -> wiederverwenden, sonst neu bauen.
    // ------------------------------------------------------
    std::map<std::string, AnimHandle> sceneMaterials_;

    // 1 = Quaternionmatrix transponiert aufbauen. Notausgang,
    // falls sich die Herleitung in MakeRotationFromQuat an echten
    // Daten nicht bestaetigt.
    int quatMode_ = 0;

    // Vom Import-Dialog uebergeben, bis die Oberflaeche ihn abholt.
    std::wstring pendingFolder_;

    // Breite und Hoehe der Bone-Objekte, beim Anlegen gesetzt.
    // Negativ = Max' Standardwert stehen lassen.
    float boneSize_ = 0.0f;

    // --- Diagnose ---
    std::wstring lastError_;
    std::wstring warnings_;
    std::wstring log_;
    std::wstring scratch_;        // Puffer fuer TYPE_STRING-Rueckgaben.
                                  // MaxScript kopiert den String sofort,
                                  // aber der Zeiger muss bis dahin gueltig
                                  // bleiben - deshalb ein Member, keine
                                  // lokale Variable.
    bool debug_ = false;

    double msRead_  = 0.0;
    double msDump_  = 0.0;
    double msBones_  = 0.0;
    double msBuild_  = 0.0;
    double msMeshes_ = 0.0;
    double msGeom_   = 0.0;
    double msSkin_   = 0.0;
    double msTex_    = 0.0;
    double msAnims_  = 0.0;
    double msKeys_   = 0.0;

    void Log(const std::wstring& msg);
    void SetError(const std::wstring& msg);
    void AddWarning(const std::wstring& msg);
    void ResetDiagnostics();

    // Prueft, ob eine Datei geladen ist, und setzt sonst lastError_.
    bool RequireFile(const wchar_t* what);

    // Wie RequireFile, zusaetzlich fuer das ausgewertete Skelett.
    // Wertet es bei Bedarf selbst aus - so muss von MaxScript aus
    // niemand daran denken, parseSkeleton() vorher aufzurufen.
    bool RequireSkeleton(const wchar_t* what);
    bool RequireMeshes(const wchar_t* what);
    bool RequireAnims(const wchar_t* what);
    bool RequireTextures(const wchar_t* what);

    // Eintrag einer Animation auf Szenen-Clump und Bone abbilden.
    bool ResolveEntryTarget(const xfbin::Anm& anm,
                            const xfbin::AnmEntry& entry,
                            size_t& clumpSlot, size_t& boneSlot);

    // Skin-Modifier auf einen fertig gebauten Knoten setzen.
    // Getrennt von BuildMeshesSkinned, weil dort sonst eine
    // Funktion mit vier Aufgaben entstuende.
    bool ApplySkin(Interface* ip, INode* node, size_t clumpSlot,
                   const std::vector<std::array<uint32_t, 4>>& vertBoneIds,
                   const std::vector<std::array<float, 4>>& vertWeights,
                   std::wstring& why);
};

// ============================================================
//  UtilityObj - Eintrag im Utilities-Panel
//
//  Traegt keine eigene Oberflaeche. Der Eintrag existiert, damit
//  die DLL eine regulaere Plugin-Klasse anmeldet; die eigentliche
//  Bedienung laeuft ueber XfbinCpp.* aus MaxScript.
// ============================================================

class XfbinImportPlugin : public UtilityObj {
public:
    void BeginEditParams(Interface* ip, IUtil* iu) override;
    void EndEditParams(Interface* ip, IUtil* iu) override;
    void DeleteThis() override {}

private:
    Interface* ip_ = nullptr;
    IUtil*     iu_ = nullptr;
};
