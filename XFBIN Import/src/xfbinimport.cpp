// ============================================================
//  XFBIN Import Plugin - Implementierung
//  Version 0.1.0
//
//  Siehe xfbinimport.h fuer die MaxScript-API.
// ============================================================

#include "xfbinimport.h"

#include <iparamb2.h>
#include <maxscript/maxscript.h>
#include <object.h>
#include <istdplug.h>
#include <triobj.h>
#include <MeshNormalSpec.h>
#include <iskin.h>
#include <stdmat.h>
#include <imtl.h>
#include <modstack.h>
#include <control.h>

// ------------------------------------------------------------
//  Class-IDs der beiden Knotentypen.
//
//  Beide sind Standard-Defines des SDK, aber je nach Version in
//  unterschiedlichen Headern. Die Rueckfallebenen unten sind die
//  offiziellen Werte aus plugapi.h - so bricht der Build nicht,
//  wenn ein SDK den Header nicht mitzieht.
// ------------------------------------------------------------
#ifndef BONE_OBJ_CLASS_ID
#define BONE_OBJ_CLASS_ID 0x28
#endif
#ifndef BONE_OBJ_CLASSID
#define BONE_OBJ_CLASSID Class_ID(BONE_OBJ_CLASS_ID, 0)
#endif
#ifndef POINTHELP_CLASS_ID
#define POINTHELP_CLASS_ID 0x1230
#endif

#include <tchar.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <locale>
#include <map>
#include <set>
#include <utility>
#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

// ============================================================
//  Statische Interface-Instanz (registriert XfbinCpp.xxx)
// ============================================================

static XfbinImportInterface theXfbinImportInterface(
    XFBINIMPORT_INTERFACE_ID,
    _T("XfbinCpp"), 0, NULL, FP_CORE,

    fn_open, _T("open"), 0, TYPE_INT, 0, 1,
        _T("path"), 0, TYPE_STRING,
    fn_close, _T("close"), 0, TYPE_INT, 0, 0,
    fn_isOpen, _T("isOpen"), 0, TYPE_INT, 0, 0,
    fn_dump, _T("dump"), 0, TYPE_INT, 0, 2,
        _T("outPath"), 0, TYPE_STRING,
        _T("includeTables"), 0, TYPE_INT,

    fn_pageCount, _T("pageCount"), 0, TYPE_INT, 0, 0,
    fn_chunkCount, _T("chunkCount"), 0, TYPE_INT, 0, 0,
    fn_countOfType, _T("countOfType"), 0, TYPE_INT, 0, 1,
        _T("typeName"), 0, TYPE_STRING,
    fn_namesOfType, _T("namesOfType"), 0, TYPE_STRING, 0, 1,
        _T("typeName"), 0, TYPE_STRING,
    fn_summary, _T("summary"), 0, TYPE_STRING, 0, 0,

    fn_version, _T("version"), 0, TYPE_STRING, 0, 0,
    fn_lastError, _T("lastError"), 0, TYPE_STRING, 0, 0,
    fn_warnings, _T("warnings"), 0, TYPE_STRING, 0, 0,
    fn_log, _T("log"), 0, TYPE_STRING, 0, 0,
    fn_timings, _T("timings"), 0, TYPE_STRING, 0, 0,
    fn_setDebug, _T("setDebug"), 0, TYPE_INT, 0, 1,
        _T("on"), 0, TYPE_INT,

    // --- Stufe 1: Skelett ---
    fn_parseSkeleton, _T("parseSkeleton"), 0, TYPE_INT, 0, 0,
    fn_clumpCount, _T("clumpCount"), 0, TYPE_INT, 0, 0,
    fn_boneCount, _T("boneCount"), 0, TYPE_INT, 0, 0,
    fn_boneSummary, _T("boneSummary"), 0, TYPE_STRING, 0, 0,
    fn_boneDump, _T("boneDump"), 0, TYPE_INT, 0, 1,
        _T("outPath"), 0, TYPE_STRING,
    fn_buildSkeleton, _T("buildSkeleton"), 0, TYPE_INT, 0, 2,
        _T("mode"), 0, TYPE_INT,
        _T("scale"), 0, TYPE_FLOAT,

    // --- Stufe 2: Meshes ---
    fn_parseMeshes, _T("parseMeshes"), 0, TYPE_INT, 0, 0,
    fn_modelCount, _T("modelCount"), 0, TYPE_INT, 0, 0,
    fn_meshSummary, _T("meshSummary"), 0, TYPE_STRING, 0, 0,
    fn_meshDump, _T("meshDump"), 0, TYPE_INT, 0, 2,
        _T("outPath"), 0, TYPE_STRING,
        _T("withVertices"), 0, TYPE_INT,
    fn_buildMeshes, _T("buildMeshes"), 0, TYPE_INT, 0, 3,
        _T("skipLod"), 0, TYPE_INT,
        _T("explicitNormals"), 0, TYPE_INT,
        _T("scale"), 0, TYPE_FLOAT,

    // --- Stufe 3: Skinning ---
    fn_buildMeshesSkinned, _T("buildMeshesSkinned"), 0, TYPE_INT, 0, 4,
        _T("skipLod"), 0, TYPE_INT,
        _T("explicitNormals"), 0, TYPE_INT,
        _T("applySkin"), 0, TYPE_INT,
        _T("scale"), 0, TYPE_FLOAT,

    // --- Stufe 5: Animationen ---
    fn_parseAnims, _T("parseAnims"), 0, TYPE_INT, 0, 0,
    fn_parseAnimsAppend, _T("parseAnimsAppend"), 0, TYPE_INT, 0, 0,
    fn_clearAnims, _T("clearAnims"), 0, TYPE_INT, 0, 0,
    fn_animCount, _T("animCount"), 0, TYPE_INT, 0, 0,
    fn_animName, _T("animName"), 0, TYPE_STRING, 0, 1,
        _T("index"), 0, TYPE_INT,
    fn_animSummary, _T("animSummary"), 0, TYPE_STRING, 0, 0,
    fn_animDump, _T("animDump"), 0, TYPE_INT, 0, 2,
        _T("outPath"), 0, TYPE_STRING,
        _T("withKeys"), 0, TYPE_INT,
    fn_buildAnim, _T("buildAnim"), 0, TYPE_INT, 0, 2,
        _T("index"), 0, TYPE_INT,
        _T("scale"), 0, TYPE_FLOAT,
    fn_buildAnimEx, _T("buildAnimEx"), 0, TYPE_INT, 0, 3,
        _T("index"), 0, TYPE_INT,
        _T("channelMask"), 0, TYPE_INT,
        _T("scale"), 0, TYPE_FLOAT,
    fn_setQuatMode, _T("setQuatMode"), 0, TYPE_INT, 0, 1,
        _T("mode"), 0, TYPE_INT,
    fn_setBoneSize, _T("setBoneSize"), 0, TYPE_INT, 0, 1,
        _T("size"), 0, TYPE_FLOAT,
    fn_buildAnimAt, _T("buildAnimAt"), 0, TYPE_INT, 0, 4,
        _T("index"), 0, TYPE_INT,
        _T("startFrame"), 0, TYPE_FLOAT,
        _T("channelMask"), 0, TYPE_INT,
        _T("scale"), 0, TYPE_FLOAT,
    fn_animFrames, _T("animFrames"), 0, TYPE_FLOAT, 0, 1,
        _T("index"), 0, TYPE_INT,
    fn_animIsSkeletal, _T("animIsSkeletal"), 0, TYPE_INT, 0, 1,
        _T("index"), 0, TYPE_INT,
    fn_buildBindPoseKey, _T("buildBindPoseKey"), 0, TYPE_INT, 0, 1,
        _T("frame"), 0, TYPE_FLOAT,
    fn_buildIdleKeys, _T("buildIdleKeys"), 0, TYPE_INT, 0, 3,
        _T("index"), 0, TYPE_INT,
        _T("startFrame"), 0, TYPE_FLOAT,
        _T("endFrame"), 0, TYPE_FLOAT,
    fn_buildVisibility, _T("buildVisibility"), 0, TYPE_INT, 0, 3,
        _T("index"), 0, TYPE_INT,
        _T("startFrame"), 0, TYPE_FLOAT,
        _T("endFrame"), 0, TYPE_FLOAT,
    fn_sceneRootName, _T("sceneRootName"), 0, TYPE_STRING, 0, 0,
    fn_fileClumpName, _T("fileClumpName"), 0, TYPE_STRING, 0, 0,
    fn_requiredInstances, _T("requiredInstances"), 0, TYPE_INT, 0, 1,
        _T("clumpName"), 0, TYPE_STRING,
    fn_buildSkeletonN, _T("buildSkeletonN"), 0, TYPE_INT, 0, 3,
        _T("mode"), 0, TYPE_INT,
        _T("scale"), 0, TYPE_FLOAT,
        _T("copies"), 0, TYPE_INT,
    fn_buildMeshesN, _T("buildMeshesN"), 0, TYPE_INT, 0, 5,
        _T("skipLod"), 0, TYPE_INT,
        _T("explicitNormals"), 0, TYPE_INT,
        _T("applySkin"), 0, TYPE_INT,
        _T("scale"), 0, TYPE_FLOAT,
        _T("copies"), 0, TYPE_INT,

    // --- Szenenzustand ---
    fn_sceneBoneCount, _T("sceneBoneCount"), 0, TYPE_INT, 0, 0,
    fn_sceneClumpName, _T("sceneClumpName"), 0, TYPE_STRING, 0, 0,
    fn_clearScene, _T("clearScene"), 0, TYPE_INT, 0, 0,
    fn_sceneReport, _T("sceneReport"), 0, TYPE_STRING, 0, 0,
    fn_layerReport, _T("layerReport"), 0, TYPE_STRING, 0, 0,
    fn_buildMaterialAnim, _T("buildMaterialAnim"), 0, TYPE_INT, 0, 3,
        _T("index"), 0, TYPE_INT,
        _T("startFrame"), 0, TYPE_FLOAT,
        _T("endFrame"), 0, TYPE_FLOAT,

    // --- Stufe 4: Texturen und Materialien ---
    fn_parseTextures, _T("parseTextures"), 0, TYPE_INT, 0, 0,
    fn_textureCount, _T("textureCount"), 0, TYPE_INT, 0, 0,
    fn_materialCount, _T("materialCount"), 0, TYPE_INT, 0, 0,
    fn_textureSummary, _T("textureSummary"), 0, TYPE_STRING, 0, 0,
    fn_exportTextures, _T("exportTextures"), 0, TYPE_INT, 0, 1,
        _T("directory"), 0, TYPE_STRING,
    fn_buildMaterials, _T("buildMaterials"), 0, TYPE_INT, 0, 1,
        _T("directory"), 0, TYPE_STRING,

    p_end
);

// ============================================================
//  Helfer
// ============================================================

namespace {

class Stopwatch {
public:
    Stopwatch() : start_(std::chrono::steady_clock::now()) {}
    double ms() const {
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - start_).count();
    }
private:
    std::chrono::steady_clock::time_point start_;
};

// ------------------------------------------------------------
//  MAXScript aus C++ aufrufen
//
//  Die Signatur hat sich mit Max 2022 geaendert. Bis 2021:
//
//      BOOL ExecuteMAXScriptScript(MCHAR* s, BOOL quietErrors,
//                                  FPValue* fpv)
//
//  Ab 2022 steht an zweiter Stelle ein MAXScript::ScriptSource -
//  ein Namensraum, den es vorher gar nicht gab. Deshalb hier eine
//  Weiche statt eines Aufrufs.
//
//  MAX_RELEASE kommt aus plugapi.h und ist damit immer die
//  Wahrheit ueber das SDK, gegen das gerade uebersetzt wird:
//  2016 = 18000, 2022 = 24000, je Jahrgang 1000 mehr.
// ------------------------------------------------------------
#if defined(MAX_RELEASE) && (MAX_RELEASE >= 24000)
bool RunScript(const std::wstring& script) {
    return ExecuteMAXScriptScript(script.c_str(),
                                  MAXScript::ScriptSource::NonEmbedded,
                                  TRUE) != FALSE;
}
#else
bool RunScript(const std::wstring& script) {
    return ExecuteMAXScriptScript(script.c_str(), TRUE) != FALSE;
}
#endif

// Anfuehrungszeichen und Backslashes fuer ein MaxScript-Stringliteral
// entschaerfen. Ohne das reisst ein Dateipfad wie D:\x\y das Literal auf.
std::wstring EscapeMaxScriptString(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 8);
    for (wchar_t c : s) {
        if (c == L'"' || c == L'\\') out.push_back(L'\\');
        if (c == L'\n') { out += L"\\n"; continue; }
        if (c == L'\r') { out += L"\\r"; continue; }
        out.push_back(c);
    }
    return out;
}

// ------------------------------------------------------------
//  cp932 (Shift-JIS) -> UTF-16.
//
//  XFBIN-Strings sind cp932. Ein simples "Byte fuer Byte nach
//  wchar_t" wuerde japanische Namen zerlegen, weil cp932 ein
//  Multibyte-Encoding ist. Deshalb ueber MultiByteToWideChar mit
//  Codepage 932.
//
//  Schlaegt die Umwandlung fehl (kaputte Bytes), wird auf die
//  ANSI-Codepage zurueckgefallen statt zu scheitern - fuer die
//  Diagnose ist ein leicht falscher Name besser als gar keiner.
// ------------------------------------------------------------
std::wstring Cp932ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();

    for (UINT cp : { UINT(932), UINT(CP_ACP) }) {
        const int n = MultiByteToWideChar(cp, 0, s.c_str(),
                                          static_cast<int>(s.size()),
                                          nullptr, 0);
        if (n <= 0) continue;
        std::wstring w(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(cp, 0, s.c_str(), static_cast<int>(s.size()),
                            &w[0], n);
        return w;
    }
    return std::wstring(s.begin(), s.end());
}

// UTF-16 -> UTF-8. Fuer Pfade, die an den SDK-freien Parser gehen:
// dessen std::ifstream nimmt schmale Pfade. Unter Windows braucht
// ein Nicht-ANSI-Pfad damit UTF-8 im Manifest - siehe README.
std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                                      static_cast<int>(w.size()),
                                      nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                        &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                      static_cast<int>(s.size()),
                                      nullptr, 0);
    if (n <= 0) return std::wstring(s.begin(), s.end());
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                        &w[0], n);
    return w;
}

std::wstring SafeStr(const MCHAR* s) {
    return s ? std::wstring(s) : std::wstring();
}

} // namespace

// ============================================================
//  Diagnose
// ============================================================

void XfbinImportInterface::Log(const std::wstring& msg) {
    log_ += msg;
    log_ += L"\n";
    if (log_.size() > 200000) {              // Protokoll begrenzen
        log_.erase(0, log_.size() - 150000);
    }
    if (debug_) {
        RunScript(L"format \"[XfbinImport] %\\n\" \"" +
                  EscapeMaxScriptString(msg) + L"\"");
    }
}

void XfbinImportInterface::SetError(const std::wstring& msg) {
    lastError_ = msg;
    Log(L"FEHLER: " + msg);
}

// Nicht-fatale Hinweise. Getrennt von lastError_, damit lastError()
// eindeutig bleibt: "" heisst alles gut.
void XfbinImportInterface::AddWarning(const std::wstring& msg) {
    if (msg.empty()) return;
    if (warnings_.find(msg) != std::wstring::npos) return;   // nicht doppelt
    if (warnings_.size() > 32000) warnings_.clear();
    if (!warnings_.empty()) warnings_ += L"\n";
    warnings_ += msg;
    Log(L"WARNUNG: " + msg);
}

void XfbinImportInterface::ResetDiagnostics() {
    lastError_.clear();
    warnings_.clear();
    log_.clear();
    msRead_ = msDump_ = msBones_ = msBuild_ = 0.0;
    msMeshes_ = msGeom_ = msSkin_ = msAnims_ = msKeys_ = msTex_ = 0.0;
}

bool XfbinImportInterface::RequireFile(const wchar_t* what) {
    if (file_) return true;
    SetError(std::wstring(what) + L": es ist keine Datei geladen. "
             L"Zuerst XfbinCpp.open <path> aufrufen.");
    return false;
}

// ============================================================
//  Oeffnen / Schliessen
// ============================================================

