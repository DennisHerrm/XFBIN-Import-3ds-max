# XFBIN Import for 3ds Max

A native 3ds Max plugin that imports CyberConnect2 **XFBIN** files —
the model, animation and texture container used by the *Naruto:
Ultimate Ninja Storm* series, *JoJo's Bizarre Adventure: All-Star
Battle*, and other CC2 titles.

Models, skeletons, skinning, textures and animations, in one step.
Works with **3ds Max 2016 through 2027**.

---

## What it does

| | |
|---|---|
| **Skeleton** | Full bone hierarchy with bind pose from `nuccChunkClump` / `nuccChunkCoord` |
| **Meshes** | NUD geometry with UVs, vertex colours, explicit normals and material IDs |
| **Skinning** | Skin modifier with per-vertex weights, applied through `ISkinImportData` |
| **Textures** | NUT textures written out as DDS and wired into Standard materials |
| **Animations** | All 23 curve formats, quantised values, per-bone keys |
| **Accessories** | Weapons and props from separate files, including multiple instances of the same model |
| **Sequence mode** | All animations laid out on one timeline with note tracks — the layout Warcraft 3 tools expect |

Everything runs in C++. A character with 222 bones, 22 600 vertices
and 72 800 keyframes loads in well under a second.

---

## Installing

**Close 3ds Max first.** While it is running the plugin file is
locked and cannot be replaced.

### The quick way

1. Download a release, or build from source (see below).
2. Run `INSTALLIERE.bat`.
3. Start 3ds Max. The tool appears under **DH Tools → XFBIN Import**.

To remove it again, run `DEINSTALLIERE.bat`.

### By hand

The installer only copies files — you can do the same yourself.

Open the Explorer and paste this into the address bar:

```
%APPDATA%\Autodesk\ApplicationPlugins
```

That expands to `C:\Users\<you>\AppData\Roaming\Autodesk\ApplicationPlugins`.
3ds Max scans this folder on every start. It is the per-user
location, so no administrator rights are needed.

Create a folder `XfbinImport` there and arrange the files like this:

```
%APPDATA%\Autodesk\ApplicationPlugins\
└── XfbinImport\
    ├── PackageContents.xml
    └── Contents\
        ├── 2016\  XfbinImport.dlu
        ├── 2017\  XfbinImport.dlu
        ├──  …      one folder per Max version
        ├── 2027\  XfbinImport.dlu
        ├── MacroScripts\
        │   ├── XfbinImport.mcr      ← the user interface
        │   └── XFBIN_Import.ms      ← optional launcher
        └── Post-Start-Up_Scripts\
            ├── XfbinMenu_2016_2024.ms
            └── XfbinMenu_2025_2027.ms
```

Two things matter:

- **`PackageContents.xml` sits next to `Contents`, not inside it.**
  Without it in the right place Max ignores the whole folder.
- **The version folders are not interchangeable.** Each `.dlu` is
  built against its own SDK. Loading a 2024 build in 2026 crashes Max
  rather than reporting an error. You only need the folder matching
  your Max version — the rest can be deleted.

Start 3ds Max. You should see this in the Listener:

```
XFBIN Import 1.5.2: Plugin geladen, Menue-Callback angemeldet.
```

---

## Starting it

Three ways, all equivalent:

**Menu.** *DH Tools → XFBIN Import*. On Max 2025 and newer the menu
may only appear after a second restart — the menu system registers
through a callback that fires on the next rebuild.

**Customize.** *Customize → Customize User Interface → Category
"DH Tools"*. From there you can drop it on a toolbar or bind a
keyboard shortcut. This entry is available immediately.

**Drag the script in.** Take `XFBIN_Import.ms` from the
`MacroScripts` folder and drag it into a 3ds Max viewport. That opens
the window right away — useful if the menu has not appeared yet, or
if you are running the files straight from the source folder without
installing.

The launcher does not contain the interface itself; it looks for
`XfbinImport.mcr` next to it, in the package folder, or under
ApplicationPlugins, loads it and calls the macro. It also reports in
the Listener whether `XfbinImport.dlu` is loaded at all, which is the
first question when something does not work.

### If nothing shows up

Type this in the MAXScript Listener:

```maxscript
XfbinCpp.version()
```

If it answers with a version string the plugin is loaded and only the
menu is missing — use *Customize* or the launcher script. If it says
*Unknown property*, the `.dlu` was not loaded: check
*Customize → Plugin Manager* and search for "Xfbin", and make sure
the folder for **your** Max version exists and contains the file.

---

## Using it

XFBIN characters are spread over several files — the body, the
animations, and often accessories such as weapons. So you pick the
**folder**, not a single file.

