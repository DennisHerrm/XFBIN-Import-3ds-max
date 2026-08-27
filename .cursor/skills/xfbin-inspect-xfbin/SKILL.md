---
name: xfbin-inspect-xfbin
description: >-
  Inspects CyberConnect2 XFBIN binary files using xfbindump and pydump tools
  without 3ds Max. Use when analyzing animation lists, chunk types, or comparing
  to Blender xfbin_lib reference.
---

# Inspect XFBIN files

Work from `XFBIN Import/` after building (or use built `xfbindump.exe` on PATH).

## xfbindump (C++, fast)

```bat
xfbindump.exe "path\to\file.xfbin"
xfbindump.exe file.xfbin --bones --meshes
xfbindump.exe file.xfbin --anims-o anims.txt
xfbindump.exe file.xfbin --tex-o textures\
```

Use before touching Max when validating parser or anim structure.

## Chunk classification (quick Python)

Count chunk markers in file bytes:

- `nuccChunkClump` → model skeleton/mesh container
- `nuccChunkAnm` → animation container
- Both present → hybrid file (model + anims in one XFBIN)

## pydump (Blender lib cross-check)

```bat
tools\VERGLEICHE.bat
tools\VERGLEICHE_ANIMS.bat
tools\VERGLEICHE_BONES.bat
tools\VERGLEICHE_MESHES.bat
```

Requires Python env setup per `DEVNOTES.md`.

## Special-move files (spl1 pattern)

Files like `*spl1.xfbin` often include:

- Camera / light / ambient chunks
- Material entries with Glare, Falloff, BlendRate curves
- Separate named tracks: `*_blur`, `*_glare`, `*_dof`, `*_colorfilter_*`

These are **not** equivalent to skeletal combat anims — sequence import must treat them differently.

## Codegraph

For how parsed data flows into Max:

```
codegraph_explore: "ParseAnims BuildAnimAt clearScene buildMaterials"
```

Namespace: user-codegraph MCP (if index exists for this repo).