int XfbinImportInterface::Open(const MCHAR* path) {
    ResetDiagnostics();

    const std::wstring wpath = SafeStr(path);
    if (wpath.empty()) {
        SetError(L"open: leerer Pfad.");
        return -1;
    }

    Log(L"open: " + wpath);

    auto parsed = std::make_unique<xfbin::XfbinFile>();

    Stopwatch sw;
    const xfbin::ReadResult r =
        xfbin::ReadXfbinFile(WideToUtf8(wpath), *parsed);
    msRead_ = sw.ms();

    if (!r.ok) {
        SetError(Utf8ToWide(r.error));
        file_.reset();
        loadedPath_.clear();
        return -1;
    }

    if (!r.warnings.empty()) {
        // Der Parser sammelt mehrzeilig; jede Zeile einzeln melden,
        // damit die Doppel-Unterdrueckung in AddWarning greift.
        std::istringstream in(r.warnings);
        std::string line;
        while (std::getline(in, line)) AddWarning(Utf8ToWide(line));
    }

    file_           = std::move(parsed);
    loadedPath_     = wpath;
    // Nur der Dateizustand wird zurueckgesetzt. sceneClumps_ und
    // boneHandles_ beschreiben die Szene und bleiben erhalten -
    // sonst waere nach dem Oeffnen der Animationsdatei das
    // Skelett "weg", obwohl es im Viewport steht.
    clumps_.clear();
    skeletonParsed_ = false;
    models_.clear();
    meshesParsed_   = false;
    // anims_ wird NICHT geleert: ein Charakter kann mehrere
    // Animationsdateien haben (bei Pein sind es vier mit
    // zusammen 104 Animationen), und die werden nacheinander
    // geoeffnet und angehaengt. Zum Leeren gibt es clearAnims().
    textures_.clear();
    materials_.clear();
    textureFiles_.clear();
    texturesParsed_ = false;

    const int chunks = static_cast<int>(file_->TotalChunks());

    wchar_t buf[256];
    swprintf_s(buf, L"gelesen in %.0f ms: %zu Pages, %d Chunks",
               msRead_, file_->pages.size(), chunks);
    Log(buf);

    return chunks;
}

int XfbinImportInterface::Close() {
    file_.reset();
    loadedPath_.clear();
    // Der Szenenzustand (sceneClumps_, boneHandles_) bleibt
    // absichtlich stehen: die Knoten sind ja weiter da.
    // Aufgeraeumt wird er nur ueber clearScene() oder beim
    // naechsten buildSkeleton.
    clumps_.clear();
    skeletonParsed_ = false;
    models_.clear();
    meshesParsed_ = false;
    anims_.clear();
    animsParsed_ = false;
    textures_.clear();
    materials_.clear();
    textureFiles_.clear();
    texturesParsed_ = false;
    Log(L"close");
    return 1;
}

int XfbinImportInterface::IsOpen() {
    return file_ ? 1 : 0;
}

// ============================================================
//  Abfragen
// ============================================================

int XfbinImportInterface::PageCount() {
    if (!RequireFile(L"pageCount")) return -1;
    return static_cast<int>(file_->pages.size());
}

int XfbinImportInterface::ChunkCount() {
    if (!RequireFile(L"chunkCount")) return -1;
    return static_cast<int>(file_->TotalChunks());
}

int XfbinImportInterface::CountOfType(const MCHAR* typeName) {
    if (!RequireFile(L"countOfType")) return -1;
    const std::string t = WideToUtf8(SafeStr(typeName));
    return static_cast<int>(file_->CountOfType(t.c_str()));
}

const MCHAR* XfbinImportInterface::NamesOfType(const MCHAR* typeName) {
    scratch_.clear();
    if (!RequireFile(L"namesOfType")) return scratch_.c_str();

    const std::string want = WideToUtf8(SafeStr(typeName));

    for (const xfbin::XfbinPage& p : file_->pages) {
        for (const xfbin::XfbinChunk& c : p.chunks) {
            if (!c.type || *c.type != want) continue;
            if (!scratch_.empty()) scratch_ += L"\n";
            scratch_ += Cp932ToWide(c.name ? *c.name : std::string());
        }
    }
    return scratch_.c_str();
}

const MCHAR* XfbinImportInterface::GetSummary() {
    scratch_.clear();
    if (!RequireFile(L"summary")) return scratch_.c_str();

    scratch_ = L"Datei: " + loadedPath_ + L"\n";
    // MakeSummary liefert nur ASCII (Typnamen und Zahlen), deshalb
    // genuegt hier UTF-8 -> UTF-16.
    scratch_ += Utf8ToWide(xfbin::MakeSummary(*file_));
    return scratch_.c_str();
}

// ============================================================
//  Dump
// ============================================================

int XfbinImportInterface::Dump(const MCHAR* outPath, int includeTables) {
    if (!RequireFile(L"dump")) return 0;

    const std::wstring wout = SafeStr(outPath);
    if (wout.empty()) {
        SetError(L"dump: leerer Ausgabepfad.");
        return 0;
    }

    // std::ofstream mit wchar_t-Pfad ist eine MSVC-Erweiterung, aber
    // genau die brauchen wir hier: der Pfad kommt aus Max und kann
    // Zeichen enthalten, die die ANSI-Codepage nicht kennt.
    std::ofstream f(wout.c_str(), std::ios::binary);
    if (!f) {
        SetError(L"dump: Ausgabedatei nicht schreibbar: " + wout);
        return 0;
    }

    // Als Label nur der Dateiname, nicht der ganze Pfad - sonst
    // unterscheiden sich die Dumps zweier Rechner in Zeile 2.
    std::wstring label = loadedPath_;
    const size_t slash = label.find_last_of(L"/\\");
    if (slash != std::wstring::npos) label = label.substr(slash + 1);

    Stopwatch sw;
    xfbin::WriteDump(*file_, WideToUtf8(label), f, includeTables != 0);
    f.close();
    msDump_ = sw.ms();

    wchar_t buf[512];
    swprintf_s(buf, L"dump geschrieben in %.0f ms: %s", msDump_, wout.c_str());
    Log(buf);

    return 1;
}

// ============================================================
//  Diagnose-Getter
// ============================================================

const MCHAR* XfbinImportInterface::GetVersion() {
    return XFBINIMPORT_VERSION_STR;
}

const MCHAR* XfbinImportInterface::GetLastError() {
    scratch_ = lastError_;
    return scratch_.c_str();
}

const MCHAR* XfbinImportInterface::GetWarnings() {
    scratch_ = warnings_;
    return scratch_.c_str();
}

const MCHAR* XfbinImportInterface::GetLog() {
    scratch_ = log_;
    return scratch_.c_str();
}

const MCHAR* XfbinImportInterface::GetTimings() {
    wchar_t buf[420];
    swprintf_s(buf,
               L"read=%.1f  bones=%.1f  build=%.1f  meshes=%.1f  geom=%.1f  "
               L"skin=%.1f  anims=%.1f  keys=%.1f  dump=%.1f  gesamt=%.1f ms",
               msRead_, msBones_, msBuild_, msMeshes_, msGeom_, msSkin_,
               msAnims_, msKeys_, msDump_,
               msRead_ + msBones_ + msBuild_ + msMeshes_ + msGeom_ + msSkin_
               + msAnims_ + msKeys_ + msDump_);
    scratch_ = buf;
    return scratch_.c_str();
}

int XfbinImportInterface::SetDebug(int on) {
    debug_ = (on != 0);
    return debug_ ? 1 : 0;
}


// ============================================================
//  Stufe 1: Skelett
// ============================================================

bool XfbinImportInterface::RequireSkeleton(const wchar_t* what) {
    if (!RequireFile(what)) return false;

    if (skeletonParsed_) return true;

    // Nicht vom Aufrufer verlangen, dass er parseSkeleton() vorher
    // aufruft - das ist eine Fehlerquelle ohne Gegenwert.
    return (ParseSkeleton() >= 0);
}

int XfbinImportInterface::ParseSkeleton() {
    if (!RequireFile(L"parseSkeleton")) return -1;

    clumps_.clear();
    skeletonParsed_ = false;

    std::string err, warn;

    Stopwatch sw;
    const bool ok = xfbin::ParseClumps(*file_, clumps_, err, warn);
    msBones_ = sw.ms();

    if (!warn.empty()) {
        std::istringstream in(warn);
        std::string line;
        while (std::getline(in, line)) AddWarning(Utf8ToWide(line));
    }

    if (clumps_.empty()) {
        SetError(Utf8ToWide(err));
        return -1;
    }

    skeletonParsed_ = true;

    size_t bones = 0;
    for (const xfbin::Clump& c : clumps_) bones += c.nodes.size();

    wchar_t buf[256];
    swprintf_s(buf, L"parseSkeleton: %zu Clump(s), %zu Bones in %.1f ms%s",
               clumps_.size(), bones, msBones_,
               ok ? L"" : L" (mit Hinweisen)");
    Log(buf);

    return static_cast<int>(bones);
}

int XfbinImportInterface::ClumpCount() {
    if (!RequireSkeleton(L"clumpCount")) return -1;
    return static_cast<int>(clumps_.size());
}

int XfbinImportInterface::BoneCount() {
    if (!RequireSkeleton(L"boneCount")) return -1;
    size_t n = 0;
    for (const xfbin::Clump& c : clumps_) n += c.nodes.size();
    return static_cast<int>(n);
}

const MCHAR* XfbinImportInterface::GetBoneSummary() {
    scratch_.clear();
    if (!RequireSkeleton(L"boneSummary")) return scratch_.c_str();

    // MakeBoneSummary gibt Clump-Namen aus, und die sind cp932.
    scratch_ = Cp932ToWide(xfbin::MakeBoneSummary(clumps_));
    return scratch_.c_str();
}

int XfbinImportInterface::BoneDump(const MCHAR* outPath) {
    if (!RequireSkeleton(L"boneDump")) return 0;

    const std::wstring wout = SafeStr(outPath);
    if (wout.empty()) {
        SetError(L"boneDump: leerer Ausgabepfad.");
        return 0;
    }

    std::ofstream f(wout.c_str(), std::ios::binary);
    if (!f) {
        SetError(L"boneDump: Ausgabedatei nicht schreibbar: " + wout);
        return 0;
    }

    std::wstring label = loadedPath_;
    const size_t slash = label.find_last_of(L"/\\");
    if (slash != std::wstring::npos) label = label.substr(slash + 1);

    xfbin::WriteBoneDump(clumps_, WideToUtf8(label), f);
    f.close();

    Log(L"boneDump geschrieben: " + wout);
    return 1;
}

// ------------------------------------------------------------
//  Knoten in der Szene anlegen
// ------------------------------------------------------------
int XfbinImportInterface::BuildSkeleton(int mode, float scale) {
    return BuildSkeletonN(mode, scale, 1);
}

// ------------------------------------------------------------
//  copies: wie viele Exemplare des Skeletts angelegt werden.
//
//  Der Anim-Container nennt "1haksbn1" zweimal, mit
//  unterschiedlichen Positionen - der Charakter traegt zwei
//  gleiche Waffen. Beide brauchen ein eigenes Skelett, sonst
//  ueberschreibt die zweite Instanz die Keys der ersten.
// ------------------------------------------------------------
// copies <= 0 bedeutet: fuer JEDEN Clump der Datei selbst
// nachsehen, wie viele Exemplare die geladenen Animationen
// erwarten.
//
// Ein fester Wert je Datei reicht nicht. 2peaacc2 enthaelt vier
// Skelette, von denen eines dreimal gebraucht wird und die
// anderen je einmal - mit einem gemeinsamen Wert entstuenden
// zwangslaeufig zwei ungenutzte Kopien der uebrigen drei.
int XfbinImportInterface::BuildSkeletonN(int mode, float scale, int copies) {
    if (!RequireSkeleton(L"buildSkeleton")) return 0;

    Interface* ip = GetCOREInterface();
    if (ip == nullptr) {
        SetError(L"buildSkeleton: keine Core-Schnittstelle.");
        return 0;
    }

    if (scale <= 0.0f) scale = 1.0f;

    // Hier verdienen sich die beiden Guards ihr Geld: 222 Knoten
    // ergeben ohne sie 222 Undo-Records und 222 Viewport-Updates.
    SceneRedrawGuard redrawGuard(ip);
    HoldSuspendGuard holdGuard;

    Stopwatch sw;

    int created = 0;
    int failed  = 0;
    std::wstring firstFailure;

    // ------------------------------------------------------
    //  Szenenzustand ANHAENGEN, nicht ersetzen.
    //
    //  Ein Charakter besteht aus mehr als einer Datei: neben
    //  1hakbod1.xfbin gibt es 1hakacc1.xfbin mit der Waffe, und
    //  die Animationen sprechen beide Skelette an. Wuerde
    //  buildSkeleton hier ersetzen, waere nach dem Laden der
    //  Waffe das Figurenskelett vergessen - obwohl es in der
    //  Szene steht.
    //
    //  Ein bereits vorhandenes Skelett gleichen Namens wird
    //  ueberschrieben statt verdoppelt; sonst sammeln sich bei
    //  mehrfachem Import Karteileichen an.
    // ------------------------------------------------------
    // Fuer jede Kopie einen eigenen Platz im Szenenzustand, mit
    // laufender Nummer. Eine schon vorhandene Instanz derselben
    // Nummer wird ueberschrieben statt verdoppelt.
    for (const xfbin::Clump& c : clumps_) {
        const int n = (copies > 0) ? copies : RequiredInstancesRaw(c.name);

        for (int k = 0; k < n; ++k) {
            size_t slot = SIZE_MAX;
            for (size_t i = 0; i < sceneClumps_.size(); ++i) {
                if (sceneClumps_[i].name == c.name && sceneInstance_[i] == k) {
                    slot = i;
                    break;
                }
            }
            if (slot == SIZE_MAX) {
                sceneClumps_.push_back(c);
                sceneInstance_.push_back(k);
                boneHandles_.push_back({});
            } else {
                sceneClumps_[slot] = c;
                boneHandles_[slot].clear();
            }
        }
    }

    for (size_t ci = 0; ci < clumps_.size(); ++ci) {
      const xfbin::Clump& clump = clumps_[ci];
      const int nCopies = (copies > 0) ? copies : RequiredInstancesRaw(clump.name);

      for (int copy = 0; copy < nCopies; ++copy) {
        // Den Platz im Szenenzustand ueber Name UND laufende
        // Nummer finden - die Reihenfolge in clumps_ und in
        // sceneClumps_ ist nicht dieselbe, sobald mehrere
        // Dateien oder Instanzen im Spiel sind.
        size_t slot = SIZE_MAX;
        for (size_t i = 0; i < sceneClumps_.size(); ++i) {
            if (sceneClumps_[i].name == clump.name &&
                sceneInstance_[i] == copy) {
                slot = i;
                break;
            }
        }
        if (slot == SIZE_MAX) continue;

        // Ab der zweiten Instanz einen Zusatz an den Namen, damit
        // man sie im Szenenbaum auseinanderhalten kann.
        std::wstring suffix;
        if (copy > 0) suffix = L" #" + std::to_wstring(copy + 1);

        // Parallel zu clump.nodes: die angelegten Knoten.
        std::vector<INode*> made(clump.nodes.size(), nullptr);
        boneHandles_[slot].assign(clump.nodes.size(), 0);

        // depthFirst garantiert, dass der Elternteil vor seinen
        // Kindern drankommt.
        for (int idx : clump.depthFirst) {
            const xfbin::CoordNode& n = clump.nodes[static_cast<size_t>(idx)];

            Object* obj = nullptr;

            if (mode == 1) {
                obj = static_cast<Object*>(
                    ip->CreateInstance(GEOMOBJECT_CLASS_ID, BONE_OBJ_CLASSID));
            }

            // Standard und Rueckfallebene: Point-Helper. Der
            // braucht keine Parameterblock-Kenntnis, sieht mit
            // ShowBone wie ein Bone aus, und der Skin-Modifier
            // akzeptiert ihn spaeter genauso als Knochen.
            if (obj == nullptr) {
                obj = static_cast<Object*>(
                    ip->CreateInstance(HELPER_CLASS_ID,
                                       Class_ID(POINTHELP_CLASS_ID, 0)));
            }

            if (obj == nullptr) {
                ++failed;
                if (firstFailure.empty()) {
                    firstFailure = Cp932ToWide(n.name);
                }
                continue;
            }

            INode* node = ip->CreateObjectNode(obj);
            if (node == nullptr) {
                ++failed;
                if (firstFailure.empty()) {
                    firstFailure = Cp932ToWide(n.name);
                }
                continue;
            }

            const std::wstring wname = Cp932ToWide(n.name) + suffix;
            node->SetName(const_cast<MCHAR*>(wname.c_str()));

            // Mat43 ist zeilenweise genauso angeordnet wie Matrix3 -
            // Zeilen 0..2 Basis, Zeile 3 Translation. Deshalb hier
            // nur die Umwandlung double -> float, kein Umsortieren.
            Matrix3 tm(1);
            for (int r = 0; r < 3; ++r) {
                tm.SetRow(r, Point3(static_cast<float>(n.world.m[r][0]),
                                    static_cast<float>(n.world.m[r][1]),
                                    static_cast<float>(n.world.m[r][2])));
            }
            tm.SetRow(3, Point3(static_cast<float>(n.world.m[3][0]) * scale,
                                static_cast<float>(n.world.m[3][1]) * scale,
                                static_cast<float>(n.world.m[3][2]) * scale));

            node->SetNodeTM(0, tm);

            // Als Knochen markieren und die Verbindungslinie zum
            // Elternteil zeichnen lassen.
            node->ShowBone(1);
            node->SetBoneNodeOnOff(TRUE, 0);

            // ------------------------------------------------
            //  Breite und Hoehe des Bone-Objekts
            //
            //  Max legt sie mit 4 an. Bei 1.793 Bones wird
            //  daraus ein Teppich aus Kaesten, der das Modell
            //  verdeckt; 0 zeichnet blosse Linien.
            //
            //  Das stand bis 1.8.0 in einem MaxScript-Nachlauf
            //  und ist beim Umbau des Imports verlorengegangen,
            //  ohne dass es jemand gemerkt haette. Hier beim
            //  Anlegen kann das nicht passieren.
            //
            //  boneobj_width und boneobj_height sind die ersten
            //  beiden Eintraege des Parameterblocks - so steht
            //  die Aufzaehlung in der SDK-Referenz.
            // ------------------------------------------------
            if (mode == 1 && boneSize_ >= 0.0f) {
                Object* bobj = node->GetObjectRef();
                if (bobj != nullptr) bobj = bobj->FindBaseObject();

                if (bobj != nullptr) {
                    // GetParamBlockByID, nicht GetParamBlock(0):
                    //
                    // Object erbt von BaseObject ein
                    // parameterloses GetParamBlock(), das ein
                    // IParamArray* liefert - die alte
                    // Parameterblock-Fassung. Diese Ueberladung
                    // VERDECKT Animatable::GetParamBlock(int),
                    // also findet der Compiler die gewuenschte
                    // Fassung gar nicht mehr. Genau darueber
                    // stolpert man mit
                    //   "akzeptiert keine 1 Argumente" und
                    //   "IParamArray* kann nicht in
                    //    IParamBlock2* konvertiert werden".
                    //
                    // GetParamBlockByID kommt direkt von
                    // Animatable und wird nicht verdeckt.
                    IParamBlock2* bpb = bobj->GetParamBlockByID(0);

                    // Rueckfallebene: die verdeckte Fassung
                    // ausdruecklich qualifiziert aufrufen.
                    if (bpb == nullptr) {
                        bpb = bobj->Animatable::GetParamBlock(0);
                    }

                    if (bpb != nullptr) {
                        bpb->SetValue(0, 0, boneSize_);   // boneobj_width
                        bpb->SetValue(1, 0, boneSize_);   // boneobj_height
                    }
                }
            }
            node->SetRenderable(FALSE);

            made[static_cast<size_t>(idx)] = node;
            boneHandles_[slot][static_cast<size_t>(idx)] = node->GetHandle();

            if (n.parent >= 0) {
                INode* parentNode = made[static_cast<size_t>(n.parent)];
                if (parentNode != nullptr) {
                    // keepTM = 1: die eben gesetzte Weltmatrix bleibt
                    // stehen, Max rechnet die lokale daraus zurueck.
                    parentNode->AttachChild(node, 1);
                }
            }

            ++created;
        }

        // Doppelte Namen sind fuer Stufe 3 (Skin) ein echtes
        // Problem, deshalb hier schon melden statt es spaeter zu
        // suchen.
        std::unordered_set<std::wstring> seenNames;
        int dupes = 0;
        for (const xfbin::CoordNode& n : clump.nodes) {
            if (!seenNames.insert(Cp932ToWide(n.name)).second) ++dupes;
        }
        if (dupes > 0) {
            wchar_t wbuf[256];
            swprintf_s(wbuf, L"Clump '%s' hat %d doppelte Bone-Namen. "
                             L"Fuer den Skin-Modifier muss das eindeutig sein.",
                       Cp932ToWide(clump.name).c_str(), dupes);
            AddWarning(wbuf);
        }
      } // Ende der Kopien-Schleife
    }

    msBuild_ = sw.ms();

    if (failed > 0) {
        wchar_t wbuf[256];
        swprintf_s(wbuf, L"%d Knoten liessen sich nicht anlegen (erster: '%s').",
                   failed, firstFailure.c_str());
        AddWarning(wbuf);
    }

    wchar_t buf[256];
    swprintf_s(buf, L"buildSkeleton: %d Knoten in %.1f ms (Modus %d, Skalierung %.3f)",
               created, msBuild_, mode, static_cast<double>(scale));
    Log(buf);

    ip->RedrawViews(ip->GetTime());

    return created;
}


