# XfbinImport 1.9.2 — Bug Report (Kabuto / 2kbx)

**Reporter:** Mauri (via investigation in 3ds Max 2025)  
**Plugin version:** 1.9.2 (`XfbinImport.dlu` + `XfbinImport.mcr`)  
**Target:** Proper C++ fixes — not MAXScript workarounds  
**Test model:** `KabutoRedCloak` (CyberConnect2 / 2kbx skeleton, prefix `2kbx`)

---

## TL;DR for AI assistant

Two separate bugs in the **C++ plugin** (`XfbinImport.dlu`):

| # | Symptom | Likely crash site | Fix area |
|---|---------|-------------------|----------|
| 1 | **Load all as sequence** crashes when `2kbxspl1.xfbin` is in the import folder | `buildMaterialAnim`, `buildAnimAt`, and/or `buildVisibility` | Skip or safely ignore **post-process / cinematic fcurve types** and camera-light targets that have no scene node |
| 2 | **Second Import** (even with “Clear scene first”) often crashes at `buildMaterials` | `buildMaterials` + incomplete `clearScene` | `clearScene()` must fully reset material/texture caches; `buildMaterials` must null-check all cached pointers |

MAXScript (`XfbinImport.mcr`) can add defensive calls (`close`, `clearScene`, `clearAnims`) but **will not fix hard ACCESS_VIOLATION inside the DLL**.

---

## Bug 1 — `2kbxspl1.xfbin` breaks “Load all as sequence”

### Reproduction

1. Open 3ds Max 2025 with XfbinImport 1.9.2 loaded.
2. Point the UI at folder: `KabutoRedCloak` (all top-level `.xfbin` files present, **including** `2kbxspl1.xfbin`).
3. **Import** — model loads fine (~658 bones, multiple clump instances).
4. Click **Load all as sequence** (default options: Rest keys, Visibility, Material anim, Note track all ON).
5. **Result:** hard crash (ACCESS_VIOLATION or STATUS_INVALID_PARAMETER).

### Control test

