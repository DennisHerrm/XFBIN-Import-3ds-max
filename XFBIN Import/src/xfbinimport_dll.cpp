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

__declspec(dllexport) const TCHAR* LibDescription() {
    return _T("XFBIN Import ") XFBINIMPORT_VERSION_STR
           _T(" - CyberConnect2 XFBIN Importer");
}
__declspec(dllexport) int        LibNumberClasses()  { return 1; }
__declspec(dllexport) ClassDesc* LibClassDesc(int i) { return (i == 0) ? &theClassDesc : nullptr; }
__declspec(dllexport) ULONG      LibVersion()        { return VERSION_3DSMAX; }
__declspec(dllexport) int        CanAutoDefer()      { return FALSE; }