// ============================================================
//  Stufe 2: Meshes
// ============================================================

bool XfbinImportInterface::RequireMeshes(const wchar_t* what) {
    if (!RequireFile(what)) return false;
    if (meshesParsed_) return true;
    return (ParseMeshes() >= 0);
}

int XfbinImportInterface::ParseMeshes() {
    if (!RequireFile(L"parseMeshes")) return -1;

    models_.clear();
    meshesParsed_ = false;

    std::string err, warn;

    Stopwatch sw;
    const bool ok = xfbin::ParseModels(*file_, models_, err, warn);
    msMeshes_ = sw.ms();

    if (!warn.empty()) {
        std::istringstream in(warn);
        std::string line;
        while (std::getline(in, line)) AddWarning(Utf8ToWide(line));
    }

    if (models_.empty()) {
        SetError(Utf8ToWide(err));
        return -1;
    }

    meshesParsed_ = true;

    size_t verts = 0, tris = 0;
    for (const xfbin::NudModel& m : models_) {
        verts += m.VertexCount();
        tris  += m.TriangleCount();
    }

    wchar_t buf[256];
    swprintf_s(buf, L"parseMeshes: %zu Modelle, %zu Vertices, %zu Dreiecke "
                    L"in %.1f ms%s",
               models_.size(), verts, tris, msMeshes_,
               ok ? L"" : L" (mit Hinweisen)");
    Log(buf);

    return static_cast<int>(verts);
}

int XfbinImportInterface::ModelCount() {
    if (!RequireMeshes(L"modelCount")) return -1;
    return static_cast<int>(models_.size());
}

const MCHAR* XfbinImportInterface::GetMeshSummary() {
    scratch_.clear();
    if (!RequireMeshes(L"meshSummary")) return scratch_.c_str();
    scratch_ = Cp932ToWide(xfbin::MakeMeshSummary(models_));
    return scratch_.c_str();
}

int XfbinImportInterface::MeshDump(const MCHAR* outPath, int withVertices) {
    if (!RequireMeshes(L"meshDump")) return 0;

    const std::wstring wout = SafeStr(outPath);
    if (wout.empty()) {
        SetError(L"meshDump: leerer Ausgabepfad.");
        return 0;
    }

    std::ofstream f(wout.c_str(), std::ios::binary);
    if (!f) {
        SetError(L"meshDump: Ausgabedatei nicht schreibbar: " + wout);
        return 0;
    }

    std::wstring label = loadedPath_;
    const size_t slash = label.find_last_of(L"/\\");
    if (slash != std::wstring::npos) label = label.substr(slash + 1);

    xfbin::WriteMeshDump(models_, WideToUtf8(label), f, withVertices != 0);
    f.close();

    Log(L"meshDump geschrieben: " + wout);
    return 1;
}

// ------------------------------------------------------------
//  Geometrie in der Szene anlegen
// ------------------------------------------------------------
// ------------------------------------------------------------
//  Skin-Modifier anlegen und Gewichte setzen
//
//  Ueber ISkinImportData, nicht ueber skinOps. Der Unterschied
//  ist nicht kosmetisch: skinOps verlangt, dass der Modifier im
//  Modify-Panel aktiv ist, und wird bei mehreren hundert Bones
//  sehr langsam. ISkinImportData braucht kein Panel und ist um
//  Groessenordnungen schneller - bei 22.601 Vertices ist das der
//  Unterschied zwischen Sekunden und Minuten.
//
//  Reihenfolge ist wichtig:
//    1. Modifier anhaengen
//    2. Szene auswerten lassen (EvalWorldState)
//    3. Bones eintragen, beim letzten update=TRUE
//    4. erneut auswerten lassen
//    5. Gewichte je Vertex setzen
//
//  Ohne die Auswertungsschritte kennt der Modifier das Mesh noch
//  nicht und weist die Gewichte ins Leere zu.
// ------------------------------------------------------------
bool XfbinImportInterface::ApplySkin(
        Interface* ip, INode* node, size_t clumpSlot,
        const std::vector<std::array<uint32_t, 4>>& vertBoneIds,
        const std::vector<std::array<float, 4>>& vertWeights,
        std::wstring& why) {

    Stopwatch sw;

    const std::vector<ULONG>& handles = boneHandles_[clumpSlot];
    if (handles.empty()) {
        why = L"Skelett ist nicht angelegt - erst Bones anlegen.";
        return false;
    }

    // Nur die Bones eintragen, die tatsaechlich Gewicht tragen.
    // Alle 222 in die Liste zu schreiben macht den Modifier
    // unuebersichtlich und das Gewichtewerkzeug traege.
    std::vector<int> used;
    std::vector<int> slotOf(handles.size(), -1);

    for (size_t v = 0; v < vertBoneIds.size(); ++v) {
        for (int k = 0; k < 4; ++k) {
            if (vertWeights[v][static_cast<size_t>(k)] <= 0.0f) continue;
            const uint32_t id = vertBoneIds[v][static_cast<size_t>(k)];
            if (id >= handles.size()) continue;
            if (slotOf[id] < 0) {
                slotOf[id] = static_cast<int>(used.size());
                used.push_back(static_cast<int>(id));
            }
        }
    }

    if (used.empty()) {
        why = L"keine Vertexgewichte gefunden.";
        return false;
    }

    Modifier* skinMod = static_cast<Modifier*>(
        ip->CreateInstance(OSM_CLASS_ID, SKIN_CLASSID));
    if (skinMod == nullptr) {
        why = L"Skin-Modifier liess sich nicht anlegen.";
        return false;
    }

    // Modifier an den Knoten haengen.
    //
    // NICHT ueber Interface::AddModifier - die Methode gibt es dort
    // nicht (sie sitzt auf Interface12). Der versionsunabhaengige
    // und dokumentierte Weg geht ueber das Derived Object: Max haengt
    // beim ersten Modifier ein IDerivedObject zwischen Knoten und
    // Basisobjekt, und dort wird der Modifier eingetragen.
    //
    // Traegt der Knoten schon eines, wird es weiterverwendet -
    // sonst haetten wir zwei ineinander verschachtelte Stapel.
    Object* baseObj = node->GetObjectRef();
    if (baseObj == nullptr) {
        why = L"Knoten hat kein Objekt.";
        return false;
    }

    IDerivedObject* dobj = nullptr;
    if (baseObj->SuperClassID() == GEN_DERIVOB_CLASS_ID) {
        dobj = static_cast<IDerivedObject*>(baseObj);
    } else {
        dobj = CreateDerivedObject(baseObj);
        if (dobj == nullptr) {
            why = L"Derived Object liess sich nicht anlegen.";
            return false;
        }
        node->SetObjectRef(dobj);
    }

    // NULL als ModContext: Max legt den Standardkontext selbst an.
    // Index 0 = oben auf dem Stapel.
    dobj->AddModifier(skinMod, nullptr, 0);

    ISkinImportData* imp = static_cast<ISkinImportData*>(
        skinMod->GetInterface(I_SKINIMPORTDATA));
    if (imp == nullptr) {
        why = L"ISkinImportData nicht verfuegbar.";
        return false;
    }

    // ------------------------------------------------------------
    //  Erweiterte Skin-Parameter
    //
    //  ignoreBoneScale ist der wichtige. In einem
    //  Importer-Beispiel steht der Kommentar "Can get some truly
    //  bizarre animations without this in MAX" - und genau das
    //  beschreibt das Symptom: ein Mesh, das sich beim Abspielen
    //  ins Absurde zieht, obwohl alle Bone-Matrizen heil sind.
    //
    //  Es passt auch zu den Daten. Die Bones dieses Rigs haben
    //  keine exakte Einheitsskalierung, sondern Werte wie
    //  0,999993 und 1,00001 - so stehen sie im nuccChunkCoord.
    //  Skin rechnet die Bone-Skalierung standardmaessig mit, und
    //  ueber eine Kette hinweg baut sich daraus etwas auf, das
    //  mit der eigentlichen Verformung nichts mehr zu tun hat.
    //
    //  bone_Limit auf 4 passt zum Format: NUD speichert genau
    //  vier Gewichte je Vertex.
    //
    //  Beides ueber den Parameterblock 2 (advanced). Schlaegt der
    //  Zugriff fehl, laeuft der Rest trotzdem - deshalb nur
    //  Warnungen, keine Abbrueche.
    // ------------------------------------------------------------
    IParamBlock2* advanced = skinMod->GetParamBlockByID(2);
    if (advanced != nullptr) {
        advanced->SetValue(0x0E, 0, TRUE);   // ignoreBoneScale
        advanced->SetValue(0x07, 0, 4);      // bone_Limit
    } else {
        AddWarning(L"Skin: erweiterter Parameterblock nicht erreichbar - "
                   L"ignoreBoneScale konnte nicht gesetzt werden.");
    }

    // Der Modifier muss das Mesh gesehen haben, bevor Bones und
    // Gewichte hineingehen.
    node->EvalWorldState(ip->GetTime());

    std::vector<INode*> boneNodes;
    boneNodes.reserve(used.size());

    for (size_t i = 0; i < used.size(); ++i) {
        INode* bn = ip->GetINodeByHandle(handles[static_cast<size_t>(used[i])]);
        if (bn == nullptr) {
            why = L"ein Bone-Knoten wurde nicht gefunden.";
            return false;
        }
        // update nur beim letzten Bone - sonst rechnet der
        // Modifier nach jedem einzelnen neu.
        imp->AddBoneEx(bn, (i + 1 == used.size()) ? TRUE : FALSE);
        boneNodes.push_back(bn);
    }

    node->EvalWorldState(ip->GetTime());

    // Die Bind-Lage des MESHES ausdruecklich setzen statt sie
    // Skin selbst herleiten zu lassen. Unsere Vertexdaten liegen
    // bereits im Weltraum und der Knoten steht auf Identitaet -
    // genau das wird hier hinterlegt, fuer beide Matrizen des
    // Knotens (Objekt- und Knotentransformation).
    {
        const Matrix3 nodeTm = node->GetNodeTM(ip->GetTime());
        const Matrix3 objTm  = node->GetObjectTM(ip->GetTime());
        imp->SetSkinTm(node, objTm, nodeTm);
    }

    Tab<INode*> tabBones;
    Tab<float>  tabWeights;

    int weighted = 0;
    int rejected = 0;

    for (size_t v = 0; v < vertBoneIds.size(); ++v) {
        tabBones.ZeroCount();
        tabWeights.ZeroCount();

        float sum = 0.0f;
        for (int k = 0; k < 4; ++k) {
            const float w = vertWeights[v][static_cast<size_t>(k)];
            if (w <= 0.0f) continue;
            const uint32_t id = vertBoneIds[v][static_cast<size_t>(k)];
            if (id >= handles.size() || slotOf[id] < 0) continue;

            INode* bn = boneNodes[static_cast<size_t>(slotOf[id])];
            tabBones.Append(1, &bn);
            tabWeights.Append(1, const_cast<float*>(&vertWeights[v][static_cast<size_t>(k)]));
            sum += w;
        }

        if (tabBones.Count() == 0 || sum <= 0.0f) continue;

        // ------------------------------------------------------
        //  Normalisieren - und zwar IMMER, nicht nur bei grober
        //  Abweichung.
        //
        //  Die SDK-Dokumentation zu AddWeights ist an der Stelle
        //  unmissverstaendlich: die Summe der Gewichte MUSS 1.0
        //  sein, sonst schlaegt der Aufruf fehl. Eine Summe von
        //  0,99999994 aus der Datei reicht also schon, um den
        //  Vertex ungewichtet zu lassen - und das faellt erst auf,
        //  wenn man einen Bone bewegt.
        //
        //  Deshalb: durch die Summe teilen und den letzten Eintrag
        //  so korrigieren, dass die Summe exakt 1.0f ergibt.
        // ------------------------------------------------------
        float running = 0.0f;
        const int last = tabWeights.Count() - 1;
        for (int k = 0; k < last; ++k) {
            tabWeights[k] /= sum;
            running += tabWeights[k];
        }
        tabWeights[last] = 1.0f - running;

        if (imp->AddWeights(node, static_cast<int>(v), tabBones, tabWeights)) {
            ++weighted;
        } else {
            ++rejected;
        }
    }

    msSkin_ += sw.ms();

    if (weighted == 0) {
        why = L"kein Vertex bekam Gewichte (AddWeights hat alle abgelehnt).";
        return false;
    }

    if (rejected > 0) {
        wchar_t wbuf[192];
        swprintf_s(wbuf, L"%d von %d Vertices hat AddWeights abgelehnt.",
                   rejected, weighted + rejected);
        AddWarning(wbuf);
    }

    return true;
}

int XfbinImportInterface::BuildMeshes(int skipLod, int explicitNormals,
                                      float scale) {
    // Alte Signatur beibehalten - sie ruft jetzt die Variante mit
    // Skinning auf. Bestehende MaxScript-Aufrufe aendern sich nicht.
    return BuildMeshesSkinned(skipLod, explicitNormals, 1, scale);
}

int XfbinImportInterface::BuildMeshesSkinned(int skipLod, int explicitNormals,
                                             int applySkin, float scale) {
    return BuildMeshesN(skipLod, explicitNormals, applySkin, scale, 1);
}

