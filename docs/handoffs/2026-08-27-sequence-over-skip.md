# Handoff — XFBIN Import sequence over-skip (1.9.4)

**Date:** 2026-08-27  
**For:** fresh Claude/Cursor session  
**Owner preference:** plan before new features; **no PR without explicit approval**; commits only when asked.

---

## Repo / branch / remotes

| Item | Value |
|------|--------|
| Local path | `D:\Stuff\Warcraft_3_All_Assets\Projects\XFBIN-Import-3ds-max` |
| Active branch | `fix/spl1-sequence-and-reimport-crash` |
| Upstream (`origin`) | https://github.com/DennisHerrm/XFBIN-Import-3ds-max |
| **Correct fork (`fork`)** | https://github.com/Lufion1420/XFBIN-Import-3ds-max |
| Old wrong fork | `fork-maurice-tpg` → Maurice-TPG (user may delete on GitHub) |
| Installed plugin | `%APPDATA%\Autodesk\ApplicationPlugins\XfbinImport` → **1.9.4** |
| Max version | **2025** |
| Cursor tip | Open the **GitHub clone** as workspace, not the AppData install folder |

Latest commits on branch:

- `ae71187` — gitignore scratch files  
- `59253df` — **Skip cinematic FX bundles…** (introduced the regression)  
- `68a25f5` / `3754384` — AGENTS.md + skills  
- `2e951e0` — original 1.9.3 spl1/`clearScene` fix (insufficient alone)

**No PR open** (earlier Maurice-TPG PR was closed on purpose).

---

## What the user reports (current bug)

After installing **1.9.4**, **“Load all as sequence” no longer loads all animations** (many / most clips missing vs before).

Crash with `2kbxspl1` present was the prior issue; 1.9.3 did **not** fix the crash; 1.9.4 stopped the crash path by over-filtering.

Test model:

`C:\Users\mauri\Desktop\Model -Conversions WIP\Kabuto\KabutoRedCloak`

---

## Root cause (diagnosed — do not rediscover from scratch)

### Why 1.9.3 failed

