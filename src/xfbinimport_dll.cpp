// ============================================================
//  XFBIN Import - DLL Entry
//
//  Hinweis zu den Exports: Die Funktionen sind NICHT extern "C",
//  deshalb exportiert __declspec(dllexport) den dekorierten Namen.
//  3ds Max sucht aber den undekorierten. Die xfbinimport.def sorgt
//  dafuer, dass beide Namen sauber herauskommen - sie darf NICHT
//  entfernt werden, auch wenn das doppelt gemoppelt aussieht.
// ============================================================

#include "xfbinimport.h"

// ClassDesc2 steht in iparamb2.h. Die neueren SDKs ziehen den
// Header ueber max.h mit herein, 2016 nicht - dort fehlt sonst die
// Basisklasse, und jede override-Methode meldet danach einen
// eigenen Fehler.
#include <iparamb2.h>

// SceneImport steht in impexp.h, RegisterNotification und die
// NOTIFY_*-Konstanten in notify.h. Die neueren SDKs ziehen beide
// ueber max.h mit, das von 2016 nicht.
#include <impexp.h>
#include <notify.h>

#include <string>

HINSTANCE hInstance = nullptr;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        hInstance = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}

static XfbinImportPlugin thePlugin;

class XfbinImportClassDesc : public ClassDesc2 {
public:
    int          IsPublic() override                { return TRUE; }
    void*        Create(BOOL) override              { return &thePlugin; }
    const MCHAR* ClassName() override               { return _T("XfbinImport"); }

    // NonLocalizedClassName gibt es erst ab Max 2022. Davor hat
    // ClassDesc die Methode nicht, und override waere ein Fehler.
#if defined(MAX_RELEASE) && (MAX_RELEASE >= 24000)
    const MCHAR* NonLocalizedClassName() override   { return _T("XfbinImport"); }
#endif
    SClass_ID    SuperClassID() override            { return UTILITY_CLASS_ID; }
    Class_ID     ClassID() override                 { return XFBINIMPORT_CLASS_ID; }
    const MCHAR* Category() override                { return _T("Import"); }
    const MCHAR* InternalName() override            { return _T("XfbinImport"); }
    HINSTANCE    HInstance() override               { return hInstance; }
};

static XfbinImportClassDesc theClassDesc;

// ============================================================
//  Eintrag in Max' eigenem Import-Dialog
//
//  Bisher lief alles ueber das Menue. Wer "File -> Import"
//  benutzt, findet XFBIN dort nicht - und sucht dann in einer
//  Liste, in der es nicht steht.
//
//  Eine SceneImport-Klasse traegt das Format ein. Der Dialog
//  laesst genau EINE Datei waehlen, und das ist fuer XFBIN zu
//  wenig: ein Charakter besteht aus Modell-, Zubehoer- und
//  Animationsdateien. Deshalb nimmt DoImport nur den ORDNER der
//  gewaehlten Datei und uebergibt ihn an die vorhandene
//  Oberflaeche - dieselbe, die das Menue oeffnet, mit dem Ordner
//  schon eingetragen und durchsucht.
//
//  Wer also irgendeine .xfbin des Charakters waehlt, bekommt
//  genau den gewohnten Ablauf.
// ============================================================

XfbinImportInterface* GetXfbinInterface();

class XfbinSceneImport : public SceneImport {
public:
    int ExtCount() override                 { return 1; }
    const MCHAR* Ext(int i) override        { return (i == 0) ? _T("xfbin") : _T(""); }

    // ShortDesc ist das, was in der Formatliste des
    // Import-Dialogs steht - Max haengt die Endung selbst an:
    //
    //     Ninja Storm XFBIN (*.xfbin)
    //
    // Das Format gehoert CyberConnect2 und steckt auch in
    // JoJo's Bizarre Adventure und anderen Titeln. Wer danach
    // sucht, findet unter "Ninja Storm" nichts - deshalb steht
    // CC2 in der langen Beschreibung, die Max in der Statuszeile
    // und im Plugin Manager zeigt.
    //
    // Wer lieber beides in der Liste haette:
    //     return _T("Ninja Storm / CC2 XFBIN");
    const MCHAR* ShortDesc() override       { return _T("Ninja Storm XFBIN"); }