// copies <= 0: die Zahl ergibt sich aus dem Skelett. Wie viele
// Instanzen eines Clumps in der Szene stehen, so viele Exemplare
// seiner Modelle werden gebaut.
int XfbinImportInterface::BuildMeshesN(int skipLod, int explicitNormals,
                                       int applySkin, float scale, int copies) {
    if (!RequireMeshes(L"buildMeshes")) return 0;

    Interface* ip = GetCOREInterface();
    if (ip == nullptr) {
        SetError(L"buildMeshes: keine Core-Schnittstelle.");
        return 0;
    }

    if (scale <= 0.0f) scale = 1.0f;

    SceneRedrawGuard redrawGuard(ip);
    HoldSuspendGuard holdGuard;

    Stopwatch sw;

    int made    = 0;
    int skipped = 0;
    int skinned = 0;

    meshNodes_.clear();

    // Die groesste vorkommende Instanzzahl bestimmt, wie oft der
    // Durchlauf ueberhaupt noetig ist; je Modell wird darin
    // geprueft, ob es diese Instanz gibt.
    int maxCopies = (copies > 0) ? copies : 1;
    if (copies <= 0) {
        for (const xfbin::Clump& c : clumps_) {
            const int n = RequiredInstancesRaw(c.name);
            if (n > maxCopies) maxCopies = n;
        }
    }

    for (int copy = 0; copy < maxCopies; ++copy) {
      // Ab der zweiten Instanz einen Zusatz an den Namen - wie
      // bei den Bones auch.
      const std::wstring suffix =
          (copy > 0) ? (L" #" + std::to_wstring(copy + 1)) : std::wstring();

      for (size_t modelIdx = 0; modelIdx < models_.size(); ++modelIdx) {
        const xfbin::NudModel& model = models_[modelIdx];
        const std::wstring wname = Cp932ToWide(model.name) + suffix;

        // LOD-Modelle auslassen. In dieser Datei sind das 5 von 19;
        // sie liegen in einer eigenen Modellgruppe des Clumps und
        // wuerden sonst deckungsgleich ueber dem Original liegen.
        if (skipLod != 0 && wname.find(L"_lod") != std::wstring::npos) {
            ++skipped;
            continue;
        }

        // Zugehoerigen Clump ueber Page und page-lokalen Chunkindex
        // finden. clumpIndex ist page-lokal, nicht global.
        const xfbin::Clump* clump = nullptr;
        size_t clumpSlot = SIZE_MAX;
        for (size_t ci = 0; ci < clumps_.size(); ++ci) {
            if (clumps_[ci].pageIndex == model.pageIndex &&
                clumps_[ci].localMapIndex == model.clumpIndex) {
                clump = &clumps_[ci];
                break;
            }
        }

        // Die Bone-Handles liegen im Szenenzustand, und dessen
        // Reihenfolge ist eine andere - ueber Name und laufende
        // Nummer suchen.
        if (clump != nullptr) {
            for (size_t i = 0; i < sceneClumps_.size(); ++i) {
                if (sceneClumps_[i].name == clump->name &&
                    sceneInstance_[i] == copy) {
                    clumpSlot = i;
                    break;
                }
            }
        }

        // Der Mesh-Bone: in seinem lokalen Raum liegen die
        // Vertexdaten. Genau wie im Blender-Importer werden die
        // Positionen damit in den Weltraum gehoben.
        xfbin::Mat43 meshMatrix = xfbin::Mat43::Identity();
        const xfbin::CoordNode* meshBone = nullptr;

        if (clump != nullptr &&
            model.meshBoneIndex < clump->nodes.size()) {
            meshBone   = &clump->nodes[model.meshBoneIndex];
            meshMatrix = meshBone->world;
        }

        // Gibt es diese Instanz fuer den Clump des Modells gar
        // nicht, wird das Modell in diesem Durchlauf ausgelassen.
        // Bei automatischer Zaehlung laeuft die Schleife bis zur
        // groessten vorkommenden Instanzzahl - die meisten Clumps
        // brauchen weniger.
        if (copies <= 0 && clump != nullptr) {
            bool exists = false;
            for (size_t i = 0; i < sceneClumps_.size(); ++i) {
                if (sceneClumps_[i].name == clump->name &&
                    sceneInstance_[i] == copy) {
                    exists = true;
                    break;
                }
            }
            if (!exists) continue;
        }

        // --- Groessen zaehlen ---
        int totalVerts = 0;
        int totalFaces = 0;
        int uvChannels = 0;
        bool anyColor  = false;
        bool anyNormal = false;

        for (const xfbin::NudMeshGroup& g : model.groups) {
            for (const xfbin::NudMesh& m : g.meshes) {
                totalVerts += static_cast<int>(m.VertexCount());
                totalFaces += static_cast<int>(m.TriangleCount());
                if (m.uvCount > uvChannels) uvChannels = m.uvCount;
                if (m.HasColor())  anyColor  = true;
                if (m.HasNormal()) anyNormal = true;
            }
        }

        if (totalVerts == 0 || totalFaces == 0) {
            AddWarning(L"Modell '" + wname + L"' hat keine Geometrie.");
            continue;
        }

        TriObject* triObj = CreateNewTriObject();
        if (triObj == nullptr) {
            AddWarning(L"Modell '" + wname + L"': TriObject liess sich nicht anlegen.");
            continue;
        }

        Mesh& mesh = triObj->GetMesh();
        mesh.setNumVerts(totalVerts);
        mesh.setNumFaces(totalFaces);

        // Map-Kanaele: 0 ist die Vertexfarbe, 1..n sind die UVs.
        // Max zaehlt UV-Kanaele ab 1, NUD ab 0 - daher der Versatz.
        if (anyColor) {
            mesh.setMapSupport(0, TRUE);
            mesh.setNumMapVerts(0, totalVerts);
        }
        for (int u = 0; u < uvChannels; ++u) {
            mesh.setMapSupport(u + 1, TRUE);
            mesh.setNumMapVerts(u + 1, totalVerts);
        }

        std::vector<Point3> normals;
        if (anyNormal && explicitNormals != 0) normals.resize(totalVerts);

        // Gewichte fuer den Skin-Modifier. Sie werden hier
        // eingesammelt, weil nur an dieser Stelle bekannt ist, auf
        // welchen Index im zusammengefassten Mesh ein Submesh-Vertex
        // gelandet ist. In einem zweiten Durchgang muesste man diese
        // Zuordnung neu herleiten.
        const bool wantSkin = (applySkin != 0);
        bool modelIsSkinned = false;
        for (const xfbin::NudMeshGroup& g : model.groups) {
            for (const xfbin::NudMesh& m : g.meshes) {
                if (m.HasBones()) { modelIsSkinned = true; break; }
            }
            if (modelIsSkinned) break;
        }

        std::vector<std::array<uint32_t, 4>> vertBoneIds;
        std::vector<std::array<float, 4>>    vertWeights;
        if (wantSkin && modelIsSkinned) {
            vertBoneIds.assign(static_cast<size_t>(totalVerts), { 0, 0, 0, 0 });
            vertWeights.assign(static_cast<size_t>(totalVerts), { 0.0f, 0.0f, 0.0f, 0.0f });
        }

        int vOffset = 0;
        int fOffset = 0;

        for (const xfbin::NudMeshGroup& g : model.groups) {
            for (size_t si = 0; si < g.meshes.size(); ++si) {
                const xfbin::NudMesh& m = g.meshes[si];

                const int nv = static_cast<int>(m.VertexCount());
                const int nf = static_cast<int>(m.TriangleCount());

                // Material-ID ist die Position des Submeshes in
                // seiner Gruppe - dieselbe Zuordnung wie im
                // Blender-Importer.
                const MtlID matId = static_cast<MtlID>(si);

                for (int i = 0; i < nv; ++i) {
                    const xfbin::Vec3& p = m.position[static_cast<size_t>(i)];

                    // Zeilenvektor: Punkt links, Matrix rechts.
                    const double x = p.x * meshMatrix.m[0][0] + p.y * meshMatrix.m[1][0]
                                   + p.z * meshMatrix.m[2][0] + meshMatrix.m[3][0];
                    const double y = p.x * meshMatrix.m[0][1] + p.y * meshMatrix.m[1][1]
                                   + p.z * meshMatrix.m[2][1] + meshMatrix.m[3][1];
                    const double z = p.x * meshMatrix.m[0][2] + p.y * meshMatrix.m[1][2]
                                   + p.z * meshMatrix.m[2][2] + meshMatrix.m[3][2];

                    mesh.setVert(vOffset + i,
                                 Point3(static_cast<float>(x) * scale,
                                        static_cast<float>(y) * scale,
                                        static_cast<float>(z) * scale));

                    if (!normals.empty()) {
                        if (m.HasNormal()) {
                            const xfbin::Vec3& n = m.normal[static_cast<size_t>(i)];
                            // Normalen nur drehen, nicht verschieben.
                            Point3 wn(
                                static_cast<float>(n.x * meshMatrix.m[0][0] +
                                                   n.y * meshMatrix.m[1][0] +
                                                   n.z * meshMatrix.m[2][0]),
                                static_cast<float>(n.x * meshMatrix.m[0][1] +
                                                   n.y * meshMatrix.m[1][1] +
                                                   n.z * meshMatrix.m[2][1]),
                                static_cast<float>(n.x * meshMatrix.m[0][2] +
                                                   n.y * meshMatrix.m[1][2] +
                                                   n.z * meshMatrix.m[2][2]));
                            normals[static_cast<size_t>(vOffset + i)] =
                                Normalize(wn);
                        } else {
                            normals[static_cast<size_t>(vOffset + i)] =
                                Point3(0.0f, 0.0f, 1.0f);
                        }
                    }

                    if (!vertBoneIds.empty() && m.HasBones()) {
                        vertBoneIds[static_cast<size_t>(vOffset + i)] =
                            m.boneIds[static_cast<size_t>(i)];
                        vertWeights[static_cast<size_t>(vOffset + i)] =
                            m.boneWeights[static_cast<size_t>(i)];
                    }

                    if (anyColor) {
                        if (m.HasColor()) {
                            const xfbin::Color4& c = m.color[static_cast<size_t>(i)];
                            mesh.setMapVert(0, vOffset + i,
                                            UVVert(c.r / 255.0f,
                                                   c.g / 255.0f,
                                                   c.b / 255.0f));
                        } else {
                            mesh.setMapVert(0, vOffset + i, UVVert(1.0f, 1.0f, 1.0f));
                        }
                    }

                    for (int u = 0; u < uvChannels; ++u) {
                        UVVert t(0.0f, 1.0f, 0.0f);
                        if (u < m.uvCount) {
                            const xfbin::Vec2& s = m.uv[static_cast<size_t>(u)][static_cast<size_t>(i)];
                            // V-Achse spiegeln - wie in Blender auch.
                            t = UVVert(s.x, 1.0f - s.y, 0.0f);
                        }
                        mesh.setMapVert(u + 1, vOffset + i, t);
                    }
                }

                for (int f = 0; f < nf; ++f) {
                    const int a = vOffset + static_cast<int>(m.triangles[static_cast<size_t>(f) * 3 + 0]);
                    const int b = vOffset + static_cast<int>(m.triangles[static_cast<size_t>(f) * 3 + 1]);
                    const int c = vOffset + static_cast<int>(m.triangles[static_cast<size_t>(f) * 3 + 2]);

                    Face& face = mesh.faces[fOffset + f];
                    face.setVerts(a, b, c);
                    face.setEdgeVisFlags(1, 1, 1);
                    // Eine Glaettungsgruppe fuer alles. Die echte
                    // Glaettung kommt aus den expliziten Normalen;
                    // ohne gesetzte Gruppe waere das Modell
                    // facettiert.
                    face.setSmGroup(1);
                    face.setMatID(matId);

                    if (anyColor) {
                        mesh.mapFaces(0)[fOffset + f].setTVerts(a, b, c);
                    }
                    for (int u = 0; u < uvChannels; ++u) {
                        mesh.mapFaces(u + 1)[fOffset + f].setTVerts(a, b, c);
                    }
                }

                vOffset += nv;
                fOffset += nf;
            }
        }

        // --- Explizite Normalen ---
        //
        // Ohne die berechnet Max die Normalen aus den
        // Glaettungsgruppen neu, und die Kantenrundung eines
        // Cel-Shading-Modells geht verloren. Die Zuordnung ist
        // hier einfach: eine Normale je Vertex, jede Ecke zeigt
        // auf ihren eigenen Vertexindex.
        if (!normals.empty()) {
            mesh.SpecifyNormals();
            MeshNormalSpec* ns = mesh.GetSpecifiedNormals();
            if (ns != nullptr) {
                ns->ClearAndFree();
                ns->SetNumFaces(totalFaces);
                ns->SetNumNormals(totalVerts);

                for (int i = 0; i < totalVerts; ++i) {
                    ns->Normal(i) = normals[static_cast<size_t>(i)];
                    ns->SetNormalExplicit(i, true);
                }
                for (int f = 0; f < totalFaces; ++f) {
                    for (int c = 0; c < 3; ++c) {
                        ns->Face(f).SpecifyNormalID(
                            c, mesh.faces[f].getVert(c));
                    }
                }
                ns->SetFlag(MESH_NORMAL_NORMALS_BUILT, true);
                ns->CheckNormals();
            }
        }

        mesh.InvalidateGeomCache();

        INode* node = ip->CreateObjectNode(triObj);
        if (node == nullptr) {
            AddWarning(L"Modell '" + wname + L"': Knoten liess sich nicht anlegen.");
            continue;
        }
        node->SetName(const_cast<MCHAR*>(wname.c_str()));

        // Fuer buildMaterials merken, welcher Knoten zu welchem
        // Modell gehoert.
        {
            MeshRef ref;
            ref.handle     = node->GetHandle();
            ref.modelIndex = modelIdx;
            meshNodes_.push_back(ref);

            // Fuer die Sichtbarkeit: ueber Dateigrenzen hinweg
            // merken, zu welchem Clump und Mesh-Bone dieses
            // Objekt gehoert.
            SceneMesh sm;
            sm.handle    = ref.handle;
            sm.instance  = copy;
            if (clump != nullptr) sm.clumpName = clump->name;
            if (meshBone != nullptr) sm.boneName = meshBone->name;
            sceneMeshes_.push_back(sm);
        }

        // ------------------------------------------------------
        //  Verknuepfung mit dem Skelett
        //
        //  Ungeskinnte Modelle (Zaehne, Augen) haengen an ihrem
        //  Mesh-Bone und folgen ihm dadurch.
        //
        //  Geskinnte Modelle werden NICHT gehaengt. Sonst wirkt die
        //  Bewegung des Mesh-Bones zweimal: einmal ueber die
        //  Elternbeziehung und einmal ueber den Skin-Modifier. Der
        //  Blender-Importer setzt zwar beides, aber Blenders
        //  Bone-Parenting rechnet die Ruhelage heraus - Max' nicht.
        // ------------------------------------------------------
        const bool useSkin = wantSkin && modelIsSkinned &&
                             clump != nullptr && clumpSlot != SIZE_MAX &&
                             clumpSlot < boneHandles_.size();

        if (!useSkin && meshBone != nullptr && clumpSlot != SIZE_MAX &&
            clumpSlot < boneHandles_.size()) {
            const std::vector<ULONG>& handles = boneHandles_[clumpSlot];
            if (model.meshBoneIndex < handles.size() &&
                handles[model.meshBoneIndex] != 0) {
                INode* boneNode = ip->GetINodeByHandle(handles[model.meshBoneIndex]);
                if (boneNode != nullptr) boneNode->AttachChild(node, 1);
            }
        }

        if (useSkin) {
            std::wstring why;
            if (!ApplySkin(ip, node, clumpSlot, vertBoneIds, vertWeights, why)) {
                AddWarning(L"Modell '" + wname + L"': " + why);
            } else {
                ++skinned;
            }
        }

        ++made;
      } // Ende der Modell-Schleife
    } // Ende der Kopien-Schleife

    msGeom_ = sw.ms();

    wchar_t buf[256];
    swprintf_s(buf, L"buildMeshes: %d Objekte in %.1f ms (%d LOD ausgelassen, "
                    L"%d geskinnt in %.1f ms, Normalen %s, Skalierung %.3f)",
               made, msGeom_, skipped, skinned, msSkin_,
               explicitNormals ? L"explizit" : L"aus",
               static_cast<double>(scale));
    Log(buf);

    ip->RedrawViews(ip->GetTime());

    return made;
}


// ============================================================
//  Stufe 5: Animationen
// ============================================================

bool XfbinImportInterface::RequireAnims(const wchar_t* what) {
    // Sind schon Animationen geladen, braucht es keine offene
    // Datei mehr - sie koennen aus mehreren stammen.
    if (animsParsed_ && !anims_.empty()) return true;
    if (!RequireFile(what)) return false;
    return (ParseAnimsAppend() >= 0);
}

int XfbinImportInterface::ParseAnims() {
    anims_.clear();
    animsParsed_ = false;
    return ParseAnimsAppend();
}

int XfbinImportInterface::ClearAnims() {
    const int had = static_cast<int>(anims_.size());
    anims_.clear();
    animsParsed_ = false;
    return had;
}

