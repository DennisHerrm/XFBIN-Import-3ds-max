# XFBIN-Import-3ds-max
XFBIN Import for 3ds Max

A native 3ds Max plugin that imports CyberConnect2 XFBIN files — the model, animation and texture container used by the Naruto: Ultimate Ninja Storm series, JoJo's Bizarre Adventure: All-Star Battle, and other CC2 titles.

Models, skeletons, skinning, textures and animations, in one step. Works with 3ds Max 2016 through 2027.

What it does
	
Skeleton	Full bone hierarchy with bind pose from nuccChunkClump / nuccChunkCoord
Meshes	NUD geometry with UVs, vertex colours, explicit normals and material IDs
Skinning	Skin modifier with per-vertex weights, applied through ISkinImportData
Textures	NUT textures written out as DDS and wired into Standard materials
Animations	All 23 curve formats, quantised values, per-bone keys
Accessories	Weapons and props from separate files, including multiple instances of the same model
Sequence mode	All animations laid out on one timeline with note tracks — the layout Warcraft 3 tools expect

Everything runs in C++. A character with 222 bones, 22 600 vertices and 72 800 keyframes loads in well under a second.

Installing
Download a release, or build from source (see below).
Run INSTALLIERE.bat. It copies the package to %APPDATA%\Autodesk\ApplicationPlugins\XfbinImport.
Start 3ds Max. The tool appears under DH Tools → XFBIN Import.

It also registers under Customize → Customize User Interface → Category "DH Tools", so you can put it on a toolbar or a shortcut.

To remove it, run DEINSTALLIERE.bat.

Using it

XFBIN characters are spread over several files — the body, the animations, and often accessories such as weapons. So you pick the folder, not a single file.

Browse… and choose the folder holding the .xfbin files. The tool reads every file and sorts them by content: a file with nuccChunkClump is a model, one with nuccChunkAnm is an animation file. File names do not matter.
Import — bones, meshes, skinning and textures, in one go. Textures land in a textures subfolder next to the source file.
Pick an animation and hit Apply selected, or Load all as sequence for the full set.
Options
Option	Default	Notes
Clear scene first	off	Handy while iterating
Import textures	on	Writes DDS and builds materials
Fill Material Editor	on	Puts the materials into the Compact Material Editor slots
Skip LOD	on	Skips models whose name contains _lod
Explicit normals	on	Keeps the cel-shading normals instead of letting Max recompute them
Skin modifier	on	Turn off to get geometry and rig without skinning
Bone size	0	Bone objects render as bare lines; takes effect immediately
Scale	1.0	The file is in centimetres. Use 0.01 if you want metres
Sequence mode

Load all as sequence builds a single timeline:

Frame 0 holds the bind pose as a key, so an exporter finds a defined starting point.
Every animation follows in turn, separated by an adjustable gap.
A note track named animations goes on the scene root, with a key at the start and end of each sequence carrying its name.
Rest keys writes bind-pose keys at both ends of a sequence for every bone that animation does not touch. Without them Max interpolates across the gap and one animation bleeds into the next — a typical animation only moves about half the skeleton.

This is the layout the Warcraft 3 exporters read.

Building

Requires Visual Studio 2022 or newer and at least one 3ds Max SDK installed in the default location (C:\Program Files\Autodesk\3ds Max <year> SDK\maxsdk).

bat
BAUE_ALLE.bat          :: builds every SDK it finds, 2016 - 2027
BAUE_ALLE.bat 2024     :: builds one version only
INSTALLIERE.bat        :: packages and installs

Versions without an installed SDK are skipped, not treated as errors. The generator is found through vswhere; XFBIN_GENERATOR overrides it.

BAUE_ALLE.bat builds xfbindump.exe first — a standalone command-line inspector that needs no Max SDK at all. If something is wrong in the parser it fails there in seconds instead of after twelve SDK builds.

bat
xfbindump.exe file.xfbin                   :: summary
xfbindump.exe file.xfbin --bones --meshes  :: skeleton and geometry stats
xfbindump.exe file.xfbin --anims-o out.txt :: full animation dump
xfbindump.exe file.xfbin --tex-o folder    :: extract textures as DDS
How it was verified

The format work is a port of the Blender XFBIN Importer, so its Python library makes an excellent reference implementation.

Every parsing stage writes a deterministic text dump, and tools/pydump_*.py produce the same dump from the Python library. The two are then compared line by line:

Dump	Lines	Differences
Container (model)	1 057	0
Container (animations)	17 984	0
Skeleton — every bone, full world matrix	1 781	0
Meshes — every vertex, normal, colour, UV, weight, triangle	52 558	0
Animations — every one of 72 844 keyframes	100 415	0

The exported DDS files are byte-identical to the ones the Python library writes, header included.

tools/VERGLEICHE_*.bat run these comparisons on your own machine. tools/PRUEFE_SCRIPTS.py lints the MaxScript files for the four mistakes that actually happened during development — non-ASCII characters, C-style comments, unbalanced brackets and forward references.

Credits
Al-Hydra and SutandoTsukai181 for the Blender XFBIN Importer and xfbin_lib, which document the format and served as the reference throughout.
Smash Forge for the original NUD and NUT research.

XFBIN, NUCC, NUD and NUT are formats of CyberConnect2. This project is unaffiliated and is meant for modding and preservation.


Build with Claude Code by DennisH
