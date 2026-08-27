---
name: xfbin-build-ship
description: >-
  Builds, verifies, and installs the XFBIN Import 3ds Max plugin from source.
  Use when rebuilding DLUs, bumping version, running INSTALLIERE.bat, or
  validating before release.
---

# XFBIN build and ship

## Prerequisites

- Visual Studio 2022+ with C++ workload
- 3ds Max SDK installed (default: `C:\Program Files\Autodesk\3ds Max <year> SDK\maxsdk`)
- Work from: `XFBIN Import/` (project root for CMake/batch files)

## Standard build

```bat
cd "XFBIN Import"
BAUE_ALLE.bat          :: all SDKs found, 2016-2027
BAUE_ALLE.bat 2025     :: single Max version
```

`xfbindump.exe` builds first (no SDK) — use it to validate parser changes quickly.

## Pre-ship checks

```bat
python tools\PRUEFE_API.py
python tools\PRUEFE_SCRIPTS.py
```

Exit code 0 required.

## Install locally

Close 3ds Max first.

```bat
INSTALLIERE.bat
```

Target: `%APPDATA%\Autodesk\ApplicationPlugins\XfbinImport\`

Verify in Max Listener:

```maxscript
XfbinCpp.version()
```

## Version bump checklist

1. `src/xfbinimport.h` — `XFBINIMPORT_VERSION_STR`
2. `package/XfbinImport/PackageContents.xml` and `../XfbinImport/PackageContents.xml`
3. `scripts/XfbinImport.mcr` (+ sync copies under `XfbinImport/` and `package/`)
4. `CHANGELOG.md`
5. New **ProductCode** GUID; **UpgradeCode** unchanged

Then full `BAUE_ALLE.bat` + `INSTALLIERE.bat`.

## UI sync after editing `.mcr`

Authoritative file: `XFBIN Import/scripts/XfbinImport.mcr`

Copy to:

- `XfbinImport/Contents/MacroScripts/XfbinImport.mcr`
- `XFBIN Import/package/XfbinImport/Contents/MacroScripts/XfbinImport.mcr` (if present)

## Do not

- Push or open PR without user approval
- Commit unless user asks