// ------------------------------------------------------------
//  Animationen der geoeffneten Datei ANHAENGEN
//
//  Ein Charakter kann mehr als eine Animationsdatei haben. Pein
//  bringt vier mit - Hauptanimationen, Zusatzkombos,
//  Nachzuegler und Ultimates - zusammen 104 Stueck, und sie
//  sprechen gemeinsam siebzehn verschiedene Skelette an. Nur die
//  erste zu laden hiesse, zwei Drittel wegzuwerfen.
// ------------------------------------------------------------
int XfbinImportInterface::ParseAnimsAppend() {
    if (!RequireFile(L"parseAnims")) return -1;

    std::vector<xfbin::Anm> fresh;
    std::string err, warn;

    Stopwatch sw;
    const bool ok = xfbin::ParseAnims(*file_, fresh, err, warn);
    msAnims_ += sw.ms();

    if (!warn.empty()) {
        std::istringstream in(warn);
        std::string line;
        while (std::getline(in, line)) AddWarning(Utf8ToWide(line));
    }

    if (fresh.empty()) {
        if (anims_.empty()) SetError(Utf8ToWide(err));
        return anims_.empty() ? -1 : 0;
    }

    const size_t before = anims_.size();
    anims_.insert(anims_.end(), fresh.begin(), fresh.end());
    animsParsed_ = true;

    size_t keys = 0;
    for (const xfbin::Anm& a : anims_) keys += a.KeyframeCount();

    wchar_t buf[320];
    swprintf_s(buf, L"parseAnims: +%zu Animationen (jetzt %zu), %zu Keyframes"
                    L" in %.1f ms%s",
               anims_.size() - before, anims_.size(), keys, msAnims_,
               ok ? L"" : L" (mit Hinweisen)");
    Log(buf);

    return static_cast<int>(keys);
}

int XfbinImportInterface::AnimCount() {
    if (!RequireAnims(L"animCount")) return -1;
    return static_cast<int>(anims_.size());
}

const MCHAR* XfbinImportInterface::GetAnimName(int index) {
    scratch_.clear();
    if (!RequireAnims(L"animName")) return scratch_.c_str();
    if (index < 0 || index >= static_cast<int>(anims_.size())) {
        SetError(L"animName: Index ausserhalb des Bereichs.");
        return scratch_.c_str();
    }
    scratch_ = Cp932ToWide(anims_[static_cast<size_t>(index)].name);
    return scratch_.c_str();
}

const MCHAR* XfbinImportInterface::GetAnimSummary() {
    scratch_.clear();
    if (!RequireAnims(L"animSummary")) return scratch_.c_str();
    scratch_ = Cp932ToWide(xfbin::MakeAnimSummary(anims_));
    return scratch_.c_str();
}

int XfbinImportInterface::AnimDump(const MCHAR* outPath, int withKeys) {
    if (!RequireAnims(L"animDump")) return 0;

    const std::wstring wout = SafeStr(outPath);
    if (wout.empty()) {
        SetError(L"animDump: leerer Ausgabepfad.");
        return 0;
    }

    std::ofstream f(wout.c_str(), std::ios::binary);
    if (!f) {
        SetError(L"animDump: Ausgabedatei nicht schreibbar: " + wout);
        return 0;
    }

    std::wstring label = loadedPath_;
    const size_t slash = label.find_last_of(L"/\\");
    if (slash != std::wstring::npos) label = label.substr(slash + 1);

    xfbin::WriteAnimDump(anims_, WideToUtf8(label), f, withKeys != 0);
    f.close();

    Log(L"animDump geschrieben: " + wout);
    return 1;
}

// 0 = direkt uebernehmen (Standard, wie 0.5.4),
// 1 = konjugieren. Der Umschalter wirkt auf BEIDE
// Rotationsquellen - Quaternion- wie Euler-Kurven.
// Breite und Hoehe der Bone-Objekte. Negativ heisst: Max'
// Standardwert stehen lassen.
int XfbinImportInterface::SetBoneSize(float size) {
    boneSize_ = size;
    return 1;
}

int XfbinImportInterface::SetQuatMode(int mode) {
    quatMode_ = (mode != 0) ? 1 : 0;
    return quatMode_;
}

// ------------------------------------------------------------
//  Eine Animation auf das vorhandene Skelett legen
//
//  Die Kurven im ANM-Chunk beschreiben die LOKALE Lage eines
//  Bones - dieselbe Groesse, die auch im nuccChunkCoord als
//  Ruhelage steht. Genau das haelt in Max der PRS-Controller des
//  Knotens: Position, Rotation und Skalierung relativ zum
//  Elternteil. Die Werte koennen also direkt hinein.
//
//  Gesetzt wird ueber Control::SetValue innerhalb eines
//  AnimateOn-Blocks statt ueber IKeyControl. Grund: SetValue
//  kommt mit jedem Controllertyp zurecht, den Max dem Knoten
//  gegeben hat, und braucht keine Annahme ueber die
//  Achsenreihenfolge eines Euler-Controllers - genau die Annahme
//  hat in AnimMerge mehrere Anlaeufe gekostet. Die Tangenten sind
//  dafuer Max' Standard; sollte sich Ueberschwingen zeigen, ist
//  der Wechsel auf IKeyControl mit ausdruecklichen Tangenten der
//  naechste Schritt.
//
//  Die Rotation geht bewusst ueber eine Matrix3 und wird von Max
//  selbst in ein Quat gewandelt. Damit erbt sie die Konvention
//  aus der Bind-Pose, die gegen Blender bereits zeichengleich
//  geprueft ist - statt einer zweiten, unabhaengigen Annahme
//  darueber, wie Max Quaternionen in Matrizen umrechnet.
// ------------------------------------------------------------
int XfbinImportInterface::BuildAnim(int index, float scale) {
    // Alle drei Kanaele - die uebliche Verwendung.
    return BuildAnimEx(index, 7, scale);
}

// ------------------------------------------------------------
//  channelMask: Bit 1 = Position, Bit 2 = Rotation, Bit 4 =
//  Skalierung.
//
//  Der Sinn ist die Eingrenzung. Wenn eine Animation etwas
//  verzerrt, ist die erste Frage, WELCHER der drei Kanaele es
//  tut - und die beantwortet man in dreissig Sekunden durch
//  Ausprobieren, statt sie sich aus einem Screenshot
//  herzuleiten. Zweimal daneben geraten ist einmal zu viel.
// ------------------------------------------------------------
int XfbinImportInterface::BuildAnimEx(int index, int channelMask, float scale) {
    return BuildAnimAt(index, 0.0f, channelMask, scale);
}

int XfbinImportInterface::BuildAnimAt(int index, float startFrame,
                                      int channelMask, float scale) {
    if (!RequireAnims(L"buildAnim")) return 0;

    if (index < 0 || index >= static_cast<int>(anims_.size())) {
        SetError(L"buildAnim: Index ausserhalb des Bereichs.");
        return 0;
    }
    // Post-process-/Kamera-only Clips (z.B. *_blur, *_glare aus
    // 2kbxspl1) haben keine Bone-Eintraege - nichts zu keyen.
    if (!anims_[static_cast<size_t>(index)].HasBoneEntries()) {
        return 0;
    }
    if (sceneClumps_.empty() || boneHandles_.empty()) {
        SetError(L"buildAnim: es steht kein Skelett in der Szene. "
                 L"Erst die Modelldatei oeffnen und Bones anlegen, dann "
                 L"die Animationsdatei.");
        return 0;
    }

    Interface* ip = GetCOREInterface();
    if (ip == nullptr) {
        SetError(L"buildAnim: keine Core-Schnittstelle.");
        return 0;
    }

    if (scale <= 0.0f) scale = 1.0f;

    const xfbin::Anm& anm = anims_[static_cast<size_t>(index)];
    const int tpf = GetTicksPerFrame();

    SceneRedrawGuard redrawGuard(ip);
    HoldSuspendGuard holdGuard;

    Stopwatch sw;

    int keysSet      = 0;
    int unmatched    = 0;
    int foreignClump = 0;
    std::string  foreignNames;
    std::wstring firstUnmatched;

    SuspendAnimate();
    AnimateOn();

    for (const xfbin::AnmEntry& entry : anm.entries) {
        // Nur Bone-Eintraege. Material-, Kamera- und Lichtkurven
        // kommen spaeter.
        if (entry.entryFormat != xfbin::kEntryBone) continue;

        // Clump ueber den Namen finden. Der Index im ANM zeigt in
        // die Referenztabelle der Page, nicht in unsere Liste.
        // ------------------------------------------------------
        //  Clump zuordnen - ueber den NAMEN, nicht ueber den Index.
        //
        //  Eine Animationsdatei spricht mehr als ein Modell an.
        //  1hakbod1c.xfbin etwa nennt drei Clumps: zweimal
        //  1haksbn1 (die Waffe) und einmal 1hakbod1 (die Figur),
        //  dazu in einzelnen Animationen noch "wrinkles",
        //  "team_leader" und "1efc_dmy01".
        //
        //  Bis 0.5.3 gab es hier einen Rueckfall "steht nur ein
        //  Skelett in der Szene, dann nimm das". Der hat die 282
        //  Eintraege der fremden Clumps mit auf das Figurenskelett
        //  geworfen. In dieser Datei ohne Folgen - keiner der
        //  Namen kollidiert -, aber es ist Glueck, kein Entwurf:
        //  ein gleichnamiger Bone in zwei Clumps genuegt, und die
        //  Animation landet auf dem falschen Knochen.
        // ------------------------------------------------------
        size_t clumpSlot = SIZE_MAX;
        size_t boneSlot  = SIZE_MAX;

        if (!ResolveEntryTarget(anm, entry, clumpSlot, boneSlot)) {
            if (clumpSlot == SIZE_MAX) {
                ++foreignClump;
                if (foreignNames.find(entry.clumpName) == std::string::npos) {
                    if (!foreignNames.empty()) foreignNames += ", ";
                    foreignNames += entry.clumpName;
                }
            } else {
                ++unmatched;
                if (firstUnmatched.empty()) {
                    firstUnmatched = Cp932ToWide(entry.targetName);
                }
            }
            continue;
        }

        const std::vector<ULONG>& handles = boneHandles_[clumpSlot];
        if (boneSlot >= handles.size() || handles[boneSlot] == 0) continue;

        INode* bn = ip->GetINodeByHandle(handles[boneSlot]);
        if (bn == nullptr) continue;

        Control* tmc = bn->GetTMController();
        if (tmc == nullptr) continue;

        Control* posCtrl = tmc->GetPositionController();
        Control* rotCtrl = tmc->GetRotationController();
        Control* sclCtrl = tmc->GetScaleController();

        for (const xfbin::AnmCurve& curve : entry.curves) {
            const xfbin::AnmBoneChannel ch =
                xfbin::BoneChannelOf(curve.curveIndex, curve.curveFormat);

            for (const xfbin::AnmKey& k : curve.keys) {
                // Zeitversatz: im Sequenzmodus liegen die
                // Animationen hintereinander auf einer Zeitleiste.
                const TimeValue t = static_cast<TimeValue>(
                    (k.frame + static_cast<double>(startFrame))
                    * static_cast<double>(tpf) + 0.5);

                if (ch == xfbin::kChannelLocation && k.count >= 3 && posCtrl &&
                    (channelMask & 1)) {
                    Point3 p(static_cast<float>(k.value[0]) * scale,
                             static_cast<float>(k.value[1]) * scale,
                             static_cast<float>(k.value[2]) * scale);
                    posCtrl->SetValue(t, &p, TRUE, CTRL_ABSOLUTE);
                    ++keysSet;
                } else if (ch == xfbin::kChannelScale && k.count >= 3 && sclCtrl &&
                           (channelMask & 4)) {
                    // ------------------------------------------
                    //  ScaleValue traegt zwei Dinge: den Faktor s
                    //  und eine Quaternion q. Die SDK-Referenz:
                    //  "the quaternion q defines the axis system in
                    //  which scaling is to be applied" - q sagt
                    //  also, ENTLANG WELCHER Achsen gestreckt wird.
                    //
                    //  Dazu die Referenz zur Quat-Klasse:
                    //  "Constructor. No initialization is
                    //  performed." Der Standardkonstruktor setzt
                    //  nichts. Verlaesst sich ScaleValue(Point3)
                    //  darauf, ist q Speichermuell - und dann wird
                    //  in einer zufaelligen Richtung gestreckt.
                    //
                    //  Deshalb der Zwei-Argument-Konstruktor mit
                    //  ausdruecklicher Einheitsquaternion. Kein
                    //  Verlass auf irgendeinen Standardwert, weder
                    //  bei ScaleValue noch bei Quat.
                    // ------------------------------------------
                    ScaleValue sv(
                        Point3(static_cast<float>(k.value[0]),
                               static_cast<float>(k.value[1]),
                               static_cast<float>(k.value[2])),
                        Quat(0.0f, 0.0f, 0.0f, 1.0f));

                    sclCtrl->SetValue(t, &sv, TRUE, CTRL_ABSOLUTE);
                    ++keysSet;
                } else if (rotCtrl && (channelMask & 2) &&
                           (ch == xfbin::kChannelRotationQuat ||
                            ch == xfbin::kChannelRotationEuler)) {
                    // Beide Rotationsquellen laufen ueber DIESELBE
                    // Matrix und dieselbe Umwandlung. Bis 0.5.4 war
                    // das nicht so: der Umschalter griff nur in den
                    // Quaternion-Pfad, waehrend die 2.174
                    // Euler-Kurven immer die andere Konvention
                    // benutzten. Zwei Rotationsquellen mit
                    // verschiedenen Konventionen im selben Skelett -
                    // damit kann keine Einstellung richtig sein.
                    xfbin::Mat43 r;

                    if (ch == xfbin::kChannelRotationEuler) {
                        if (k.count < 3) continue;
                        r = xfbin::MakeRotation(k.value[0], k.value[1], k.value[2]);
                    } else {
                        if (k.count < 4) continue;
                        r = xfbin::MakeRotationFromQuat(k.value[0], k.value[1],
                                                        k.value[2], k.value[3]);
                    }

                    Matrix3 m3(1);
                    for (int row = 0; row < 3; ++row) {
                        m3.SetRow(row, Point3(static_cast<float>(r.m[row][0]),
                                              static_cast<float>(r.m[row][1]),
                                              static_cast<float>(r.m[row][2])));
                    }

                    // ----------------------------------------------
                    //  Quaternion direkt uebernehmen - NICHT
                    //  konjugieren.
                    //
                    //  0.6.0 hat hier konjugiert, mit Verweis auf den
                    //  MDLX-Importer, wo genau das die Loesung war.
                    //  Das war eine falsche Uebertragung: dort ging es
                    //  um die Quaternion-Konvention des MDX-FORMATS,
                    //  nicht um eine Eigenheit von Max. Am Modell hat
                    //  es die Animation sichtbar verschlechtert.
                    //
                    //  Die SDK-Referenz zu Control::SetValue ist an
                    //  der Stelle eindeutig: bei CTRL_ABSOLUTE zeigt
                    //  val auf ein Quat, und "the controller should
                    //  simply store the value". Keine Umrechnung.
                    //
                    //  Der Umschalter bleibt als Notausgang, wirkt
                    //  aber auf BEIDE Rotationsquellen. Zwei
                    //  Konventionen in einem Skelett - Euler hier,
                    //  Quaternion dort - koennen nie beide stimmen.
                    // ----------------------------------------------
                    Quat q(m3);
                    if (quatMode_ != 0) q = Conjugate(q);

                    rotCtrl->SetValue(t, &q, TRUE, CTRL_ABSOLUTE);
                    ++keysSet;
                }
                // kChannelOpacity wird noch nicht uebertragen -
                // dafuer braucht es das Material aus Stufe 4.
            }
        }
    }

    AnimateOff();
    ResumeAnimate();

    msKeys_ = sw.ms();

    // Zeitleiste mitwachsen lassen statt sie zu ersetzen - im
    // Sequenzmodus kommt eine Animation nach der anderen dazu.
    const TimeValue end = static_cast<TimeValue>(
        (anm.frameCount + static_cast<double>(startFrame))
        * static_cast<double>(tpf) + 0.5);

    const Interval cur = ip->GetAnimRange();
    if (end > cur.End()) ip->SetAnimRange(Interval(cur.Start(), end));

    if (foreignClump > 0) {
        // Das ist der Normalfall, kein Fehler: eine
        // Animationsdatei spricht mehrere Modelle an, und nur
        // eines davon steht in der Szene.
        wchar_t wbuf[420];
        swprintf_s(wbuf, L"%d Eintraege gehoeren zu Clumps, die nicht in der "
                         L"Szene stehen (%s) - uebersprungen.",
                   foreignClump, Cp932ToWide(foreignNames).c_str());
        AddWarning(wbuf);
    }

    if (unmatched > 0) {
        wchar_t wbuf[256];
        swprintf_s(wbuf, L"%d Eintraege ohne passenden Bone (erster: '%s'). "
                         L"Gehoert die Animation zu diesem Modell?",
                   unmatched, firstUnmatched.c_str());
        AddWarning(wbuf);
    }

    wchar_t buf[256];
    swprintf_s(buf, L"buildAnim '%s': %d Keys in %.1f ms, %.2f Frames ab %.0f "
                    L"(Kanaele: %s%s%s)",
               Cp932ToWide(anm.name).c_str(), keysSet, msKeys_, anm.frameCount,
               static_cast<double>(startFrame),
               (channelMask & 1) ? L"Pos " : L"",
               (channelMask & 2) ? L"Rot " : L"",
               (channelMask & 4) ? L"Scl"  : L"");
    Log(buf);

    ip->RedrawViews(ip->GetTime());

    return keysSet;
}





