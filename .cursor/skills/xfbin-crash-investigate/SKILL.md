---
name: xfbin-crash-investigate
description: >-
  Investigates 3ds Max crashes involving XFBIN Import — CER dumps, sequence
  load failures, re-import buildMaterials errors, spl1 special-move files.
---

# XFBIN crash investigation

## Gather evidence

1. **User steps** — import only vs sequence vs second import; options (textures, visibility, material anim).
2. **CER folder** — `%LOCALAPPDATA%\Autodesk\CER\<machine-hash>\<n>\`
   - `dmpuserinfo.xml` — Max version, session length
   - `3dsmax_minidump.dmp` — strings often show anim names (`2kbxspl1_*`, etc.)
3. **MAXScript error** — line in `XfbinImport.mcr` vs actual fault in `XfbinCpp.*` (C++).
4. **Listener log** — enable “Log to Listener”, `XfbinCpp.log()`, `XfbinCpp.warnings()`.

## Classify failure

| Pattern | Likely layer |
|---------|----------------|
| Line 587 `buildMaterials` on 2nd import | Stale `sceneMaterials_` / `meshNodes_` after `delete objects` |
| Sequence crash with `2kbxspl1` present | Post-process anims without bone entries |
| Wrong Max year `.dlu` | Instant crash, any operation |
| `Unknown property` | Old DLU, API not rebuilt |

## Code hotspots

- `ClearScene()` — must clear `meshNodes_`, `sceneMaterials_`, bone/mesh registries
- `BuildMaterials()` — null-check `Mtl*`, invalidate cache on scene clear
- `BuildVisibility` / `BuildMaterialAnim` / `BuildAnimAt` — skip non-`kEntryBone` clips
- `Anm::HasBoneEntries()` / `animIsSkeletal` — sequence gating
- `ScanFolder` — hybrid clump+anm files in **both** lists

## Inspect suspect XFBIN without Max

```bat
xfbindump.exe file.xfbin --anims-o out.txt
```

Look for `AnmEntryFormat`: bone=1, camera=2, material=4, lights=5/6.

Python string scan for `FCURVE_TYPE_BLUR`, `GLARE`, `DOF`, `COLOR_FILTER`.

## Output

Write findings to `BUGREPORT_<short-name>.md` at repo root:

- Reproduction
- Root cause (with file/line)
- Fix recommendation (C++ vs MAXScript)
- Acceptance tests

Do **not** open PR unless user approves.