`2kbxspl1.xfbin` Anm clips **all have bone entries**. Blur/Glare/DOF are **Binary `.fcv` chunks**, not bone-less Anm clips.  
`animIsSkeletal` / `HasBoneEntries()` → always 1 → sequence still ran `buildVisibility` / `buildMaterialAnim` / `buildAnimAt` → crash `c000000d` (CER #7).

### Why 1.9.4 “fixed” crash but broke sequence

`FileLooksCinematic()` in `XFBIN Import/src/xfbin_anm.cpp` marks an **entire XFBIN file** as cinematic if it contains **any** of:

- `nuccChunkCamera`, `LightDirc`, `LightPoint`, `Ambient`, `Billboard`, `Trail`
- or `nuccChunkBinary` name matching needles: `blur`, `glare`, `dof`, `colorfilter`, `bright_rate`, `_omb`, **`shadow`**, …

Then **every** Anm from that file gets `cinematicSource = true` → `IsSequenceSafe() == false` → sequence skips them.

Scan of Kabuto folder (`docs/handoffs/_seq_false_positive_scan.txt`):

| File | Flagged? | Anm count | Why |
|------|----------|-----------|-----|
| `2kbxspl1.xfbin` | YES (wanted) | 14 | Camera/lights + Binary FX |
| **`2kbxbod1c.xfbin`** | **YES (false positive)** | **37** | Camera + Trail + LightDirc |
| **`2kbxbod1l.xfbin`** | **YES (false positive)** | **50** | Trail + Camera + Light + Binary FX names |
| **`2kbxbod1s.xfbin`** | **YES (false positive)** | **6** | Camera + Trail |
| `2kbxskl1` / `skl2` | NO | 1 each | OK |

≈ **93 legitimate clips dropped** from sequence because body hybrid packs share the file with a bit of camera/trail/light. File-wide flag is too coarse.

UI gate: `XFBIN Import/scripts/XfbinImport.mcr` ~L747 uses `XfbinCpp.animIsSequenceSafe` (not `animIsSkeletal`).

---

## Intended product behavior

1. Sequence must include normal combat / body anim packs (bod1c/l/s, skl*, etc.).  
2. Sequence must **not crash** with `2kbxspl1` in the folder.  
3. Prefer skipping only truly unsafe clips (or only `*spl1*` cinematic bundles), not every Anm that shares a file with a camera chunk.  
4. Single-animation apply can still allow inspection of special-move clips if useful.  
5. Re-import / `clearScene` / `meshNodes_` / material null-check from 1.9.3 must remain.

---

## Suggested fix direction (plan with user before large rewrites)

Tighten classification — **do not keep file-wide “any Camera/Trail ⇒ all Anms unsafe”**.

Options (pick with user / prefer least brittle):

**A (recommended first cut):** File-level flag only when Binary post-process FCVs present **or** filename/path matches special-move pattern (`spl1`), **not** on mere Camera/Trail/Light alone.  
**B:** Per-Anm: unsafe if `HasCameraOrLightEntries()` **or** (optional) material entry count / foreign-clump ratio extreme — still allow pure bone clips in hybrid files.  
**C:** Hardening-only in `BuildAnimAt` / visibility / material (NaN guards already partially there) so cinematic hybrids can stay in sequence without crashing — harder, needs repro isolation.  
**D:** Combo: A or B for skip + keep float guards.

Also remove over-broad needle **`shadow`** if it false-positives on normal Binary names.

After code change: bump **1.9.5**, new `ProductCode`, rebuild `BAUE_ALLE.bat 2025`, `INSTALLIERE.bat`, Max closed.

---

## Key files

| Path | Role |
|------|------|
| `XFBIN Import/src/xfbin_anm.cpp` | `FileLooksCinematic`, `IsSequenceSafe`, parse flag |
| `XFBIN Import/src/xfbin_anm.h` | `cinematicSource`, API on `Anm` |
| `XFBIN Import/src/xfbinimport.cpp` / `.h` | `animIsSequenceSafe` MaxScript API |
| `XFBIN Import/scripts/XfbinImport.mcr` | Sequence button skip |
| `XFBIN Import/tools/PRUEFE_API.py` / `PRUEFE_SCRIPTS.py` | Must pass after API changes |
| `AGENTS.md` | Agent rules |
| `.cursor/skills/xfbin-*` | Project skills |
| `BUGREPORT_spl1_and_reimport.md` | Original crash write-up (partially outdated on “no bone entries”) |

Build: VS + Max **2025 SDK** already installed at  
`C:\Program Files\Autodesk\3ds Max 2025 SDK\maxsdk`.

Codegraph index: `XFBIN Import/.codegraph/` (gitignored). Prefer `codegraph_explore` with `projectPath` set to that folder.

---

## How to verify

1. Close Max → build/install 1.9.5 → `XfbinCpp.version()` = 1.9.5  
2. Import Kabuto folder **with** `2kbxspl1.xfbin` present  
3. **Load all as sequence**  
   - Must **not** crash  
   - Must load bod1c/l/s (+ skl) anims onto timeline (not ~only a handful)  
   - Listener may still report skipped spl1 clips  
4. Control: move `2kbxspl1` to `Extra\` → sequence still complete  
5. Re-import / clear scene still OK  

---

## Stash / out of scope

- Do **not** open PR until user approves after test  
- Do **not** push to Maurice-TPG  
- Do **not** commit AppData install tree or `output/` / `build_2025/`  
- EU AI Act / Shopify rules irrelevant here  
- User may still have Desktop `XFBIN_Import.ms` (old launcher only — ignore for this bug)

---

## NEXT PROMPT (paste into new session)

```
Continue XFBIN Import from handoff:

Read: D:\Stuff\Warcraft_3_All_Assets\Projects\XFBIN-Import-3ds-max\docs\handoffs\2026-08-27-sequence-over-skip.md
Also: AGENTS.md and docs/handoffs/_seq_false_positive_scan.txt

Workspace: D:\Stuff\Warcraft_3_All_Assets\Projects\XFBIN-Import-3ds-max
Branch: fix/spl1-sequence-and-reimport-crash
Fork remote: Lufion1420 (not Maurice-TPG). No PR unless I approve.

Problem: 1.9.4 "Load all as sequence" skips most animations. FileLooksCinematic is too broad — body packs 2kbxbod1c/l/s (~93 clips) are flagged because they contain Camera/Trail/Light, while only 2kbxspl1 should be treated as the unsafe cinematic/FX bundle.

Fix: tighten sequence-safe classification (prefer not file-wide on mere Camera/Trail), keep spl1 from crashing sequence, bump to 1.9.5, rebuild+install Max 2025, tell me how to retest. Plan briefly then implement. Do not open a PR.
```