// ------------------------------------------------------------
//  Welche Bones eine Animation ueberhaupt anfasst
//
//  Ausgelagert, weil BuildAnimAt und BuildIdleKeys dieselbe
//  Zuordnung brauchen: Eintrag -> Szenen-Clump und Bone. Zwei
//  Kopien davon waeren zwei Stellen, die auseinanderlaufen
//  koennen.
// ------------------------------------------------------------
bool XfbinImportInterface::ResolveEntryTarget(const xfbin::Anm& anm,
                                              const xfbin::AnmEntry& entry,
                                              size_t& clumpSlot,
                                              size_t& boneSlot) {
    clumpSlot = SIZE_MAX;
    boneSlot  = SIZE_MAX;

    // Welche Instanz? Die laufende Nummer ergibt sich daraus, wie
    // viele gleichnamige Clump-Referenzen VOR dieser stehen.
    int wantInstance = 0;
    if (entry.clumpIndex > 0) {
        for (int ci = 0; ci < entry.clumpIndex; ++ci) {
            if (static_cast<size_t>(ci) < anm.clumps.size() &&
                anm.clumps[static_cast<size_t>(ci)].clumpName == entry.clumpName) {
                ++wantInstance;
            }
        }
    }

    for (size_t ci = 0; ci < sceneClumps_.size(); ++ci) {
        if (sceneClumps_[ci].name == entry.clumpName &&
            sceneInstance_[ci] == wantInstance) {
            clumpSlot = ci;
            break;
        }
    }
    if (clumpSlot == SIZE_MAX) {
        for (size_t ci = 0; ci < sceneClumps_.size(); ++ci) {
            if (sceneClumps_[ci].name == entry.clumpName) { clumpSlot = ci; break; }
        }
    }
    if (clumpSlot == SIZE_MAX &&
        sceneClumps_.size() == 1 && anm.clumps.size() == 1) {
        clumpSlot = 0;
    }
    if (clumpSlot == SIZE_MAX) return false;

    const xfbin::Clump& clump = sceneClumps_[clumpSlot];
    for (size_t bi = 0; bi < clump.nodes.size(); ++bi) {
        if (clump.nodes[bi].name == entry.targetName) { boneSlot = bi; break; }
    }

    return (boneSlot != SIZE_MAX);
}

// ------------------------------------------------------------
//  Ruhelage-Keys fuer alles, was diese Animation NICHT anfasst
//
//  Ohne das ist eine Sequenz nicht in sich abgeschlossen. Die
//  Zahlen aus dieser Datei: eine typische Animation bewegt 112
//  der 222 Figur-Bones, und "1hakdmg0f" ruehrt die Waffen gar
//  nicht an. Die uebrigen Knoten haetten zwischen zwei Sequenzen
//  keinen einzigen Key - Max interpoliert dann quer durch die
//  Luecke, und die vorige Animation blutet in die naechste.
//
//  Also je einen Key am Anfang und am Ende der Sequenz, auf der
//  Bind-Pose. Nur fuer die nicht angefassten Bones: bei den
//  animierten wuerde ein Key am Ende die Bewegung zurueckreissen.
// ------------------------------------------------------------
int XfbinImportInterface::BuildIdleKeys(int index, float startFrame,
                                        float endFrame) {
    if (!RequireAnims(L"buildIdleKeys")) return 0;
    if (index < 0 || index >= static_cast<int>(anims_.size())) return 0;
    if (!anims_[static_cast<size_t>(index)].HasBoneEntries()) return 0;
    if (sceneClumps_.empty() || boneHandles_.empty()) return 0;

    Interface* ip = GetCOREInterface();
    if (ip == nullptr) return 0;

    const xfbin::Anm& anm = anims_[static_cast<size_t>(index)];
    const int tpf = GetTicksPerFrame();

    // Angefasste Bones sammeln.
    std::set<std::pair<size_t, size_t>> touched;
    for (const xfbin::AnmEntry& e : anm.entries) {
        if (e.entryFormat != xfbin::kEntryBone) continue;
        size_t cs = 0, bs = 0;
        if (ResolveEntryTarget(anm, e, cs, bs)) touched.insert({ cs, bs });
    }

    const TimeValue t0 =
        static_cast<TimeValue>(static_cast<double>(startFrame) * tpf + 0.5);
    const TimeValue t1 =
        static_cast<TimeValue>(static_cast<double>(endFrame) * tpf + 0.5);

    SceneRedrawGuard redrawGuard(ip);
    HoldSuspendGuard holdGuard;

    int keyed = 0;

    SuspendAnimate();
    AnimateOn();

    for (size_t ci = 0; ci < sceneClumps_.size(); ++ci) {
        if (ci >= boneHandles_.size()) break;
        const xfbin::Clump& clump = sceneClumps_[ci];
        const std::vector<ULONG>& handles = boneHandles_[ci];

        for (size_t bi = 0; bi < clump.nodes.size() && bi < handles.size(); ++bi) {
            if (touched.count({ ci, bi }) != 0) continue;
            if (handles[bi] == 0) continue;

            INode* bn = ip->GetINodeByHandle(handles[bi]);
            if (bn == nullptr) continue;

            Control* tmc = bn->GetTMController();
            if (tmc == nullptr) continue;

            const xfbin::CoordNode& n = clump.nodes[bi];

            Control* posCtrl = tmc->GetPositionController();
            Control* rotCtrl = tmc->GetRotationController();
            Control* sclCtrl = tmc->GetScaleController();

            Point3 p(n.position[0], n.position[1], n.position[2]);

            const xfbin::Mat43 r = xfbin::MakeRotation(
                n.rotation[0], n.rotation[1], n.rotation[2]);
            Matrix3 m3(1);
            for (int row = 0; row < 3; ++row) {
                m3.SetRow(row, Point3(static_cast<float>(r.m[row][0]),
                                      static_cast<float>(r.m[row][1]),
                                      static_cast<float>(r.m[row][2])));
            }
            Quat q(m3);
            if (quatMode_ != 0) q = Conjugate(q);

            ScaleValue sv(Point3(n.scale[0], n.scale[1], n.scale[2]),
                          Quat(0.0f, 0.0f, 0.0f, 1.0f));

            for (int pass = 0; pass < 2; ++pass) {
                const TimeValue t = (pass == 0) ? t0 : t1;
                if (posCtrl) posCtrl->SetValue(t, &p,  TRUE, CTRL_ABSOLUTE);
                if (rotCtrl) rotCtrl->SetValue(t, &q,  TRUE, CTRL_ABSOLUTE);
                if (sclCtrl) sclCtrl->SetValue(t, &sv, TRUE, CTRL_ABSOLUTE);
            }

            ++keyed;
        }
    }

    AnimateOff();
    ResumeAnimate();

    return keyed;
}



// ------------------------------------------------------------
//  Einen Sichtbarkeits-Key schreiben - immer
//
//  INode::SetVisibility legt KEINEN Key an, wenn der Wert sich
//  nicht aendert. Fuer eine Animation, in der ein Mesh
//  durchgehend unsichtbar ist, entsteht damit gar nichts - und
//  genau die braucht der Warcraft-3-Export: je Sequenz einen
//  Key am Anfang und einen am Ende, auch wenn dazwischen nichts
//  passiert.
//
//  Deshalb der Weg ueber IKeyControl, wie ihn das Animation
//  Merge Tool fuer die Bones nimmt: der Key wird angehaengt,
//  ohne dass jemand den Wert vergleicht.
//
//  Die Tangenten stehen gleich auf BEZKEY_STEP. Sichtbarkeit
//  kennt kein Dazwischen - ein Objekt ist da oder nicht, und
//  Warcraft 3 hat fuer halb sichtbar keine Entsprechung.
// ------------------------------------------------------------
namespace {

IKeyControl* GetVisKeys(INode* node) {
    Control* vc = node->GetVisController();

    if (vc == nullptr) {
        vc = static_cast<Control*>(CreateInstance(
            CTRL_FLOAT_CLASS_ID, Class_ID(HYBRIDINTERP_FLOAT_CLASS_ID, 0)));
        if (vc == nullptr) return nullptr;
        node->SetVisController(vc);
    }

    return GetKeyControlInterface(vc);
}

void AppendVisKey(IKeyControl* ik, TimeValue t, float value) {
    if (ik == nullptr) return;

    IBezFloatKey k;
    memset(&k, 0, sizeof(k));

    k.time   = t;
    k.val    = value;
    k.intan  = 0.0f;
    k.outtan = 0.0f;
    k.inLength  = 0.0f;
    k.outLength = 0.0f;

    SetInTanType(k.flags,  BEZKEY_STEP);
    SetOutTanType(k.flags, BEZKEY_STEP);

    ik->AppendKey(&k);
}

} // namespace

// ------------------------------------------------------------
//  Sichtbarkeit aus der Deckkraft
//
//  Kanal 3 eines Bone-Eintrags ist die Deckkraft, und sie ist in
//  diesen Dateien das Mittel, mit dem Modelle erscheinen und
//  verschwinden. In den Zusatzanimationen von Pein wird
//  "2kyfbod1" in acht Animationen ein- und ausgeblendet, Pein
//  selbst in elf.
//
//  Ohne das steht jedes Modell in JEDER Sequenz sichtbar herum -
//  bei 104 Sequenzen und siebzehn Skeletten ein Bild, in dem der
//  halbe Bosskampf gleichzeitig auf der Matte steht.
//
//  Drei Faelle:
//    * Clump kommt in dieser Animation gar nicht vor
//        -> unsichtbar ueber die ganze Sequenz
//    * Mesh-Bone hat eine Deckkraft-Kurve
//        -> deren Verlauf als Sichtbarkeit
//    * Clump kommt vor, aber ohne Deckkraft-Kurve
//        -> sichtbar, mit Key an beiden Enden
//
//  Der Key an beiden Enden ist auch hier noetig: sonst blendet
//  Max zwischen zwei Sequenzen ueber.
// ------------------------------------------------------------
int XfbinImportInterface::BuildVisibility(int index, float startFrame,
                                          float endFrame) {
    if (!RequireAnims(L"buildVisibility")) return 0;
    if (index < 0 || index >= static_cast<int>(anims_.size())) return 0;
    // Ohne Bone-Eintraege wuerde present leer bleiben und ALLE
    // Meshes fuer diesen Abschnitt unsichtbar - bei Blur/Glare-
    // Clips aus spl1 genau der falsche Effekt, und unnoetige
    // Keys belasten die Szene.
    if (!anims_[static_cast<size_t>(index)].HasBoneEntries()) return 0;
    if (sceneMeshes_.empty()) return 0;

    Interface* ip = GetCOREInterface();
    if (ip == nullptr) return 0;

    const xfbin::Anm& anm = anims_[static_cast<size_t>(index)];
    const int tpf = GetTicksPerFrame();

    const TimeValue t0 =
        static_cast<TimeValue>(static_cast<double>(startFrame) * tpf + 0.5);
    const TimeValue t1 =
        static_cast<TimeValue>(static_cast<double>(endFrame) * tpf + 0.5);

    // Welche Clump-Namen spricht diese Animation ueberhaupt an?
    std::set<std::string> present;
    for (const xfbin::AnmClumpRef& c : anm.clumps) present.insert(c.clumpName);

    SceneRedrawGuard redrawGuard(ip);
    HoldSuspendGuard holdGuard;

    int touched = 0;
    int hidden  = 0;

    // Kein AnimateOn noetig: IKeyControl::AppendKey schreibt den
    // Key unabhaengig vom Animationsmodus.
    for (const SceneMesh& m : sceneMeshes_) {
        INode* node = ip->GetINodeByHandle(m.handle);
        if (node == nullptr) continue;

        IKeyControl* ik = GetVisKeys(node);
        if (ik == nullptr) continue;

        // --- Fall 1: gehoert nicht zu dieser Animation ---
        if (present.find(m.clumpName) == present.end()) {
            AppendVisKey(ik, t0, 0.0f);
            AppendVisKey(ik, t1, 0.0f);
            ++hidden;
            ++touched;
            continue;
        }

        // --- Fall 2: Deckkraft-Kurve des Mesh-Bones suchen ---
        const xfbin::AnmCurve* opacity = nullptr;

        for (const xfbin::AnmEntry& e : anm.entries) {
            if (e.entryFormat != xfbin::kEntryBone) continue;
            if (e.clumpName != m.clumpName) continue;
            if (e.targetName != m.boneName) continue;

            // Die richtige Instanz: so viele gleichnamige
            // Clump-Referenzen stehen vor dieser.
            int inst = 0;
            for (int ci = 0; ci < e.clumpIndex; ++ci) {
                if (static_cast<size_t>(ci) < anm.clumps.size() &&
                    anm.clumps[static_cast<size_t>(ci)].clumpName == e.clumpName) {
                    ++inst;
                }
            }
            if (inst != m.instance) continue;

            for (const xfbin::AnmCurve& c : e.curves) {
                if (xfbin::BoneChannelOf(c.curveIndex, c.curveFormat) ==
                        xfbin::kChannelOpacity && !c.keys.empty()) {
                    opacity = &c;
                }
            }
            break;
        }

        if (opacity != nullptr) {
            // Anfang und Ende festnageln, damit nichts in die
            // Nachbarsequenzen laeuft.
            AppendVisKey(ik, t0,
                static_cast<float>(opacity->keys.front().value[0]));

            for (const xfbin::AnmKey& k : opacity->keys) {
                const TimeValue t = static_cast<TimeValue>(
                    (k.frame + static_cast<double>(startFrame)) * tpf + 0.5);
                if (t > t0 && t < t1) {
                    AppendVisKey(ik, t, static_cast<float>(k.value[0]));
                }
            }

            AppendVisKey(ik, t1,
                static_cast<float>(opacity->keys.back().value[0]));
        } else {
            // --- Fall 3: dabei, aber ohne Kurve ---
            AppendVisKey(ik, t0, 1.0f);
            AppendVisKey(ik, t1, 1.0f);
        }

        ++touched;
    }

    // AppendKey verlangt aufsteigende Zeiten. Der Sequenzmodus
    // haelt das ein, aber wer eine Animation einzeln nachtraegt,
    // haelt es nicht ein - dann bringt Sortieren die Spur wieder
    // in Ordnung, statt sie stillschweigend zu verderben.
    for (const SceneMesh& m : sceneMeshes_) {
        INode* node = ip->GetINodeByHandle(m.handle);
        if (node == nullptr) continue;
        Control* vc = node->GetVisController();
        if (vc == nullptr) continue;
        IKeyControl* ik = GetKeyControlInterface(vc);
        if (ik != nullptr) ik->SortKeys();
    }

    wchar_t buf[192];
    swprintf_s(buf, L"buildVisibility '%s': %d Objekte, davon %d ausgeblendet",
               Cp932ToWide(anm.name).c_str(), touched, hidden);
    Log(buf);

    return touched;
}