Move `2kbxspl1.xfbin` out of the scan folder (e.g. into `Extra\`).  
→ Sequence load completes successfully for all remaining animation files.

### Why this file is special

`2kbxspl1.xfbin` is a **special-move cinematic bundle**, not a normal combat anim pack like `2kbxskl1.xfbin`.

**Chunk types present in spl1 but not in skl1:**

- `nuccChunkCamera`
- `nuccChunkLightDirc`, `nuccChunkLightPoint`
- `nuccChunkAmbient`
- `nuccChunkBillboard`, `nuccChunkTrail`, `nuccChunkBinary`

**Post-process fcurve types (FCURVE_TYPE_*):**

| File | FCURVE count | Types |
|------|--------------|-------|
| `2kbxskl1.xfbin` | 0 | — |
| `2kbxbod1l.xfbin` | 3 | `BRIGHT_RATE`, `COLOR_FILTER_RADIAL` (minor) |
| **`2kbxspl1.xfbin`** | **13** | **`BLUR`, `GLARE`, `DOF`, `COLOR_FILTER_DIRECTIONAL`, `SHADOW`, `SOFTFOCUS`, `OMB`, `BRIGHT_RATE`** |

**Named animation tracks inside spl1 (22 total):**

Skeletal / gameplay clips (should be kept):

- `2kbxspl1_atk`, `2kbxspl1_atk_e`
- `2kbxspl1_cut`, `2kbxspl1_cut_nc`, `2kbxspl1_cut_dmg`
- `2kbxspl1_dmg`
- `2kbxspl1_e`, `2kbxspl1_l`, `2kbxspl1_s`

Post-process **sub-tracks** (likely cause of crash — engine runtime data, not Max-scene data):

- `2kbxspl1_atk_blur`, `_glare`, `_dof`, `_colorfilter_directional`, `_colorfilter_directional_2`, `_bright_rate`, `_omb`, `_shadow`, `_softfocus`
- `2kbxspl1_cut_blur`, `_glare`, `_colorfilter_directional`, `_colorfilter_directional_2`

Also references: `camera001`, light/ambient/particle/trail data, path like  
`Z:/char/c/2kbx/anm/2kbxspl1/fcv/2kbxspl1_cut_colorfilter_directional.fcv`

### What the UI does (sequence path)

From `XfbinImport.mcr`, **btnSequence** loops every loaded animation index and calls:

```maxscript
XfbinCpp.buildIdleKeys i fAt (fAt + fLen)      -- if Rest keys
XfbinCpp.buildVisibility i fAt (fAt + fLen)    -- if Visibility
XfbinCpp.buildMaterialAnim i fAt (fAt + fLen)    -- if Material anim
XfbinCpp.buildAnimAt i fAt 7 spnScale.value
```

Crash dump memory (CER report #6, 2026-08-27) contained spl1 names (`2kbxspl1_atk`, `_cut`, `_dmg`, etc.) while `XfbinImport.dlu` was loaded. Fault was in `3dsmax.exe` / `ucrtbase.dll` (invalid parameter / null deref chain).

### Root cause (hypothesis — verify in source)

The plugin registers post-process fcurves as **separate animation entries** in the anim list. During sequence build it tries to:

1. **`buildMaterialAnim`** — apply `COLOR_FILTER_DIRECTIONAL`, `BLUR`, etc. as if they were UV/opacity material keys on scene materials.
2. **`buildAnimAt`** — resolve bone targets for tracks that reference **camera / lights**, not rig bones.
3. **`buildVisibility`** — hide/show nodes that were never imported into the Max scene.

When target resolution fails, code dereferences **null** → ACCESS_VIOLATION.

### Required fix (C++)

**Option A — Filter at parse time (preferred):**

In `parseAnimsAppend` (or wherever anims are registered from `nuccChunkAnm`):

- Do **not** register sub-tracks whose fcurve type is cinematic/post-process:
  - `FCURVE_TYPE_BLUR`, `GLARE`, `DOF`, `OMB`, `SHADOW`, `SOFTFOCUS`
  - `FCURVE_TYPE_COLOR_FILTER_DIRECTIONAL`, `COLOR_FILTER_RADIAL`, `BRIGHT_RATE`
- Do **not** register tracks bound to camera/light/ambient chunks as keyable bone anims.
- Keep only gameplay skeletal clips (`2kbxspl1_atk`, `_cut`, `_dmg`, etc.).

**Option B — Filter at apply time:**

In `buildAnimAt`, `buildMaterialAnim`, `buildVisibility`, `buildIdleKeys`:

- Before writing keys, resolve target node/material/controller.
- If target is missing → **skip silently** (log warning if debug enabled), never dereference null.
- Return 0 keys for skipped tracks; do not abort the whole sequence.

**Option C — Both** (best): filter at parse + null-safe apply as safety net.

### Acceptance tests

- [ ] Kabuto folder **with** `2kbxspl1.xfbin` → Import → Load all as sequence → **no crash**
- [ ] Sequence timeline contains note keys for base spl1 clips (`atk`, `cut`, `dmg`, …)
- [ ] Post-process sub-tracks (`_blur`, `_glare`, …) are **not** in the animation dropdown (or are skipped during sequence)
- [ ] Single **Apply selected** on `2kbxspl1_atk` still works
- [ ] Other characters (e.g. Pein, 104 anims) still work unchanged

---

## Bug 2 — Second import crashes at `buildMaterials`

### Reproduction

1. Import Kabuto (or any model) successfully with **Import textures** checked.
2. Either:
   - Click **Import** again with **Clear scene first** checked, or
   - Manually delete scene objects and import again.
3. **Result:** MAXScript Rollout Handler Exception at **`XfbinImport.mcr` line 587**:

   ```maxscript
   XfbinCpp.buildMaterials sTexDir
   ```

   Exception: **ACCESS_VIOLATION**, read address **`0x00000000`** (null pointer).

Status bar may still show `loading 2kbxbod1.xfbin ...` — crash happens during texture/material pass of the model loop, **before** sequence load.

### Current clear logic (insufficient)

```maxscript
if (chkClear.checked) then
(
  delete objects
  XfbinCpp.clearScene()
)
-- ...
XfbinCpp.clearAnims()   -- always called
-- buildMaterials called every import, every model file, if textures enabled
```

Problems:

1. **`clearScene()` only runs when checkbox is checked** — stale C++ state possible otherwise.
2. Even **with** checkbox, **`clearScene()` does not fully reset** the material/texture builder internal caches.
3. **`buildMaterials`** is invoked again on re-import and uses **dangling pointers** to materials/nodes from the previous import.

### Required fix (C++)

**In `clearScene()` (and ensure it is complete):**

Reset **all** session state, including:

- Bone / clump / instance registry
- Node handle → skeleton maps
- **Material builder cache** (created materials, texture slots, pending assignments)
- **Open file / container state** (or rely on `close()`)
- Animation registry should be cleared by `clearAnims()` — verify no cross-dependencies

**In `buildMaterials`:**

- Null-check every cached pointer before use (nodes, materials, texmaps, MtlBase refs).
- If scene was cleared or node no longer valid (`INode` null / handle not found) → rebuild from scratch, do not reuse stale cache.
- Consider idempotent behavior: safe to call multiple times on a fresh scene.

**Optional MAXScript hardening** (small improvement, not a substitute):

At start of `btnImport`, always:

```maxscript
XfbinCpp.close()
if chkClear.checked then delete objects
XfbinCpp.clearScene()
XfbinCpp.clearAnims()
```

This helps only if `clearScene()` is actually fixed in C++.

### Acceptance tests

- [ ] Import Kabuto with textures → Import again (Clear scene first) → **no crash**
- [ ] Import 3× in a row without restarting Max → **no crash**
- [ ] Materials still correct after each re-import
- [ ] `sceneBoneCount()` / `sceneClumpName()` safe on empty scene after `clearScene()`

---

## Reference: Kabuto folder layout

**Scanned (top-level only):**

| File | Role |
|------|------|
| `2kbxbod1.xfbin` | Main body model |
| `2kbxkiz.xfbin` | Accessory mesh |
| `2kbxbod1c/l/s.xfbin` | Animation + embedded clump |
| `2kbxskl1/2.xfbin` | Skill anims |
| **`2kbxspl1.xfbin`** | **Special move (problem file)** |
| `2kbx_x.xfbin`, `2kbxspl1_x.xfbin` | Effect extras (may not scan as anm/clump) |

**Not scanned** (subfolder `Extra\`): `2kbxacc1.xfbin`, `2kbxacc2.xfbin`, moved `2kbxspl1.xfbin`

---

## Reference: crash artifacts

CER folder (Mauri's PC):

`C:\Users\mauri\AppData\Local\Autodesk\CER\7437b8fc60175bb6ec52d170d8f2b9d276e333ec\6\`

- `3dsmax_minidump.dmp` — spl1 anim names in memory, `XfbinImport.dlu` loaded
- Exception code: `0xC000000D` (STATUS_INVALID_PARAMETER) on report #6; RIP in `3dsmax.exe+0x228DA7`

PDB path embedded in DLL:

`C:\Users\shank\Downloads\XFBIN_Import_1_9_2_source\XFBIN Import\build_2025\Release\XfbinImport.pdb`

Use this PDB with WinDbg to symbolize plugin offsets seen in dump (e.g. `XfbinImport.dlu+0x4CA40`).

---

## Suggested implementation order for vibecoder + AI

1. **Reproduce both bugs** with Kabuto folder in Max 2025.
2. **Fix `clearScene()` + `buildMaterials`** first — smaller scope, fixes daily workflow pain.
3. **Add fcurve type enum/filter** — grep source for `FCURVE_TYPE_` string literals already in binary (`BLUR`, `GLARE`, `DOF`, `COLOR_FILTER_DIRECTIONAL`, …).
4. **Add null-guards** on all `build*Anim*` target resolution paths.
5. **Bump version** to 1.9.3 in `PackageContents.xml`, `.mcr`, rebuild all target Max year DLLs.
6. Run existing script check: `python tools\PRUEFE_SCRIPTS.py` (mentioned in mcr header).

---

## Files to touch (expected)

| File | Changes |
|------|---------|
| C++ source (anim parser) | Skip cinematic fcurve types; optional warning log |
| C++ `clearScene()` | Full material/texture cache reset |
| C++ `buildMaterials()` | Null-safe, no stale pointer reuse |
| C++ `buildMaterialAnim` / `buildAnimAt` / `buildVisibility` | Null-safe target lookup |
| `XfbinImport.mcr` (optional) | Always call `close` + `clearScene` + `clearAnims` on import start |
| `PackageContents.xml` | Version bump |

---

## What NOT to do

- Do not “fix” spl1 only by hardcoding `2kbxspl1` filename — other characters will have the same fcurve types on special moves.
- Do not rely on MAXScript `try/catch` for ACCESS_VIOLATION — it will not catch native null derefs reliably.
- Do not disable Material anim globally — fix the underlying target resolution.

---

*Generated from live debugging session — 2026-08-27*
