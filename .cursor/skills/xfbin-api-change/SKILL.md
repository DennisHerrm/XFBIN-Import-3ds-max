---
name: xfbin-api-change
description: >-
  Adds or changes XfbinCpp MaxScript API in the XFBIN Import plugin. Use when
  exposing new C++ functions to MAXScript or renaming buildAnim/clearScene APIs.
---

# XFBIN C++ API changes

Every new `XfbinCpp.*` function needs **four** updates:

## 1. Header enum (`src/xfbinimport.h`)

Add `fn_yourName` to the enum (order matters for IDs).

## 2. Function map (`src/xfbinimport.h`)

Add `FN_n(fn_yourName, TYPE_..., YourMethod, ...)` inside `BEGIN_FUNCTION_MAP`.

## 3. Interface descriptor (`src/xfbinimport.cpp`)

Add block like:

```cpp
fn_yourName, _T("yourName"), 0, TYPE_INT, 0, N,
    _T("arg"), 0, TYPE_STRING,
```

## 4. Implementation

Method on `XfbinImportInterface` in `.cpp` / declaration in `.h`.

Document in the API comment block at top of `xfbinimport.h`.

## 5. MAXScript linter list

Add name to `PLUGIN_API` set in `tools/PRUEFE_SCRIPTS.py`.

## 6. Verify

```bat
python tools\PRUEFE_API.py
```

Must print: *Alle Funktionen sind an allen drei Stellen eingetragen.*

## UI usage

If the UI calls the new API, update `scripts/XfbinImport.mcr` and sync installed copies.

## Release

API changes require **DLU rebuild** for every supported Max year — MAXScript-only callers will see `Unknown property` until rebuild.