// ------------------------------------------------------------
//  Material-Animationen
//
//  Die Datei animiert je Material achtzehn Groessen. In den
//  Testdaten sind fast alle davon einwertig, also konstant;
//  bewegt werden der Offset der ersten UV-Ebene und die
//  Deckkraft - klassisches UV-Scrollen fuer Augen, Haare und
//  Effektflaechen.
//
//  Uebertragen wird, was in Max eine Entsprechung hat:
//
//    U0/V0 Offset -> Offset der Bitmap (StdUVGen)
//    U0/V0 Scale  -> Kachelung der Bitmap
//    Alpha        -> Deckkraft des Materials
//
//  Nicht uebertragen, weil es dafuer nichts Entsprechendes
//  gibt: Glare, Falloff, BlendRate, OutlineID - das sind
//  Groessen des Shaders der Spiel-Engine. Sie werden gezaehlt
//  und gemeldet, damit klar ist, dass sie da sind und nicht
//  ankommen.
//
//  ------------------------------------------------------------
//  DAS VORZEICHEN VON V
//  ------------------------------------------------------------
//  Beim Import der Meshes wird die V-Achse gespiegelt (1-v),
//  weil Max die Textur andersherum aufzieht. Ein Versatz von
//  +dv in der Datei ist danach ein Versatz von -dv in Max:
//
//      v_max  = 1 - v_datei
//      v_max' = 1 - (v_datei + dv) = v_max - dv
//
//  Also wird der V-Offset negiert. Die Kachelung bleibt, wie
//  sie ist - bei einer gespiegelten Achse ist eine Skalierung
//  nicht mehr sauber uebertragbar, und in den Testdaten ist sie
//  ohnehin konstant.
// ------------------------------------------------------------
int XfbinImportInterface::BuildMaterialAnim(int index, float startFrame,
                                            float endFrame) {
    if (!RequireAnims(L"buildMaterialAnim")) return 0;
    if (index < 0 || index >= static_cast<int>(anims_.size())) return 0;
    if (!anims_[static_cast<size_t>(index)].HasBoneEntries()) return 0;
    if (sceneMaterials_.empty()) return 0;

    Interface* ip = GetCOREInterface();
    if (ip == nullptr) return 0;

    const xfbin::Anm& anm = anims_[static_cast<size_t>(index)];
    const int tpf = GetTicksPerFrame();

    HoldSuspendGuard holdGuard;

    int keysSet  = 0;
    int skipped  = 0;

    SuspendAnimate();
    AnimateOn();

    for (const xfbin::AnmEntry& e : anm.entries) {
        if (e.entryFormat != xfbin::kEntryMaterial) continue;

        auto it = sceneMaterials_.find(e.targetName);
        if (it == sceneMaterials_.end()) continue;

        Mtl* mtl = it->second;
        if (mtl == nullptr) continue;

        // Die Bitmap der Farbkarte - dort sitzen Offset und
        // Kachelung.
        StdUVGen* uvgen = nullptr;
        Texmap* tm = mtl->GetSubTexmap(ID_DI);
        if (tm != nullptr && tm->ClassID() == Class_ID(BMTEX_CLASS_ID, 0)) {
            BitmapTex* bmt = static_cast<BitmapTex*>(tm);
            UVGen* g = bmt->GetUVGen();
            if (g != nullptr && g->IsStdUVGen()) {
                uvgen = static_cast<StdUVGen*>(g);
            }
        }

        StdMat2* std2 = nullptr;
        if (mtl->ClassID() == Class_ID(DMTL_CLASS_ID, 0)) {
            std2 = static_cast<StdMat2*>(mtl);
        }

        for (const xfbin::AnmCurve& c : e.curves) {
            const xfbin::AnmMaterialChannel ch =
                xfbin::MaterialChannelOf(c.curveIndex);

            // Einwertige Kurven sind konstant - dafuer braucht es
            // keine Keys ueber die ganze Sequenz.
            const bool constant = (c.keys.size() <= 1);

            for (const xfbin::AnmKey& k : c.keys) {
                if (k.count < 1) continue;

                const TimeValue t = static_cast<TimeValue>(
                    (k.frame + static_cast<double>(startFrame)) * tpf + 0.5);

                const float v = static_cast<float>(k.value[0]);

                switch (ch) {
                case xfbin::kMatU0Offset:
                    if (uvgen) { uvgen->SetUOffs(v, t); ++keysSet; }
                    break;
                case xfbin::kMatV0Offset:
                    if (uvgen) { uvgen->SetVOffs(-v, t); ++keysSet; }
                    break;
                case xfbin::kMatU0Scale:
                    if (uvgen) { uvgen->SetUScl(v, t); ++keysSet; }
                    break;
                case xfbin::kMatV0Scale:
                    if (uvgen) { uvgen->SetVScl(v, t); ++keysSet; }
                    break;
                case xfbin::kMatAlpha:
                    if (std2) { std2->SetOpacity(v, t); ++keysSet; }
                    break;
                default:
                    if (!constant) ++skipped;
                    break;
                }
            }
        }
    }

    AnimateOff();
    ResumeAnimate();

    if (skipped > 0) {
        wchar_t wbuf[256];
        swprintf_s(wbuf, L"%d animierte Materialkurven ohne Entsprechung in Max "
                         L"(Glare, Falloff, BlendRate, OutlineID) - ausgelassen.",
                   skipped);
        AddWarning(wbuf);
    }

    return keysSet;
}

// ------------------------------------------------------------
//  Zuordnung Knoten -> Clump, als Text
//
//  Damit kann MaxScript die Layer bauen. Absichtlich nicht in
//  C++: die Layerverwaltung heisst dort ILayerManager und
//  ILayerProperties und ist umstaendlich, waehrend es in
//  MaxScript zwei benannte Aufrufe sind. Dieselbe Ueberlegung
//  wie bei der Bone-Groesse und beim Material-Editor.
//
//  Eine Zeile je Knoten:
//
//      <clump> TAB <instanz> TAB bone|mesh TAB <handle>
// ------------------------------------------------------------
const MCHAR* XfbinImportInterface::GetLayerReport() {
    scratch_.clear();

    std::wostringstream o;

    for (size_t ci = 0; ci < sceneClumps_.size(); ++ci) {
        if (ci >= boneHandles_.size()) break;

        const std::wstring cname = Cp932ToWide(sceneClumps_[ci].name);
        const int inst = (ci < sceneInstance_.size()) ? sceneInstance_[ci] : 0;

        for (ULONG h : boneHandles_[ci]) {
            if (h == 0) continue;
            o << cname << L"\t" << inst << L"\tbone\t" << h << L"\n";
        }
    }

    for (const SceneMesh& m : sceneMeshes_) {
        if (m.handle == 0) continue;
        o << Cp932ToWide(m.clumpName) << L"\t" << m.instance
          << L"\tmesh\t" << m.handle << L"\n";
    }

    scratch_ = o.str();
    return scratch_.c_str();
}

// ------------------------------------------------------------
//  Clump-Name der geoeffneten Datei
//
//  Nicht zu verwechseln mit sceneClumpName(): das beschreibt die
//  Szene, dies hier die Datei. Beim Import braucht man den
//  Dateinamen, BEVOR das Skelett angelegt ist - um die
//  Animationen zu fragen, wie viele Exemplare sie erwarten.
// ------------------------------------------------------------
const MCHAR* XfbinImportInterface::GetFileClumpName() {
    scratch_.clear();
    if (!RequireSkeleton(L"fileClumpName")) return scratch_.c_str();
    if (clumps_.empty()) return scratch_.c_str();
    scratch_ = Cp932ToWide(clumps_[0].name);
    return scratch_.c_str();
}

// ------------------------------------------------------------
//  Wie viele Exemplare eines Clumps die Animationen erwarten
//
//  Der Anim-Container listet je Animation seine Clumps auf. Taucht
//  derselbe Name mehrfach auf, traegt der Charakter das Modell
//  mehrfach - zwei gleiche Waffen etwa. Genommen wird das Maximum
//  ueber alle Animationen, damit auch eine Animation bedient ist,
//  die als einzige beide Exemplare bewegt.
// ------------------------------------------------------------
// Ohne Umweg ueber MCHAR - so kann buildSkeletonN die Zahl je
// Clump selbst bestimmen, statt sie von aussen gereicht zu
// bekommen.
int XfbinImportInterface::RequiredInstancesRaw(const std::string& want) {
    if (want.empty()) return 1;
    if (anims_.empty()) return 1;

    int best = 1;
    for (const xfbin::Anm& a : anims_) {
        int n = 0;
        for (const xfbin::AnmClumpRef& c : a.clumps) {
            if (c.clumpName == want) ++n;
        }
        if (n > best) best = n;
    }
    return best;
}

int XfbinImportInterface::RequiredInstances(const MCHAR* clumpName) {
    if (!RequireAnims(L"requiredInstances")) return 1;
    return RequiredInstancesRaw(WideToUtf8(SafeStr(clumpName)));
}

// ------------------------------------------------------------
//  Laenge einer Animation in Frames
// ------------------------------------------------------------
float XfbinImportInterface::AnimFrames(int index) {
    if (!RequireAnims(L"animFrames")) return 0.0f;
    if (index < 0 || index >= static_cast<int>(anims_.size())) return 0.0f;
    return static_cast<float>(anims_[static_cast<size_t>(index)].frameCount);
}

int XfbinImportInterface::AnimIsSkeletal(int index) {
    if (!RequireAnims(L"animIsSkeletal")) return 0;
    if (index < 0 || index >= static_cast<int>(anims_.size())) return 0;
    return anims_[static_cast<size_t>(index)].HasBoneEntries() ? 1 : 0;
}

// ------------------------------------------------------------
//  Name des Wurzel-Bones in der Szene
//
//  Der Sequenzmodus haengt den Note Track an diesen Knoten -
//  dorthin schaut auch NeoDex beim Export.
// ------------------------------------------------------------
const MCHAR* XfbinImportInterface::GetSceneRootName() {
    scratch_.clear();
    if (sceneClumps_.empty()) return scratch_.c_str();

    // Das Skelett mit den meisten Bones nehmen, nicht einfach
    // das erste. Sind Figur und Waffe geladen, entscheidet sonst
    // die alphabetische Reihenfolge der Dateien darueber, wo der
    // Note Track landet - und "1hakacc1" kommt vor "1hakbod1".
    size_t best = 0;
    for (size_t i = 1; i < sceneClumps_.size(); ++i) {
        if (sceneClumps_[i].nodes.size() > sceneClumps_[best].nodes.size()) {
            best = i;
        }
    }

    const xfbin::Clump& c = sceneClumps_[best];
    if (c.roots.empty()) return scratch_.c_str();

    const size_t idx = static_cast<size_t>(c.roots[0]);
    if (idx < c.nodes.size()) scratch_ = Cp932ToWide(c.nodes[idx].name);
    return scratch_.c_str();
}

// ------------------------------------------------------------
//  Ruhelage als Key setzen
//
//  Fuer den Sequenzmodus: auf Frame 0 steht die Bind-Pose, damit
//  ein Exporter eine definierte Ausgangslage vorfindet und die
//  erste Animation nicht aus dem Nichts einsetzt.
//
//  Genommen wird dieselbe lokale Lage, die auch buildSkeleton
//  benutzt - Position, Euler-Rotation und Skalierung aus dem
//  nuccChunkCoord. Damit ist der Key per Konstruktion genau die
//  Lage, in der das Skelett angelegt wurde.
// ------------------------------------------------------------
int XfbinImportInterface::BuildBindPoseKey(float frame) {
    if (sceneClumps_.empty() || boneHandles_.empty()) {
        SetError(L"buildBindPoseKey: es steht kein Skelett in der Szene.");
        return 0;
    }

    Interface* ip = GetCOREInterface();
    if (ip == nullptr) return 0;

    const int tpf = GetTicksPerFrame();
    const TimeValue t =
        static_cast<TimeValue>(static_cast<double>(frame) * tpf + 0.5);

    SceneRedrawGuard redrawGuard(ip);
    HoldSuspendGuard holdGuard;

    int keyed = 0;

    SuspendAnimate();
    AnimateOn();

    for (size_t ci = 0; ci < sceneClumps_.size(); ++ci) {
        const xfbin::Clump& clump = sceneClumps_[ci];
        if (ci >= boneHandles_.size()) break;
        const std::vector<ULONG>& handles = boneHandles_[ci];

        for (size_t bi = 0; bi < clump.nodes.size() && bi < handles.size(); ++bi) {
            if (handles[bi] == 0) continue;

            INode* bn = ip->GetINodeByHandle(handles[bi]);
            if (bn == nullptr) continue;

            Control* tmc = bn->GetTMController();
            if (tmc == nullptr) continue;

            const xfbin::CoordNode& n = clump.nodes[bi];

            Control* posCtrl = tmc->GetPositionController();
            Control* rotCtrl = tmc->GetRotationController();
            Control* sclCtrl = tmc->GetScaleController();

            if (posCtrl != nullptr) {
                Point3 p(n.position[0], n.position[1], n.position[2]);
                posCtrl->SetValue(t, &p, TRUE, CTRL_ABSOLUTE);
            }

            if (rotCtrl != nullptr) {
                const xfbin::Mat43 r = xfbin::MakeRotation(
                    n.rotation[0], n.rotation[1], n.rotation[2]);

                Matrix3 m3(1);
                for (int row = 0; row < 3; ++row) {
                    m3.SetRow(row, Point3(static_cast<float>(r.m[row][0]),
                                          static_cast<float>(r.m[row][1]),
                                          static_cast<float>(r.m[row][2])));
                }

                Quat q(m3);
                if (quatMode_ != 0) q = Conjugate(q);
                rotCtrl->SetValue(t, &q, TRUE, CTRL_ABSOLUTE);
            }

            if (sclCtrl != nullptr) {
                ScaleValue sv(Point3(n.scale[0], n.scale[1], n.scale[2]),
                              Quat(0.0f, 0.0f, 0.0f, 1.0f));
                sclCtrl->SetValue(t, &sv, TRUE, CTRL_ABSOLUTE);
            }

            ++keyed;
        }
    }

    // In der Ruhelage ist alles sichtbar. Ohne einen Key hier
    // haette der erste Sichtbarkeits-Key der ersten Sequenz
    // keinen Vorgaenger, und Max zoege seinen Wert bis Frame 0
    // zurueck - dann waere schon die Ausgangslage falsch.
    for (const SceneMesh& m : sceneMeshes_) {
        INode* mn = ip->GetINodeByHandle(m.handle);
        if (mn == nullptr) continue;
        AppendVisKey(GetVisKeys(mn), t, 1.0f);
    }

    AnimateOff();
    ResumeAnimate();

    wchar_t buf[192];
    swprintf_s(buf, L"buildBindPoseKey: %d Bones auf Frame %.0f",
               keyed, static_cast<double>(frame));
    Log(buf);

    return keyed;
}

// ============================================================
//  Szenenzustand
//
//  Diese drei Funktionen gibt es, weil "was steht in der Datei"
//  und "was steht in der Szene" zwei verschiedene Dinge sind.
//  Die Oberflaeche braucht den Unterschied, um zu entscheiden,
//  ob "Animation setzen" ueberhaupt sinnvoll ist.
// ============================================================

int XfbinImportInterface::SceneBoneCount() {
    size_t n = 0;
    for (const std::vector<ULONG>& h : boneHandles_) n += h.size();
    return static_cast<int>(n);
}

const MCHAR* XfbinImportInterface::GetSceneClumpName() {
    scratch_.clear();
    if (sceneClumps_.empty()) return scratch_.c_str();

    for (const xfbin::Clump& c : sceneClumps_) {
        if (!scratch_.empty()) scratch_ += L", ";
        scratch_ += Cp932ToWide(c.name);
    }
    return scratch_.c_str();
}

int XfbinImportInterface::ClearScene() {
    // Loescht nur die Buchfuehrung, nicht die Knoten. Wer die
    // Knoten loeschen will, macht das in Max - hier waere es
    // eine Ueberraschung.
    //
    // Wichtig: auch meshNodes_ und sceneMaterials_ leeren.
    // Nach "delete objects" sind die alten Mtl*-Zeiger und
    // Mesh-Handles tot. Ein zweiter Import mit buildMaterials
    // ohne diesen Reset endet in ACCESS_VIOLATION (Null-Deref
    // auf verwaisten Materialzeigern).
    const int had = SceneBoneCount();
    sceneClumps_.clear();
    sceneInstance_.clear();
    boneHandles_.clear();
    sceneMeshes_.clear();
    sceneMaterials_.clear();
    meshNodes_.clear();
    Log(L"clearScene: Szenenzustand vergessen");
    return had;
}