    const MCHAR* LongDesc() override {
        return _T("CyberConnect2 XFBIN - Naruto Ultimate Ninja Storm "
                  "(Modell, Animationen, Texturen)");
    }
    const MCHAR* AuthorName() override      { return _T("DennisH"); }
    const MCHAR* CopyrightMessage() override { return _T(""); }
    const MCHAR* OtherMessage1() override   { return _T(""); }
    const MCHAR* OtherMessage2() override   { return _T(""); }

    unsigned int Version() override         { return 216; }
    void ShowAbout(HWND) override           {}

    int DoImport(const MCHAR* name, ImpInterface*, Interface*,
                 BOOL suppressPrompts) override {
        if (name == nullptr) return 0;

        // Ordner aus dem Dateipfad ziehen.
        std::wstring path(name);
        const size_t slash = path.find_last_of(L"/\\");
        std::wstring dir = (slash == std::wstring::npos)
                               ? std::wstring()
                               : path.substr(0, slash);

        XfbinImportInterface* iface = GetXfbinInterface();
        if (iface == nullptr) return 0;

        iface->SetPendingFolder(dir);

        // suppressPrompts ist gesetzt, wenn Max ohne Rueckfragen
        // importieren soll - etwa aus einem Skript heraus. Dann
        // waere ein Fenster falsch; der Ordner bleibt hinterlegt
        // und die Oberflaeche holt ihn beim naechsten Oeffnen ab.
        if (suppressPrompts) return 1;

        iface->RunMacro();
        return 1;
    }
};

class XfbinSceneImportClassDesc : public ClassDesc2 {
public:
    int IsPublic() override { return TRUE; }

    // ------------------------------------------------------
    //  new, NICHT ein statisches Objekt.
    //
    //  Das Singleton-Muster, das die Autodesk-Doku zeigt, gilt
    //  fuer UTILITY-Plugins - von denen gibt es genau eines.
    //  SceneImport ist ein anderer Fall: Max legt eine Instanz
    //  an, fragt ExtCount, Ext und die Beschreibungen ab und
    //  gibt den Speicher danach wieder frei. Die Klasse hat kein
    //  DeleteThis, also loescht Max direkt.
    //
    //  Ein statisches Objekt zu loeschen zerlegt den Heap. Genau
    //  deshalb ist 3ds Max 2027 beim Oeffnen des Import-Dialogs
    //  abgestuerzt - noch bevor irgendetwas importiert wurde.
    // ------------------------------------------------------
    void* Create(BOOL) override { return new XfbinSceneImport(); }
    const MCHAR* ClassName() override               { return _T("XFBIN Import"); }
#if defined(MAX_RELEASE) && (MAX_RELEASE >= 24000)
    const MCHAR* NonLocalizedClassName() override   { return _T("XFBIN Import"); }
#endif
    SClass_ID    SuperClassID() override            { return SCENE_IMPORT_CLASS_ID; }
    Class_ID     ClassID() override                 { return XFBINIMPORT_SCENE_CLASS_ID; }
    const MCHAR* Category() override                { return _T("Import"); }
    const MCHAR* InternalName() override            { return _T("XfbinSceneImport"); }
    HINSTANCE    HInstance() override               { return hInstance; }
};

static XfbinSceneImportClassDesc theSceneImportClassDesc;


