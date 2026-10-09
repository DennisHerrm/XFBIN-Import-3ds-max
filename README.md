# XFBIN Import for 3ds Max

[![Download](https://img.shields.io/github/v/release/DennisHerrm/XFBIN-Import-3ds-max?label=download&color=2ea043)](https://github.com/DennisHerrm/XFBIN-Import-3ds-max/releases/latest)
[![3ds Max](https://img.shields.io/badge/3ds%20Max-2016%20–%202027-0696D7)](#installing)
[![CI](https://github.com/DennisHerrm/XFBIN-Import-3ds-max/actions/workflows/ci.yml/badge.svg)](https://github.com/DennisHerrm/XFBIN-Import-3ds-max/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![Verified](https://img.shields.io/badge/parser-173k%20lines%20verified-brightgreen)](#how-it-was-verified)

A native 3ds Max plugin that imports characters from the
*Naruto: Ultimate Ninja Storm* games **straight from the installed
game** — models, skeletons, skinning, textures, the character's own
animations and its effects. No unpacking, no folder hunting.

Supported: **Storm 1, Storm 2, Storm 3 Full Burst, Storm 4 and Storm
Connections** (found automatically in every Steam library, also when
Steam names the folder differently, e.g. `QWANT` / `Qwant2` - the game
is recognised by its executable), or any folder with unpacked
CyberConnect2 **XFBIN** files. Storm Revolution is detected as well,
but untested.

A **WC3 mode** prepares everything for the
[WhiteoutDex](https://github.com/FernandoS27/WhiteoutDex) Warcraft III exporter; without it,
everything is imported the way the game does it.

Built on the format research of the
**[Blender XFBIN Importer](https://github.com/Al-Hydra/Blender-XFBIN-Importer)**
and **[Storm-Engine-NUCC](https://github.com/Al-Hydra/Storm-Engine-NUCC)**
— if you work in Blender, use the Blender importer.

[![Watch the presentation](https://img.youtube.com/vi/R0Xpl9ZzdVA/maxresdefault.jpg)](https://www.youtube.com/watch?v=R0Xpl9ZzdVA)

<sub>▶ Click to watch the presentation on YouTube</sub>

<p align="center">
  <a href="https://github.com/DennisHerrm/XFBIN-Import-3ds-max/releases/latest"><b>⬇&nbsp; Download the latest version</b></a>
  &nbsp;·&nbsp; <a href="#installing">Installation guide</a>
  &nbsp;·&nbsp; <a href="CHANGELOG.md">Changelog</a>
</p>

---

## What it does

| Area | What you get |
| :--- | :--- |
| **Game data** | Reads the CPK archives directly — CRILAYLA, nested CPKs (Storm 3) and the encrypted files of Storm 4 / Connections. Patches and DLC override the base game |
| **Character list** | Every character and costume of the game, searchable by name or code (`1nrt`, `2ssk` …) |
| **Skeleton** | Full bone hierarchy with bind pose from `nuccChunkClump` / `nuccChunkCoord` |
| **Meshes** | NUD geometry with UVs, vertex colours, explicit normals and material IDs |
| **Skinning** | Skin modifier with per-vertex weights |
| **Textures** | NUT textures written out as DDS and wired into Standard materials |
| **Own animations only** | Exactly the character's files (`bod1c`, `bod1l`, `skl1`, `spl1`, `awa` …) and only the clips that move its skeleton. Shared hit reactions (`1cmnbod1`) on request |
| **Effects** | The game's particle system: effect animations started by each attack (Rasengan …), effect models, mesh and sprite particles as WhiteoutDex *Particle 2*, all timed like in the game |
| **Transformations** | Forms a character turns into during its own animations (Naruto's awakening → `1nrvbod1`) |
| **Clones** | A skeleton used several times (shadow clones, two weapons) gets one copy per use |
| **Material animation** | UV offset, tiling and opacity — sprite flipbooks and UV scrolling |
| **Timeline** | One clip, or every clip in a row with note track and `NeoDexSequenceData` |
| **WC3 mode** | One mesh per material, facing −Y, in place, hard effect visibility per sequence |

Everything runs in C++. Importing Naruto from Storm 4 with 126 own
animations, 51 effect models and 34 particle emitters takes about four
seconds; all 126 animations go onto the timeline in under ten.

---

## Installing

> [!IMPORTANT]
> Close 3ds Max first. While it is running the plugin file is locked
> and cannot be replaced.

All downloads are on the **[Releases page](https://github.com/DennisHerrm/XFBIN-Import-3ds-max/releases/latest)**.
Do not use *Code → Download ZIP* — that is the source code, not the
plugin.

### Option 1 — Setup (recommended)

1. Download **`XfbinImport-<version>-Setup.exe`** from the
   [latest release](https://github.com/DennisHerrm/XFBIN-Import-3ds-max/releases/latest).
2. Run it. It installs for all users into
   `C:\ProgramData\Autodesk\ApplicationPlugins`, next to WhiteoutDex and
   other plugins. Without administrator rights, pick *Install for me
   only* in the first dialog.
3. Start 3ds Max and open **DH Tools → XFBIN Import**.

**Updating:** run the Setup of the newer version — it replaces the old
one. **Uninstalling:** *Windows Settings → Apps → Installed apps →
XFBIN Import for 3ds Max*.

One installer covers every 3ds Max version from 2016 to 2027; Max picks
the matching plugin build by itself.

> [!NOTE]
> The installer is not code-signed yet. On a fresh download Windows
> SmartScreen may say *"Windows protected your PC"* — click
> **More info → Run anyway**. To make sure the file is the original,
> compare its hash with `SHA256SUMS.txt` from the release:
>
> ```powershell
> Get-FileHash .\XfbinImport-2.1.6-Setup.exe
> ```

### Option 2 — ZIP, without an installer

1. Download **`XfbinImport-<version>.zip`** and extract it.
2. Double-click **`Install.bat`**. `Uninstall.bat` removes it again.

Or copy the `XfbinImport` folder from the ZIP by hand into
`%ProgramData%\Autodesk\ApplicationPlugins` — see below.

<details>
<summary><b>Where the plugin goes, and what the folder must look like</b></summary>

Paste this into the Explorer address bar:

```
%ProgramData%\Autodesk\ApplicationPlugins
```

It expands to `C:\ProgramData\Autodesk\ApplicationPlugins`. 3ds Max
scans this folder on every start, for every user. Users may create
folders there, so no administrator rights are needed.
(`%APPDATA%\Autodesk\ApplicationPlugins` works too, for one user only;
versions up to 2.x installed there — the 3.0 installers remove that old
copy, otherwise Max would load the plugin twice.)

The `XfbinImport` folder has to end up looking exactly like this:

```text
%ProgramData%\Autodesk\ApplicationPlugins\
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
        │   ├── XfbinImport.mcr        menu commands
        │   └── XFBIN_Import.ms        optional launcher
        ├── Post-Start-Up_Scripts\
        │   └── XfbinMenu_2016_2024.ms
        └── Pre-Start-Up_Scripts\
            └── XfbinMenu_2025_2027.ms
```

> [!WARNING]
> **`PackageContents.xml` sits next to `Contents`, not inside it.**
> In the wrong place, Max ignores the whole folder without a word.
>
> **The version folders are not interchangeable.** Each `.dlu` is built
> against its own SDK — loading a 2024 build in 2026 crashes Max rather
> than reporting an error.

</details>

After starting 3ds Max, the MAXScript Listener shows:

```
XFBIN Import 3.0.0: Plugin geladen, Menue-Callback angemeldet.
```

(on 3ds Max 2016 – 2024: `... Menue registriert (menuMan).`)

---

## Starting it

| Way | Where |
| :--- | :--- |
| **Menu** | *DH Tools → XFBIN Import* (characters) and *DH Tools → XFBIN Animations* |
| **File → Import** | pick any `.xfbin` or `.cpk` of a game — the window opens with that game |
| **Customize** | *Customize → Customize User Interface → Category "DH Tools"* — put it on a toolbar or a shortcut |
| **Drag & drop** | `XFBIN_Import.ms` into a viewport |

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

### 1. Import a character

**XFBIN Import** lists every installed Storm game in the *Game* box —
the newest is chosen first. *Browse…* takes any other folder.

Pick a character (double-click, or *Import*). The tabs separate
playable characters from bosses and special forms; the search finds
names and codes (`naruto`, `2ssk`, `bod2` for a second costume).

| Switch | Default | What it does |
| :--- | :---: | :--- |
| **Textures** | on | Writes the DDS files to `%LOCALAPPDATA%\XfbinImport\textures` and builds materials |
| **Effects** | on | Effect models, started effect animations and particles of the character's jutsu and ultimates |
| **Reactions** | off | Adds the shared hit, fall and get-up animations every character uses |
| **Clear scene** | off | Deletes everything before the import |
| **WC3 mode** | off | Prepares the scene for WhiteoutDex — see below |

### 2. Load animations

**XFBIN Animations** (or *Animations…* in the first window) lists the
imported character's own animations with length and group: *Basic,
Combos, Support, Intro & win, Jutsu, Ultimate, Awakening* (and
*Reactions* if switched on).

- **Load** (or double-click) puts one animation on the timeline, with
  its effects.
- **All to timeline** puts every listed animation in a row: frame 0 is
  the bind pose, then one sequence after another with the chosen gap,
  named in a note track on the scene root and in `NeoDexSequenceData`.
  The *Sequence* box jumps to one of them. Esc cancels.

| Switch | Default | What it does |
| :--- | :---: | :--- |
| **WC3 mode** | off | See below |
| **Note track** | on | Sequence names on the scene root plus `NeoDexSequenceData` |
| **Rest keys** | on | Bones a clip does not move get bind-pose keys at both ends, so clips do not bleed into each other |
| **Visibility** | on | Step keys per sequence from the opacity data — weapons, clones and forms appear and vanish |
| **Material** | on | UV offset, tiling and opacity of the materials |

### Effects

The games drive their effects through particle chunks: every animation
has one with the same name, and each emitter starts something at a
dummy bone, at times given in hundredths of a frame:

| Emitter starts | In Max |
| :--- | :--- |
| an **effect animation** (Rasengan, smoke burst …) | its models hang on the dummy bone; the animation is keyed at the start time |
| a single **mesh** (ring, shock wave) | the mesh, visible for its life time, growing like in the game |
| a **swarm** of sprites or meshes | a WhiteoutDex *Particle 2* with count, life time, spawn radius, size and colour gradient, texture and flipbook grid — only if WhiteoutDex is installed |

Effect animations can start further effects; up to four levels are
followed.

### WC3 mode

Made after the WhiteoutDex importer guidelines, so the exported model
works without fixing it by hand:

- **One mesh per material** — a Multi/Sub material would export
  untextured.
- **Facing −Y** — a helper *WC3 root* turns the character, because
  WhiteoutDex writes Max −Y as Warcraft's forward.
- **In place** — the root bones keep their horizontal rest position;
  jumps keep their height. Effects and clones still move.
- **Effects on/off** — step keys from opacity, scale and the particle
  timelines, with a key at every sequence start and end; frame 0 shows
  no effect.
- Every skinned vertex has a weight, nothing exported is hidden.

Without WC3 mode the character travels, and effects fade, scale and
scroll the way the game does it.

> [!TIP]
> `tools/VIS_CHECK.ms` walks every mesh against every sequence and
> reports any place where visibility keys are missing.

---

## Building from source

Requires Visual Studio 2022 or newer and at least one 3ds Max SDK in the
default location `C:\Program Files\Autodesk\3ds Max <year> SDK\maxsdk`.

```bat
BAUE_ALLE.bat          :: builds every SDK it finds, 2016 - 2027
BAUE_ALLE.bat 2024     :: builds one version only
INSTALLIERE.bat        :: packages and installs for the current user
DEINSTALLIERE.bat      :: removes it again
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

xfbindump.exe --spiele                                 :: installed Storm games
xfbindump.exe --spiel <game> katalog naruto            :: characters and their files
xfbindump.exe --spiel <game> figur 1nrt                :: own animations of a character
xfbindump.exe --spiel <game> extract data/spc/1nrtbod1.xfbin out.xfbin
xfbindump.exe --spiel <game> test data/spc/            :: unpack and parse every file
xfbindump.exe --spiel <game> partikel data/spc/1nrtskl1 x   :: particle emitters
```

### Repository layout

| Path | Contents |
| :--- | :--- |
| `src/` | The C++ plugin: parsers (XFBIN, NUD, NUT, animation, particles, CPK), game catalog, windows and import flow |
| `scripts/` | Menu commands, launcher and menu registration |
| `package/` | `PackageContents.xml` — the Autodesk application package manifest |
| `installer/` | Inno Setup script and the `Install.bat` / `Uninstall.bat` for the ZIP |
| `tools/` | Verification tools and in-Max check scripts (see below) |
| `.github/` | CI workflow and issue templates |
| `CHANGELOG.md` | Every version and what changed (German) |
| `DEVNOTES.md` | Design decisions and internals (German) |

### Making a release

Needs [Inno Setup 6](https://jrsoftware.org/isinfo.php)
(`winget install JRSoftware.InnoSetup`).

```bat
python tools\VERSION_SETZEN.py 3.0.1   :: sets the version everywhere
BAUE_ALLE.bat                          :: all twelve Max versions
ERSTELLE_RELEASE.bat                   :: dist\ -> Setup.exe, .zip, SHA256SUMS.txt
```

The script prints the matching `gh release create` command at the end.
It refuses to build a release unless all twelve Max versions are present
and `PackageContents.xml` carries the same version as the plugin header.

Compiled files are never committed — they belong on the Releases page.
CI builds the parser and runs the checks on every push; the plugin
itself cannot be built there because the Max SDKs are not
redistributable.

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

Four more checks run before every release, and none of them needs
3ds Max:

| Tool | Checks |
| :--- | :--- |
| `PRUEFE_SCRIPTS.py` | the MaxScript files: non-ASCII, C-style comments, bracket balance, forward references, handlers for controls that do not exist, calls to plugin functions that do not exist, and `continue` (which MAXScript does not have) |
| `PRUEFE_API.py` | that every plugin function is registered in all three places — enum, function map and interface descriptor |
| `PRUEFE_INCLUDES.py` | that every `std` type a file uses has its header included |
| `VERSION_SETZEN.py` | sets all six version places at once and reads them back |

Each of these exists because the corresponding mistake actually
happened during development and cost a build.

Three scripts help check the result inside Max:

| Script | Shows |
| :--- | :--- |
| `NOTE_CHECK.ms` | the note tracks on the scene root, with sequence names and times |
| `VIS_CHECK.ms` | whether every mesh has at least two visibility keys in every sequence |
| `BONE_CHECK.ms` | the bone matrices and the bind matrices stored in the Skin modifier |

---

## Known limitations

- **Effect spawn timing** comes from the particle chunks. Visibility
  follows the game data: opacity along the bone chain and through the
  animation's own parent table (guide dummies such as `1efc_dmy01`),
  and the model list of every clump reference. Material-only tricks
  (UV scrolling, glare) are not turned into visibility.
- Particle 2 values are mapped from the game data, partly by best
  guess (speed, gravity, size). Forces such as vortex and wind are not
  applied.
- A started effect animation is played once per use, not once per
  particle — an emitter that starts twenty copies shows one. While it
  runs, all of its models are shown.
- Shader-only material values — glare, falloff, blend rate, outline ID
  — have no 3ds Max equivalent.
- Of the seven NUT pixel formats, two are verified against real data
  (R5G6B5 and DXT1). The others are implemented but untested.
- Camera and light entries are parsed, but not applied.
- Import only — there is no exporter back to XFBIN.

---

## Credits

This importer would not exist without the people who worked out the
format in the first place.

| Project | By | What it gave this one |
| :--- | :--- | :--- |
| [Blender-XFBIN-Importer](https://github.com/Al-Hydra/Blender-XFBIN-Importer) | [Al-Hydra](https://github.com/Al-Hydra) | The current Blender addon (4.2+). Its `xfbin_lib` is the reference implementation this project was verified against, line by line. |
| [cc2_xfbin_blender](https://github.com/SutandoTsukai181/cc2_xfbin_blender) | [SutandoTsukai181](https://github.com/SutandoTsukai181) | The original addon and `xfbin_lib` (MIT), where the format work started. |
| [Smash Forge](https://github.com/jam1garner/Smash-Forge) | [jam1garner](https://github.com/jam1garner) | The original NUD and NUT research the container parsing rests on. |
| [Storm-Engine-NUCC](https://github.com/Al-Hydra/Storm-Engine-NUCC) | [Al-Hydra](https://github.com/Al-Hydra) and contributors | 010 Editor templates (MIT) — the `nuccChunkParticle` and `nuccChunkBillboard` layouts the effect import is built on. |

**Working in Blender?** Use the
[Blender XFBIN Importer](https://github.com/Al-Hydra/Blender-XFBIN-Importer)
— it does import *and* export and is the more complete tool. This
project exists for people whose pipeline runs through 3ds Max.

XFBIN, NUCC, NUD and NUT are formats of CyberConnect2. This project is
unaffiliated and is meant for modding and preservation.

---

## License

[MIT](LICENSE) © 2026 DennisH

---

<sub>Built with Claude Code by DennisH</sub>
