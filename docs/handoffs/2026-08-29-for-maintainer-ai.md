# Handoff — for the maintainer (and the maintainer's AI)

**Date:** 2026-08-29
**From:** Lufion1420's fork, branch `fix/spl1-sequence-and-reimport-crash`
**Re:** PR to `DennisHerrm/XFBIN-Import-3ds-max` — "Fix 'Load all as sequence' over-skipping and the 2kbxspl1_atk crash" (PR #2)

This is context, not a demand. The repo is yours; merge, adapt, or ignore any
of it. What we'd ask: **before landing your own version, sanity-check it
against the points in "For your self-evaluation" below** — we hit a few sharp
edges that a binary diff won't surface.

---

## 1. The shared problem

`2kbxspl1.xfbin` (Kabuto special move) crashed **Load all as sequence**.
1.9.3 didn't fix it; 1.9.4 stopped the crash by having `FileLooksCinematic`
mark the whole XFBIN as cinematic and skipping every Anm in it. That was too
coarse: it also dropped ~93 legitimate skeletal clips from `2kbxbod1c/l/s`
and every usable clip inside `2kbxspl1` itself.

Test data: `KabutoRedCloak` folder — `2kbxbod1c` (37 clips), `2kbxbod1l`
(50), `2kbxbod1s` (6), `2kbxskl1/2` (1 each), `2kbxspl1` (14).

---

## 2. Root causes we nailed down (transferable regardless of approach)

### 2a. `FileLooksCinematic` is file-wide

Any `nuccChunkCamera/LightDirc/LightPoint/Ambient/Billboard/Trail` chunk, or a
`nuccChunkBinary` whose name contains a post-process needle
(`blur/glare/dof/colorfilter/...`), flags the entire file. `2kbxbod1c/l/s`
each carry a stray camera/trail chunk beside dozens of pure bone clips, so
all of them were excluded. See `docs/handoffs/_seq_false_positive_scan.txt`
for the per-file scan.

### 2b. The actual crash: a CRT buffer overflow, not the animation math

From the crash minidump (`%LOCALAPPDATA%\Autodesk\CER\<hash>\<n>\`):

- Exception `0xC000000D` (STATUS_INVALID_PARAMETER), 0 parameters
- Faulting address in **`ucrtbase.dll`** — the C runtime, not the plugin's
  animation code, and **not** an access violation

`BuildAnimAt`, after keying, builds a warning listing every clump the clip
references that isn't in the scene:

```cpp
wchar_t wbuf[420];
swprintf_s(wbuf, L"%d Eintraege gehoeren zu Clumps ... (%s) ...",
           foreignClump, Cp932ToWide(foreignNames).c_str());
```

`2kbxspl1_atk` is a monster: 803 entries, 105 clump refs, 700 coord parents,
and it names **~40** out-of-scene clumps (`2efb_edt_*`, `1efc_dmy01`,
`2kbxsnk01`, `2ddwbod1`, `2itwbod1`, `2kzwbod1`, `wrinkles`, …). The name
string is ~640 wchars; it overruns `wbuf[420]`, and the secure CRT reacts to
overflow by invoking the invalid-parameter handler → `RaiseException`. A
MAXScript `try/catch` cannot catch that.

**Both of us independently landed the same fix for this** — build the warning
via `std::wstring` instead of a fixed `swprintf_s` buffer. Your build already
has the `std::wstring` form of that exact string. Good sign.

### 2c. `2kbxspl1_atk` is the only clip that animates `2ddwbod1 / 2itwbod1 /
2kzwbod1`

`xfbindump --anims` across all 6 anim-bearing files in the folder: those
three guest rigs appear in exactly one clip, `2kbxspl1_atk` (~134 / 113 / 116
bone entries). So whatever the sequence filter is, it must let `_atk` through
or that animation is lost. Of the 14 `2kbxspl1` clips, only `_atk` and `_cut`
carry camera/light entries; the other 12 are bone + material only.

---

## 3. What our PR does (branch `fix/spl1-sequence-and-reimport-crash`,
commit `1683631`, version 1.9.4 → 1.9.8)

- **Removed `FileLooksCinematic` and `Anm::cinematicSource` entirely.**
- `IsSequenceSafe()` is now just `HasBoneEntries()`. Rationale: the four
  builders (`BuildAnimAt / BuildVisibility / BuildMaterialAnim /
  BuildIdleKeys`) already iterate `kEntryBone` only and skip foreign clumps,
  so camera/light/ambient entries in a clip are ignored, not a reason to drop
  it. This is what lets `_atk` back into the sequence.
- **Per-clip `try/catch` in the sequence loop** (`XfbinImport.mcr`): one clip
  that raises a MAXScript error is skipped + logged; the run continues.
  Status line reports `X of Y animations` and an errored count.
- **Crash fix (2b):** cap `foreignNames` while collecting (12 names / 240
  chars, `", ..."` appended), build the `foreignClump` / `unmatched`
  warnings via `std::wstring`, and switch the name-bearing `Log` lines in
  `BuildAnimAt` / `BuildVisibility` to `_snwprintf_s(_TRUNCATE)`.
- Fixed a pre-existing `.mcr` bug: the skipped-clip `format` line was missing
  its `\` continuation → `format: not enough arguments`.
- Kept every 1.9.3 fix (clearScene-before-delete, hybrid clump+anm dual
  listing in the folder scan, `buildMaterials` null-check + `try/catch`).

Full per-version breakdown in `XFBIN Import/CHANGELOG.md` (1.9.5–1.9.8).

---

## 4. Where our approach and yours diverge (from a binary/`.mcr` diff of the
build on the user's desktop, labeled 1.9.4)

| Aspect | This PR | Your build |
|---|---|---|
| Crash fix (2b) | cap + `std::wstring` + `_snwprintf_s` | `std::wstring` form present — **convergent** |
| Cinematic classification | removed | also absent |
| Sequence clip filter | `IsSequenceSafe() = HasBoneEntries()` gate + per-clip `try/catch` | **no filter, no per-clip guard** — every clip runs, relies on hardened builders |
| `animIsSequenceSafe` / `animIsSkeletal` API | kept | **removed** |
| Re-import cleanup | `clearScene()` first, always | **new `pruneScene()` + `close()`**; `clearScene()` only on "Clear scene first" |
| Folder scan for hybrid (clump+anm) files | independent `if` / `if` | reverted to `else if` |
| `buildMaterials` guard in `.mcr` | `try/catch` kept | removed |

We think **`pruneScene()` is a real improvement** over "always clearScene" —
it handles the user hand-deleting nodes between imports, which ours doesn't.
Worth keeping in whatever lands.

---

## 5. For your self-evaluation (please verify on your side)

Not assertions that you're wrong — these are the specific things we couldn't
check from a binary and that bit us during this work:

1. **Folder scan `else if (iAnm > 0)`.** Kabuto `2kbxbod1c/l/s` and
   `2kbxspl1` carry `nuccChunkClump` **and** `nuccChunkAnm` in one file. With
   `else if`, are their animations still loaded? If hybrid handling moved
   into C++ `ScanFolder`, fine — but confirm the dropdown clip count matches
   (bod1c 37 + bod1l 50 + bod1s 6 + skl 2 + spl1 14). If it's short, this is
   the pre-1.9.3 regression returning.

2. **No per-clip isolation in the sequence loop.** If one clip raises a
   MAXScript-level error mid-run (the `format`-continuation bug did exactly
   this), the whole sequence aborts and the user loses everything built so
   far. Is "the C++ builders never throw / never fault" actually guaranteed
   across all CC2 data, or just for the Kabuto set? A cheap per-clip
   `try/catch` costs nothing and turns a dead run into one skipped clip.

3. **Builder hardening coverage.** `2kbxspl1_atk` exercises: 105 clump refs
   (most out-of-scene), ~40 distinct foreign clump names, camera + 2 lights +
   ambient entries, 68 material entries, references to 3 other character
   skeletons. Confirm each builder is safe against: foreign/absent clumps,
   NaN/Inf keys, `kEntryCamera/Light/Ambient` entries, and the warning-string
   length (2b). We found NaN guards at `BuildAnimAt` and `BuildMaterialAnim`;
   check `BuildVisibility` / `BuildIdleKeys` too.

4. **Versioning.** The desktop build still reports `1.9.4` with a *new*
   `ProductCode`. Same `AppVersion` + changed `ProductCode` + shared
   `UpgradeCode` can confuse Max's package resolution on upgrade/downgrade.
   Bump the version string if the binary changed.

5. **Is dropping the `.mcr`-level filter the intended architecture?** It's
   cleaner long-term *if* the builders are provably crash-proof — it ends the
   1.9.4→1.9.8 "re-tune the filter" cycle. If so, keep a trivial
   `HasBoneEntries()` skip anyway: a pure camera/FX clip has nothing to add
   to a skeletal sequence, so running four builders over it is wasted work,
   not safety.

---

## 6. A possible combined result

- **From your build:** `pruneScene()` + `close()` re-import model; any extra
  builder hardening.
- **From this PR:** the per-clip `try/catch`, the `HasBoneEntries()` gate,
  the `foreignNames` length cap (your `std::wstring` fix stops the crash but
  still emits a ~640-char warning line).
- **Both already agree:** kill `FileLooksCinematic` / `cinematicSource`, fix
  the foreign-clump warning via `std::wstring`.
- **Conflict to resolve:** folder scan — `if`/`if` unless hybrid handling is
  confirmed in C++.

To merge properly we'd need your **source / branch / patch**, not just the
built `.dlu`s — happy to rebase our branch onto whatever you push.

---

## 7. How to reproduce / verify

1. 3ds Max 2025, plugin installed, import the `KabutoRedCloak` folder **with
   `2kbxspl1.xfbin` present**.
2. Apply single animation — spot-check a normal clip and `2kbxspl1_atk`.
3. Load all as sequence:
   - no crash, no `Sequence aborted`
   - clip count on the timeline ≈ full bod/skl set **plus** the 12–14 spl1
     clips
   - `2ddwbod1 / 2itwbod1 / 2kzwbod1` pick up keys (from `_atk`)
   - a warning naming out-of-scene clumps is expected, not an error
4. Control: move `2kbxspl1.xfbin` aside → sequence unchanged otherwise.
5. Re-import with and without "Clear scene first" (+ textures) → no
   `buildMaterials` null deref, no stale-node crash.
6. `python XFBIN\ Import/tools/PRUEFE_API.py` and `PRUEFE_SCRIPTS.py` pass.

Offline inspection without Max: `XFBIN Import/build_tool/Release/xfbindump.exe
<file> --anims-o out.txt --no-keys`.
