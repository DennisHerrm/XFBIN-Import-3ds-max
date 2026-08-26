# XFBIN Import for 3ds Max

[![3ds Max](https://img.shields.io/badge/3ds%20Max-2016%20–%202027-0696D7)](#building)
[![Language](https://img.shields.io/badge/C%2B%2B-17-00599C)](#building)
[![Verified](https://img.shields.io/badge/parser-173k%20lines%20verified-brightgreen)](#how-it-was-verified)

A native 3ds Max plugin that imports CyberConnect2 **XFBIN** files — the
model, animation and texture container used by the *Naruto: Ultimate
Ninja Storm* series, *JoJo's Bizarre Adventure: All-Star Battle* and
other CC2 titles.

Models, skeletons, skinning, textures and animations, in one step.

Built on the format research of the
**[Blender XFBIN Importer](https://github.com/Al-Hydra/Blender-XFBIN-Importer)**
— if you work in Blender, use that one.

[![Watch the presentation](https://img.youtube.com/vi/R0Xpl9ZzdVA/maxresdefault.jpg)](https://www.youtube.com/watch?v=R0Xpl9ZzdVA)

<sub>▶ Click to watch the presentation on YouTube</sub>

---

## What it does

| Area | What you get |
| :--- | :--- |
| **Skeleton** | Full bone hierarchy with bind pose from `nuccChunkClump` / `nuccChunkCoord` |
| **Meshes** | NUD geometry with UVs, vertex colours, explicit normals and material IDs |
| **Skinning** | Skin modifier with per-vertex weights, applied through `ISkinImportData` |
| **Textures** | NUT textures written out as DDS and wired into Standard materials |
| **Animations** | All 23 curve formats, quantised values, per-bone keys |
| **Accessories** | Weapons and props from separate files, including multiple instances of the same model |
| **Sequence mode** | All animations on one timeline with note tracks — the layout Warcraft 3 tools expect |

Everything runs in C++. A character with **222 bones**, **22,600
vertices** and **72,800 keyframes** loads in well under a second.

---

## Installing

> [!IMPORTANT]
> Close 3ds Max first. While it is running the plugin file is locked
> and cannot be replaced.

### What is in this repository

| Folder | What it is |
| :--- | :--- |
| `XfbinImport/` | **The ready-made plugin.** Copy this folder as it is — nothing to build. |
| `XFBIN Import/` | The full source, build scripts and verification tools. |

If you only want to use the importer, you need the first one.

### The quick way

1. Download the repository (**Code → Download ZIP**) or a release.
2. Copy the whole `XfbinImport` folder into
   `%APPDATA%\Autodesk\ApplicationPlugins`.
3. Start 3ds Max. The tool appears under **DH Tools → XFBIN Import**.

That is all — the folder already has the right shape. If you built from
source instead, `INSTALLIERE.bat` does the copying for you, and
`DEINSTALLIERE.bat` removes it again.

### Where it goes

Paste this into the Explorer address bar:

```
%APPDATA%\Autodesk\ApplicationPlugins
```

It expands to `C:\Users\<you>\AppData\Roaming\Autodesk\ApplicationPlugins`.
3ds Max scans this folder on every start. It is the per-user location,
so no administrator rights are needed.

The `XfbinImport` folder from this repository has to end up looking
exactly like this:

```text
%APPDATA%\Autodesk\ApplicationPlugins\
└── XfbinImport\
    ├── PackageContents.xml
    └── Contents\
        ├── 2016\
        │   └── XfbinImport.dlu
        ├── 2017\
        │   └── XfbinImport.dlu
        ├── ...                        one folder per Max version
        ├── 2027\
        │   └── XfbinImport.dlu
        ├── MacroScripts\
        │   ├── XfbinImport.mcr        the user interface
        │   └── XFBIN_Import.ms        optional launcher
        └── Post-Start-Up_Scripts\
            ├── XfbinMenu_2016_2024.ms
            └── XfbinMenu_2025_2027.ms
```

> [!WARNING]
> **`PackageContents.xml` sits next to `Contents`, not inside it.**
> In the wrong place, Max ignores the whole folder without a word.
>
> **The version folders are not interchangeable.** Each `.dlu` is built
> against its own SDK — loading a 2024 build in 2026 crashes Max rather
> than reporting an error. You only need the folder matching your Max
> version; the rest can be deleted.

Start 3ds Max. The Listener should show:

```
XFBIN Import 1.5.2: Plugin geladen, Menue-Callback angemeldet.
```

---

## Starting it

Three ways, all equivalent:

| Way | Where | Note |
| :--- | :--- | :--- |
| **Menu** | *DH Tools → XFBIN Import* | On Max 2025+ it may take a second restart — the menu registers through a callback that fires on the next rebuild |
| **Customize** | *Customize → Customize User Interface → Category "DH Tools"* | Available immediately; put it on a toolbar or a shortcut |
| **Drag & drop** | `XFBIN_Import.ms` into a viewport | Opens the window right away, also works without installing |

The launcher script does not hold the interface itself. It looks for
`XfbinImport.mcr` next to it, in the package folder or under
ApplicationPlugins, loads it and calls the macro — and reports in the
Listener whether the plugin is loaded at all.

<details>
<summary><b>Nothing shows up?</b></summary>

Type this in the MAXScript Listener:

```maxscript
XfbinCpp.version()
```

- **Answers with a version string** → the plugin is loaded and only the
  menu is missing. Use *Customize* or the launcher script.
- **Says `Unknown property`** → the `.dlu` was not loaded. Check
  *Customize → Plugin Manager* and search for "Xfbin", and make sure the
  folder for **your** Max version exists and holds the file.

</details>

---

## Using it

XFBIN characters are spread over several files — the body, the
animations, and often accessories such as weapons. So you pick the
**folder**, not a single file.

1. **Browse…** and choose the folder holding the `.xfbin` files.
   Every file is read and sorted by content: `nuccChunkClump` means a
   model, `nuccChunkAnm` means an animation file. File names do not
   matter.
2. **Import** — bones, meshes, skinning and textures in one go.
   Textures land in a `textures` subfolder next to the source file.
3. Pick an animation and hit **Apply selected**, or **Load all as
   sequence** for the full set.

### Options

| Option | Default | Notes |
| :--- | :---: | :--- |
| Clear scene first | off | Handy while iterating |
| Import textures | on | Writes DDS and builds materials |
| Fill Material Editor | on | Puts the materials into the Compact Material Editor slots |
| Skip LOD | on | Skips models whose name contains `_lod` |
| Explicit normals | on | Keeps the cel-shading normals instead of letting Max recompute them |
| Skin modifier | on | Turn off for geometry and rig without skinning |
| Bone size | `0` | Bone objects render as bare lines; takes effect immediately |
| Scale | `1.0` | The file is in centimetres — use `0.01` for metres |

### Sequence mode

**Load all as sequence** builds a single timeline:

- Frame 0 holds the **bind pose** as a key, so an exporter finds a
  defined starting point.
- Every animation follows in turn, separated by an adjustable gap.
- A note track named `animations` goes on the scene root, with a key at
  the start and end of each sequence carrying its name.
- **Rest keys** writes bind-pose keys at both ends of a sequence for
  every bone that animation does not touch. Without them Max
  interpolates across the gap and one animation bleeds into the next —
  a typical animation only moves about half the skeleton.

> [!TIP]
> This is the layout the Warcraft 3 exporters read.

---

## Building

Requires Visual Studio 2022 or newer and at least one 3ds Max SDK in the
default location `C:\Program Files\Autodesk\3ds Max <year> SDK\maxsdk`.

```bat
BAUE_ALLE.bat          :: builds every SDK it finds, 2016 - 2027
BAUE_ALLE.bat 2024     :: builds one version only
INSTALLIERE.bat        :: packages and installs
```

Versions without an installed SDK are skipped, not treated as errors.
The generator is found through `vswhere`; `XFBIN_GENERATOR` overrides it.

`BAUE_ALLE.bat` builds **`xfbindump.exe`** first — a standalone
command-line inspector that needs no Max SDK at all. If something is
wrong in the parser it fails there in seconds instead of after twelve
SDK builds.

```bat
xfbindump.exe file.xfbin                    :: summary
xfbindump.exe file.xfbin --bones --meshes   :: skeleton and geometry stats
xfbindump.exe file.xfbin --anims-o out.txt  :: full animation dump
xfbindump.exe file.xfbin --tex-o folder     :: extract textures as DDS
```

---

## How it was verified

The format handling follows the
[Blender XFBIN Importer](https://github.com/Al-Hydra/Blender-XFBIN-Importer),
so its Python library `xfbin_lib` makes an excellent reference
implementation to check against.

Every parsing stage writes a deterministic text dump, and
`tools/pydump_*.py` produce the same dump from the Python library. The
two are then compared line by line:

| Dump | Lines | Differences |
| :--- | ---: | :---: |
| Container (model) | 1,057 | **0** |
| Container (animations) | 17,984 | **0** |
| Skeleton — every bone, full world matrix | 1,781 | **0** |
| Meshes — every vertex, normal, colour, UV, weight, triangle | 52,558 | **0** |
| Animations — every one of 72,844 keyframes | 100,415 | **0** |

The exported DDS files are **byte-identical** to the ones the Python
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

This importer would not exist without the people who worked out the
format in the first place.

| Project | By | What it gave this one |
| :--- | :--- | :--- |
| [Blender-XFBIN-Importer](https://github.com/Al-Hydra/Blender-XFBIN-Importer) | [Al-Hydra](https://github.com/Al-Hydra) | The current Blender addon (4.2+). Its `xfbin_lib` is the reference implementation this project was verified against, line by line. |
| [cc2_xfbin_blender](https://github.com/SutandoTsukai181/cc2_xfbin_blender) | [SutandoTsukai181](https://github.com/SutandoTsukai181) | The original addon and `xfbin_lib` (MIT), where the format work started. |
| [Smash Forge](https://github.com/jam1garner/Smash-Forge) | [jam1garner](https://github.com/jam1garner) | The original NUD and NUT research the container parsing rests on. |

**Working in Blender?** Use the
[Blender XFBIN Importer](https://github.com/Al-Hydra/Blender-XFBIN-Importer)
— it does import *and* export and is the more complete tool. This
project exists for people whose pipeline runs through 3ds Max.

XFBIN, NUCC, NUD and NUT are formats of CyberConnect2. This project is
unaffiliated and is meant for modding and preservation.

---

<sub>Built with Claude Code by DennisH</sub>