// ============================================================
//  Die eingebettete Oberflaeche registrieren
//
//  Die .mcr liegt als RCDATA in der DLL. Beim Start wird sie in
//  den Temp-Ordner geschrieben und eingelesen - danach ist das
//  Makro registriert, ohne dass eine Datei installiert sein
//  muesste.
//
//  Warum ueber eine Datei und nicht direkt aus dem Speicher:
//  MAXScript liest Makroskripte aus Dateien; ein macroScript
//  aus einer Zeichenkette heraus zu definieren geht zwar, aber
//  fileIn ist der Weg, den Max selbst benutzt, und er meldet
//  Fehler an derselben Stelle wie sonst auch.
//
//  Eine INSTALLIERTE Datei hat Vorrang. Wer die .mcr von Hand
//  aendert, um etwas auszuprobieren, soll seine Fassung sehen
//  und nicht die eingebackene - sonst sucht man den Fehler an
//  der falschen Stelle.
// ============================================================

namespace {

const int kMcrResourceId = 1000;

std::wstring WriteEmbeddedScript() {
    HRSRC res = FindResource(hInstance, MAKEINTRESOURCE(kMcrResourceId), RT_RCDATA);
    if (res == nullptr) return std::wstring();

    HGLOBAL handle = LoadResource(hInstance, res);
    if (handle == nullptr) return std::wstring();

    const void* data = LockResource(handle);
    const DWORD size = SizeofResource(hInstance, res);
    if (data == nullptr || size == 0) return std::wstring();

    wchar_t tempDir[MAX_PATH] = { 0 };
    if (GetTempPathW(MAX_PATH, tempDir) == 0) return std::wstring();

    std::wstring path = std::wstring(tempDir) + L"XfbinImport_embedded.mcr";

    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return std::wstring();

    DWORD written = 0;
    const BOOL ok = WriteFile(f, data, size, &written, nullptr);
    CloseHandle(f);

    if (!ok || written != size) return std::wstring();
    return path;
}

bool InstalledScriptExists() {
    // %APPDATA%\Autodesk\ApplicationPlugins\XfbinImport\Contents\
    //   MacroScripts\XfbinImport.mcr
    wchar_t appdata[MAX_PATH] = { 0 };
    const DWORD n = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;

    const std::wstring path = std::wstring(appdata) +
        L"\\Autodesk\\ApplicationPlugins\\XfbinImport\\Contents"
        L"\\MacroScripts\\XfbinImport.mcr";

    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void OnStartup(void*, NotifyInfo*) {
    if (InstalledScriptExists()) {
        // Die installierte Fassung wird von Max ohnehin geladen.
        return;
    }

    const std::wstring path = WriteEmbeddedScript();
    if (path.empty()) return;

    std::wstring cmd = L"try (fileIn @\"" + path + L"\") catch "
                       L"(format \"[XFBIN] Eingebettete Oberflaeche liess sich "
                       L"nicht laden: %\\n\" (getCurrentException()))";

    XfbinImportInterface* iface = GetXfbinInterface();
    if (iface != nullptr) iface->RunScriptText(cmd);
}

} // namespace

__declspec(dllexport) const TCHAR* LibDescription() {
    return _T("XFBIN Import ") XFBINIMPORT_VERSION_STR
           _T(" - CyberConnect2 XFBIN Importer");
}
__declspec(dllexport) int        LibNumberClasses()  { return 2; }

__declspec(dllexport) ClassDesc* LibClassDesc(int i) {
    switch (i) {
    case 0:  return &theClassDesc;
    case 1:  return &theSceneImportClassDesc;
    default: return nullptr;
    }
}
__declspec(dllexport) ULONG      LibVersion()        { return VERSION_3DSMAX; }
__declspec(dllexport) int        CanAutoDefer()      { return FALSE; }

// LibInitialize laeuft, bevor MAXScript bereit ist - deshalb wird
// das Einlesen auf den Systemstart verschoben.
__declspec(dllexport) int LibInitialize() {
    RegisterNotification(OnStartup, nullptr, NOTIFY_SYSTEM_STARTUP);
    return TRUE;
}

__declspec(dllexport) int LibShutdown() {
    UnRegisterNotification(OnStartup, nullptr, NOTIFY_SYSTEM_STARTUP);
    return TRUE;
}
