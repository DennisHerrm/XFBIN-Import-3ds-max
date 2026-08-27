# Agent guide — XFBIN Import for 3ds Max

Instructions for AI agents working in this repository.

## Repository layout

| Path | Role |
|------|------|
| `XFBIN Import/` | **Source of truth** — C++ plugin, MAXScript UI, build scripts, verification tools |
| `XfbinImport/` | **Installable package** — prebuilt or copied `.dlu` per Max year + `PackageContents.xml` |
| `README.md` | User-facing install and usage |
| `XFBIN Import/DEVNOTES.md` | Internal architecture and dev decisions |
| `XFBIN Import/CHANGELOG.md` | Version history (update on every release) |
| `BUGREPORT_*.md` | Crash / bug write-ups (optional, keep at repo root) |

### Source map (C++)

| Area | Files |
|------|--------|
| MaxScript API / scene state | `src/xfbinimport.cpp`, `src/xfbinimport.h` |
| XFBIN container | `src/xfbin_reader.*` |
| Skeleton | `src/xfbin_clump.*` |
| Meshes / NUD | `src/xfbin_nud.*` |
| Textures / materials | `src/xfbin_tex.*` |
| Animations | `src/xfbin_anm.*` |
| UI (authoritative) | `scripts/XfbinImport.mcr` |
| UI (installed copy) | `../XfbinImport/Contents/MacroScripts/XfbinImport.mcr` — **keep in sync** after UI edits |

New MaxScript API functions must appear in **three** places: enum, `BEGIN_FUNCTION_MAP`, and interface descriptor in `xfbinimport.cpp`. Run `python tools/PRUEFE_API.py` from `XFBIN Import/`.

---

## Owner workflow (mandatory)

These rules come from the maintainer. **Do not skip them.**

### 1. Plan before new features

For **new features** (not trivial one-line fixes):

1. Read relevant code and `DEVNOTES.md`.
2. Post a **short plan**: goal, files touched, risks, test steps, version bump needed or not.
3. **Wait for explicit approval** before implementing.

Use skill: `.cursor/skills/xfbin-feature-plan/SKILL.md`.

### 2. No PR / push / fork publish without approval

- **Do not** open pull requests, push branches to GitHub, or fork-publish unless the user explicitly asks (e.g. “open a PR”, “push this”).
- Local work is fine: branch, commit (only if asked), build, test.
- When fixes are ready, summarize changes and ask: *“Should I push a branch / open a PR?”*

### 3. Commits

- **Do not commit** unless the user asks.
- When committing: concise message, focus on *why*; never force-push `main`.

### 4. Version bumps

On release-worthy changes:

- Bump `XFBINIMPORT_VERSION_STR`, both `PackageContents.xml` copies, `.mcr` header, `CHANGELOG.md`.
- **Regenerate `ProductCode`** in `PackageContents.xml` (new GUID per AppVersion).
- **Never change `UpgradeCode`.**
- Prefer `tools/VERSION_SETZEN.py` if maintained; otherwise mirror existing 1.9.3 pattern.

Rebuild **all** target Max years after C++ changes (`BAUE_ALLE.bat`), then `INSTALLIERE.bat`.

---

## How to investigate problems

### XFBIN file content (no Max required)

From `XFBIN Import/` after build:

```bat
xfbindump.exe path\to\file.xfbin
xfbindump.exe file.xfbin --anims-o dump.txt
```

Python cross-check vs Blender lib: `tools/pydump*.py`, `tools/VERGLEICHE*.bat`.

### 3ds Max crashes

- CER dumps: `%LOCALAPPDATA%\Autodesk\CER\<hash>\<n>\` — `dmpuserinfo.xml`, `3dsmax_minidump.dmp`
- Plugin must match Max year (wrong `.dlu` → crash, no message).
- Sequence issues: check `AnmEntryFormat` (bone vs camera/light/material) in `xfbin_anm.h`.
- Re-import issues: `clearScene()`, `sceneMaterials_`, `meshNodes_` in `xfbinimport.cpp`.

Use skill: `.cursor/skills/xfbin-crash-investigate/SKILL.md`.

### Code navigation

- **Codegraph** is indexed under `XFBIN Import/.codegraph/` (23 C++ source files). Re-init after large refactors: `codegraph init -i "XFBIN Import"`.
- **Prefer Codegraph MCP** (`codegraph_explore`, pass `projectPath` if the session workspace is not the source tree) for “how does X work” and call paths in `XFBIN Import/src/`.
- Use `Grep` / `Read` for exact strings or small edits.
- Do not duplicate long explore loops if codegraph is available.

---

## Verification before calling work “done”

From `XFBIN Import/`:

```bat
python tools\PRUEFE_API.py
python tools\PRUEFE_SCRIPTS.py
```

After C++ changes: `BAUE_ALLE.bat 2025` (or user’s Max year), `INSTALLIERE.bat`, manual test in Max:

1. Import folder (e.g. Kabuto / Pein test data).
2. Apply single animation.
3. Load all as sequence.
4. Import again with “Clear scene first” + textures.

---

## Common pitfalls

| Topic | Detail |
|-------|--------|
| Hybrid XFBIN files | Same file can have `nuccChunkClump` **and** `nuccChunkAnm` — both model and anim lists (not `else if` in `ScanFolder`). |
| spl1 / special moves | Post-process clips (Blur, Glare, DOF, ColorFilter) often have **no bone entries** — must not run visibility/material/bone keying as full sequences. |
| `clearScene()` | Clears bookkeeping only, not Max nodes — must also clear `meshNodes_` / stale `Mtl*` maps before re-import. |
| MAXScript | ASCII only, `--` comments, functions **before** callers in rollouts, no `continue`. |
| UI sync | Edit `scripts/XfbinImport.mcr`, copy to `XfbinImport/Contents/MacroScripts/` and `package/...` if those trees exist. |

---

## Project skills (`.cursor/skills/`)

| Skill | Use when |
|-------|----------|
| `xfbin-feature-plan` | New feature requested — produce plan, wait for approval |
| `xfbin-build-ship` | Build, install, version bump |
| `xfbin-api-change` | Adding/changing `XfbinCpp.*` API |
| `xfbin-crash-investigate` | Max crash or sequence/import failure |
| `xfbin-inspect-xfbin` | Inspecting `.xfbin` bytes without Max |

---

## External references

- Upstream: https://github.com/DennisHerrm/XFBIN-Import-3ds-max
- Format reference: [Blender XFBIN Importer](https://github.com/Al-Hydra/Blender-XFBIN-Importer)
- Test assets: user-provided CC2 `.xfbin` folders (e.g. Kabuto `2kbx` — special-move file `2kbxspl1.xfbin`)