// ------------------------------------------------------------
//  Diagnose: welches Objekt ist zu gross?
//
//  Liefert eine Textliste der groessten Objekte am aktuellen
//  Frame, dazu ihre Skalierung, ihr Elternteil und ob ein
//  Skin-Modifier drauf sitzt.
//
//  Der Punkt: eine Ueberdehnung, die erst MIT der Animation
//  auftritt und in der Bind-Pose nicht, ist fast immer ein
//  Skinning-Problem - falsch zugeordnete Gewichte fallen in
//  Ruhelage nicht auf, weil dort nichts verformt wird. Deshalb
//  steht hier neben der Groesse auch, ob und mit wie vielen
//  Bones das Objekt geskinnt ist.
// ------------------------------------------------------------
const MCHAR* XfbinImportInterface::GetSceneReport() {
    scratch_.clear();

    Interface* ip = GetCOREInterface();
    if (ip == nullptr) return scratch_.c_str();

    const TimeValue t = ip->GetTime();

    struct Row {
        double size = 0.0;
        std::wstring name;
        Point3 bb;
        Point3 scale;
        std::wstring parent;
        int skinBones = -1;
    };
    std::vector<Row> rows;

    INode* root = ip->GetRootNode();

    // Rekursion ohne Rekursion - ein Stapel genuegt und kann
    // nicht ueberlaufen.
    std::vector<INode*> stack;
    for (int i = 0; i < root->NumberOfChildren(); ++i) {
        stack.push_back(root->GetChildNode(i));
    }

    while (!stack.empty()) {
        INode* n = stack.back();
        stack.pop_back();
        for (int i = 0; i < n->NumberOfChildren(); ++i) {
            stack.push_back(n->GetChildNode(i));
        }

        const ObjectState os = n->EvalWorldState(t);
        if (os.obj == nullptr) continue;

        // GetObjectTM liefert eine Matrix3 ALS WERT. Die Adresse
        // eines Temporaries gibt es nicht - deshalb erst in eine
        // lokale Variable, dann deren Adresse. (C2102: "&"
        // erwartet L-Wert)
        Matrix3 otm = n->GetObjectTM(t);

        Box3 bb;
        os.obj->GetDeformBBox(t, bb, &otm);

        Row r;
        r.bb   = bb.Max() - bb.Min();
        r.size = static_cast<double>(
            (r.bb.x > r.bb.y ? (r.bb.x > r.bb.z ? r.bb.x : r.bb.z)
                             : (r.bb.y > r.bb.z ? r.bb.y : r.bb.z)));
        r.name = n->GetName();

        Matrix3 tm = n->GetNodeTM(t);
        Point3 sc(Length(tm.GetRow(0)), Length(tm.GetRow(1)), Length(tm.GetRow(2)));
        r.scale = sc;

        INode* par = n->GetParentNode();
        if (par != nullptr && !par->IsRootNode()) {
            r.parent = par->GetName();
        } else {
            r.parent = L"-";
        }

        // Skin-Modifier suchen
        Object* obj = n->GetObjectRef();
        while (obj && obj->SuperClassID() == GEN_DERIVOB_CLASS_ID) {
            IDerivedObject* d = static_cast<IDerivedObject*>(obj);
            for (int m = 0; m < d->NumModifiers(); ++m) {
                Modifier* mod = d->GetModifier(m);
                if (mod && mod->ClassID() == SKIN_CLASSID) {
                    ISkin* sk = static_cast<ISkin*>(mod->GetInterface(I_SKIN));
                    r.skinBones = sk ? sk->GetNumBones() : 0;
                }
            }
            obj = d->GetObjRef();
        }

        rows.push_back(std::move(r));
    }

    std::sort(rows.begin(), rows.end(),
              [](const Row& a, const Row& b) { return a.size > b.size; });

    std::wostringstream o;
    o.imbue(std::locale::classic());
    o << L"Frame " << (t / GetTicksPerFrame()) << L", "
      << rows.size() << L" Objekte\n";
    o << L"Die groessten (ein Charakter ist rund 180 Einheiten hoch):\n";

    const size_t show = rows.size() < 12 ? rows.size() : 12;
    for (size_t i = 0; i < show; ++i) {
        const Row& r = rows[i];

        // Eigene Variable statt eines ternaeren Ausdrucks mit
        // to_wstring(...).c_str(): so ist die Lebensdauer des
        // Strings offensichtlich richtig und nicht nur zufaellig.
        const std::wstring skinText =
            (r.skinBones < 0) ? std::wstring(L"nein")
                              : std::to_wstring(r.skinBones);

        wchar_t buf[512];
        swprintf_s(buf,
                   L"  %7.0f  %-28s  bbox=%.0f/%.0f/%.0f  scale=%.2f/%.2f/%.2f"
                   L"  skin=%s  parent=%s\n",
                   r.size, r.name.c_str(),
                   static_cast<double>(r.bb.x), static_cast<double>(r.bb.y),
                   static_cast<double>(r.bb.z),
                   static_cast<double>(r.scale.x), static_cast<double>(r.scale.y),
                   static_cast<double>(r.scale.z),
                   skinText.c_str(),
                   r.parent.c_str());
        o << buf;
    }

    scratch_ = o.str();
    return scratch_.c_str();
}


// ============================================================
//  Stufe 4: Texturen und Materialien
// ============================================================

// Class-IDs der Standardmaterialien. Wie bei den Bones: das sind
// Standard-Defines, aber je nach SDK-Version in unterschiedlichen
// Headern. Die Werte unten sind die offiziellen aus plugapi.h.
#ifndef DMTL_CLASS_ID
#define DMTL_CLASS_ID 0x00000002
#endif
#ifndef MULTI_CLASS_ID
#define MULTI_CLASS_ID 0x00000200
#endif
#ifndef BMTEX_CLASS_ID
#define BMTEX_CLASS_ID 0x00000240
#endif

bool XfbinImportInterface::RequireTextures(const wchar_t* what) {
    if (!RequireFile(what)) return false;
    if (texturesParsed_) return true;
    return (ParseTextures() >= 0);
}

int XfbinImportInterface::ParseTextures() {
    if (!RequireFile(L"parseTextures")) return -1;

    textures_.clear();
    materials_.clear();
    textureFiles_.clear();
    texturesParsed_ = false;

    std::string err1, warn1, err2, warn2;

    Stopwatch sw;
    xfbin::ParseTextures(*file_, textures_, err1, warn1);
    xfbin::ParseMaterials(*file_, materials_, err2, warn2);
    msTex_ = sw.ms();

    for (const std::string* w : { &warn1, &warn2 }) {
        if (w->empty()) continue;
        std::istringstream in(*w);
        std::string line;
        while (std::getline(in, line)) AddWarning(Utf8ToWide(line));
    }

    if (textures_.empty() && materials_.empty()) {
        SetError(Utf8ToWide(err1));
        return -1;
    }

    texturesParsed_ = true;
    textureFiles_.assign(textures_.size(), std::string());

    wchar_t buf[256];
    swprintf_s(buf, L"parseTextures: %zu Texturen, %zu Materialien in %.1f ms",
               textures_.size(), materials_.size(), msTex_);
    Log(buf);

    return static_cast<int>(textures_.size());
}

int XfbinImportInterface::TextureCount() {
    if (!RequireTextures(L"textureCount")) return -1;
    return static_cast<int>(textures_.size());
}

int XfbinImportInterface::MaterialCount() {
    if (!RequireTextures(L"materialCount")) return -1;
    return static_cast<int>(materials_.size());
}

const MCHAR* XfbinImportInterface::GetTextureSummary() {
    scratch_.clear();
    if (!RequireTextures(L"textureSummary")) return scratch_.c_str();
    scratch_ = Cp932ToWide(xfbin::MakeTextureSummary(textures_, materials_));
    return scratch_.c_str();
}

int XfbinImportInterface::ExportTextures(const MCHAR* directory) {
    if (!RequireTextures(L"exportTextures")) return 0;

    const std::wstring wdir = SafeStr(directory);
    if (wdir.empty()) {
        SetError(L"exportTextures: leerer Ordner.");
        return 0;
    }

    std::string warn;
    const int n = xfbin::ExportTextures(textures_, WideToUtf8(wdir),
                                        textureFiles_, warn);

    if (!warn.empty()) {
        std::istringstream in(warn);
        std::string line;
        while (std::getline(in, line)) AddWarning(Utf8ToWide(line));
    }

    wchar_t buf[320];
    swprintf_s(buf, L"exportTextures: %d DDS nach %s", n, wdir.c_str());
    Log(buf);

    return n;
}

// ------------------------------------------------------------
//  Material aus einem Chunk bauen
// ------------------------------------------------------------
namespace {

Mtl* MakeStandardMaterial(Interface* ip, const std::wstring& name,
                          const std::wstring& mapFile) {
    StdMat2* mtl = static_cast<StdMat2*>(
        ip->CreateInstance(MATERIAL_CLASS_ID, Class_ID(DMTL_CLASS_ID, 0)));
    if (mtl == nullptr) return nullptr;

    mtl->SetName(const_cast<MCHAR*>(name.c_str()));

    // Cel-Shading-Modelle bringen ihre Beleuchtung in der Textur
    // mit. Glanzlichter aus, sonst liegt ein Spiegelfleck auf
    // einem Bild, das schon eine Schattierung enthaelt.
    mtl->SetShinStr(0.0f, 0);
    mtl->SetShininess(0.0f, 0);

    if (!mapFile.empty()) {
        BitmapTex* bmt = static_cast<BitmapTex*>(
            ip->CreateInstance(TEXMAP_CLASS_ID, Class_ID(BMTEX_CLASS_ID, 0)));

        if (bmt != nullptr) {
            bmt->SetMapName(const_cast<MCHAR*>(mapFile.c_str()));
            bmt->SetName(const_cast<MCHAR*>(name.c_str()));

            // Ohne das bleibt die Bitmap ungeladen: SetMapName
            // merkt sich nur den Pfad. In den Beispielen von
            // Autodesk steht der Aufruf direkt dahinter, und ohne
            // ihn zeigt der Material-Editor eine leere Vorschau,
            // bis man die Datei von Hand neu waehlt.
            bmt->ReloadBitmapAndUpdate();

            // Ab hier ist die Bitmap geladen; das Material soll sie
            // auch im Viewport zeigen.

            mtl->SetSubTexmap(ID_DI, bmt);
            mtl->EnableMap(ID_DI, TRUE);
            mtl->SetTexmapAmt(ID_DI, 1.0f, 0);

            // Im Viewport anzeigen - ohne das bleibt das Modell
            // grau und man haelt den Import fuer gescheitert.
            mtl->SetMtlFlag(MTL_TEX_DISPLAY_ENABLED, TRUE);
            mtl->SetActiveTexmap(bmt);
        }
    }

    return mtl;
}

} // namespace

int XfbinImportInterface::BuildMaterials(const MCHAR* directory) {
    if (!RequireTextures(L"buildMaterials")) return 0;
    if (!RequireMeshes(L"buildMaterials")) return 0;

    if (meshNodes_.empty()) {
        SetError(L"buildMaterials: es sind keine Meshes angelegt - "
                 L"erst importieren.");
        return 0;
    }

    Interface* ip = GetCOREInterface();
    if (ip == nullptr) return 0;

    const std::wstring wdir = SafeStr(directory);

    SceneRedrawGuard redrawGuard(ip);
    HoldSuspendGuard holdGuard;

    Stopwatch sw;

    // Ein Material kann von mehreren Modellen benutzt werden -
    // in dieser Datei teilen sich acht Modelle das Material
    // "1hakbody1". Also einmal bauen, dann wiederverwenden.
    std::map<std::string, Mtl*>& byName = sceneMaterials_;

    int assigned = 0;
    int missing  = 0;
    std::wstring firstMissing;

    for (const MeshRef& ref : meshNodes_) {
        if (ref.modelIndex >= models_.size()) continue;

        INode* node = ip->GetINodeByHandle(ref.handle);
        if (node == nullptr) continue;

        const xfbin::NudModel& model = models_[ref.modelIndex];

        const std::vector<xfbin::RawString> matNames =
            xfbin::ResolveModelMaterials(*file_, model.pageIndex,
                                         model.materialIndices);
        if (matNames.empty()) continue;

        // Die Material-ID eines Submeshes ist sein Index in der
        // Gruppe. Gibt es mehr Submeshes als Materialien, zeigt
        // eine ID ins Leere - Max faellt dann auf einen leeren
        // Platz zurueck. Die SDK-Doku weist ausdruecklich darauf
        // hin, dass das vorkommen kann.
        for (const xfbin::NudMeshGroup& g : model.groups) {
            if (g.meshes.size() > matNames.size()) {
                wchar_t wbuf[320];
                swprintf_s(wbuf, L"Modell '%s': %zu Submeshes, aber nur %zu "
                                 L"Materialien - die ueberzaehligen bleiben "
                                 L"ohne.",
                           Cp932ToWide(model.name).c_str(),
                           g.meshes.size(), matNames.size());
                AddWarning(wbuf);
                break;
            }
        }

        std::vector<Mtl*> subs;
        subs.reserve(matNames.size());

        for (const xfbin::RawString& mn : matNames) {
            auto it = byName.find(mn);
            if (it != byName.end()) {
                // Verwaiste Zeiger nach Scene-Clear/delete objects
                // nicht wiederverwenden. clearScene() leert die Map,
                // aber zur Sicherheit auch hier null pruefen.
                if (it->second != nullptr) {
                    subs.push_back(it->second);
                    continue;
                }
                byName.erase(it);
            }

            // Textur des Materials suchen und die dazu
            // geschriebene DDS-Datei nachschlagen.
            std::wstring mapFile;
            xfbin::RawString texName;

            for (const xfbin::XfbinMaterial& m : materials_) {
                if (m.name != mn) continue;
                texName = m.DiffuseTexture();
                break;
            }

            if (!texName.empty()) {
                bool found = false;
                for (size_t ti = 0; ti < textures_.size(); ++ti) {
                    if (textures_[ti].name != texName) continue;
                    if (ti < textureFiles_.size() && !textureFiles_[ti].empty()) {
                        mapFile = wdir + L"\\" + Utf8ToWide(textureFiles_[ti]);
                        found = true;
                    }
                    break;
                }

                // Referenzierte Texturen liegen nicht immer in
                // derselben Datei - "celshade" etwa kommt aus
                // einem gemeinsamen XFBIN. Das ist kein Fehler,
                // aber es gehoert gemeldet.
                if (!found) {
                    ++missing;
                    if (firstMissing.empty()) firstMissing = Cp932ToWide(texName);
                }
            }

            Mtl* built = MakeStandardMaterial(ip, Cp932ToWide(mn), mapFile);
            if (built == nullptr) {
                AddWarning(L"buildMaterials: CreateInstance fuer Standardmaterial "
                           L"fehlgeschlagen.");
                continue;
            }
            byName[mn] = built;
            subs.push_back(built);
        }

        Mtl* result = nullptr;

        if (subs.size() == 1) {
            result = subs[0];
        } else if (subs.size() > 1) {
            // Multi/Sub-Object. Die Material-ID der Faces ist der
            // Index des Submeshes in seiner Gruppe - genau die
            // Reihenfolge, in der die Materialien hier stehen.
            //
            // Die Anzahl der Unterplaetze wird NICHT gesetzt: das
            // Standardmaterial bringt zehn mit, wir belegen die
            // ersten. Ein ungenutzter Platz stoert nicht, das
            // Antasten des Parameterblocks schon.
            Mtl* multi = static_cast<Mtl*>(
                ip->CreateInstance(MATERIAL_CLASS_ID, Class_ID(MULTI_CLASS_ID, 0)));

            if (multi != nullptr) {
                const int slots = multi->NumSubMtls();
                for (size_t i = 0; i < subs.size(); ++i) {
                    if (static_cast<int>(i) < slots && subs[i] != nullptr) {
                        multi->SetSubMtl(static_cast<int>(i), subs[i]);
                    }
                }
                multi->SetName(const_cast<MCHAR*>(
                    Cp932ToWide(model.name).c_str()));
                result = multi;
            } else {
                result = subs[0];
            }
        }

        if (result != nullptr) {
            node->SetMtl(result);
            ++assigned;
        }
    }

    msTex_ += sw.ms();

    if (missing > 0) {
        wchar_t wbuf[320];
        swprintf_s(wbuf, L"%d Materialien verweisen auf Texturen, die nicht in "
                         L"dieser Datei liegen (erste: '%s') - sie kommen aus "
                         L"einem gemeinsamen XFBIN.",
                   missing, firstMissing.c_str());
        AddWarning(wbuf);
    }

    wchar_t buf[256];
    swprintf_s(buf, L"buildMaterials: %d Objekte in %.1f ms", assigned, msTex_);
    Log(buf);

    ip->RedrawViews(ip->GetTime());

    return assigned;
}

// ============================================================
//  UtilityObj
// ============================================================

void XfbinImportPlugin::BeginEditParams(Interface* ip, IUtil* iu) {
    ip_ = ip;
    iu_ = iu;

    // Info in den Listener statt in eine modale MessageBox - die
    // wuerde bei jedem Oeffnen des Panels blockieren.
    RunScript(
        std::wstring(L"format \"\\n=== XFBIN Import ") + XFBINIMPORT_VERSION_STR +
        L" ===\\n\"\n"
        L"format \"MaxScript-Funktionen:\\n\"\n"
        L"format \"  XfbinCpp.open <path> / .close() / .isOpen()\\n\"\n"
        L"format \"  XfbinCpp.dump <outPath> <includeTables>\\n\"\n"
        L"format \"  XfbinCpp.pageCount() / .chunkCount()\\n\"\n"
        L"format \"  XfbinCpp.countOfType <type> / .namesOfType <type>\\n\"\n"
        L"format \"  XfbinCpp.summary()\\n\"\n"
        L"format \"  XfbinCpp.version() / .lastError() / .warnings()\\n\"\n"
        L"format \"  XfbinCpp.log() / .timings() / .setDebug <0|1>\\n\"\n"
        L"format \"  XfbinCpp.parseSkeleton() / .clumpCount() / .boneCount()\\n\"\n"
        L"format \"  XfbinCpp.boneSummary() / .boneDump <outPath>\\n\"\n"
        L"format \"  XfbinCpp.buildSkeleton <mode> <scale>\\n\"\n"
        L"format \"  XfbinCpp.parseMeshes() / .modelCount() / .meshSummary()\\n\"\n"
        L"format \"  XfbinCpp.meshDump <outPath> <withVerts>\\n\"\n"
        L"format \"  XfbinCpp.buildMeshes <skipLod> <normals> <scale>\\n\"\n"
        L"format \"  XfbinCpp.parseAnims() / .animCount() / .animName <i>\\n\"\n"
        L"format \"  XfbinCpp.animSummary() / .animDump <outPath> <withKeys>\\n\"\n"
        L"format \"  XfbinCpp.buildAnim <index> <scale> / .setQuatMode <0|1>\\n\\n\"\n");
}

void XfbinImportPlugin::EndEditParams(Interface* ip, IUtil* iu) {
    UNREFERENCED_PARAMETER(ip);
    UNREFERENCED_PARAMETER(iu);
    ip_ = nullptr;
    iu_ = nullptr;
}