1. **Browse…** and choose the folder holding the `.xfbin` files.
   The tool reads every file and sorts them by content: a file with
   `nuccChunkClump` is a model, one with `nuccChunkAnm` is an
   animation file. File names do not matter.
2. **Import** — bones, meshes, skinning and textures, in one go.
   Textures land in a `textures` subfolder next to the source file.
3. Pick an animation and hit **Apply selected**, or **Load all as
   sequence** for the full set.

### Options

| Option | Default | Notes |
|---|---|---|
| Clear scene first | off | Handy while iterating |
| Import textures | on | Writes DDS and builds materials |
| Fill Material Editor | on | Puts the materials into the Compact Material Editor slots |
| Skip LOD | on | Skips models whose name contains `_lod` |
| Explicit normals | on | Keeps the cel-shading normals instead of letting Max recompute them |
| Skin modifier | on | Turn off to get geometry and rig without skinning |
| Bone size | 0 | Bone objects render as bare lines; takes effect immediately |
| Scale | 1.0 | The file is in centimetres. Use `0.01` if you want metres |

### Sequence mode

**Load all as sequence** builds a single timeline:

- Frame 0 holds the **bind pose** as a key, so an exporter finds a
  defined starting point.
- Every animation follows in turn, separated by an adjustable gap.
- A note track named `animations` goes on the scene root, with a key
  at the start and end of each sequence carrying its name.
- **Rest keys** writes bind-pose keys at both ends of a sequence for
  every bone that animation does not touch. Without them Max
  interpolates across the gap and one animation bleeds into the next —
  a typical animation only moves about half the skeleton.

This is the layout the Warcraft 3 exporters read.

---

## Building

Requires Visual Studio 2022 or newer and at least one 3ds Max SDK
installed in the default location
(`C:\Program Files\Autodesk\3ds Max <year> SDK\maxsdk`).

```bat
BAUE_ALLE.bat          :: builds every SDK it finds, 2016 - 2027
BAUE_ALLE.bat 2024     :: builds one version only
INSTALLIERE.bat        :: packages and installs
```

Versions without an installed SDK are skipped, not treated as errors.
The generator is found through `vswhere`; `XFBIN_GENERATOR` overrides
it.

`BAUE_ALLE.bat` builds `xfbindump.exe` first — a standalone
command-line inspector that needs no Max SDK at all. If something is
wrong in the parser it fails there in seconds instead of after twelve
SDK builds.

```bat
xfbindump.exe file.xfbin                   :: summary
xfbindump.exe file.xfbin --bones --meshes  :: skeleton and geometry stats
xfbindump.exe file.xfbin --anims-o out.txt :: full animation dump
xfbindump.exe file.xfbin --tex-o folder    :: extract textures as DDS
```

---

## How it was verified

The format work is a port of the
[Blender XFBIN Importer](https://github.com/Al-Hydra/Blender-XFBIN-Importer),
so its Python library makes an excellent reference implementation.

Every parsing stage writes a deterministic text dump, and
`tools/pydump_*.py` produce the same dump from the Python library.
The two are then compared line by line:

| Dump | Lines | Differences |
|---|---|---|
| Container (model) | 1 057 | 0 |
| Container (animations) | 17 984 | 0 |
| Skeleton — every bone, full world matrix | 1 781 | 0 |
| Meshes — every vertex, normal, colour, UV, weight, triangle | 52 558 | 0 |
| Animations — every one of 72 844 keyframes | 100 415 | 0 |

The exported DDS files are byte-identical to the ones the Python
library writes, header included.

`tools/VERGLEICHE_*.bat` run these comparisons on your own machine.
`tools/PRUEFE_SCRIPTS.py` lints the MaxScript files for the four
mistakes that actually happened during development — non-ASCII
characters, C-style comments, unbalanced brackets and forward
references.

---

## Known limitations

- Material animations (UV scroll, glare, blend) are parsed but not
  applied.
- Of the seven NUT pixel formats, two are verified against real data
  (R5G6B5 and DXT1). The others are implemented but untested.
- Import only — there is no exporter yet.
- CPK-compressed XFBIN files are detected and rejected with a clear
  message rather than unpacked.

---

## Credits

- **[Al-Hydra](https://github.com/Al-Hydra)** and
  **[SutandoTsukai181](https://github.com/SutandoTsukai181)** for the
  Blender XFBIN Importer and `xfbin_lib`, which document the format
  and served as the reference throughout.
- **[Smash Forge](https://github.com/jam1garner/Smash-Forge)** for the
  original NUD and NUT research.

XFBIN, NUCC, NUD and NUT are formats of CyberConnect2. This project is
unaffiliated and is meant for modding and preservation.
