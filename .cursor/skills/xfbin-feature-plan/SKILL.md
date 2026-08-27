---
name: xfbin-feature-plan
description: >-
  Plans new XFBIN Import features before coding. Use when the user asks for
  a new feature, enhancement, or non-trivial behavior change in this repo;
  requires posting a plan and waiting for approval before implementation.
---

# XFBIN feature planning

## When to use

- User requests a **new feature** or **behavior change** (not a one-line typo fix).
- Scope touches C++ API, sequence mode, import flow, or package layout.

## Workflow

1. **Read context** (do not code yet):
   - `AGENTS.md`, `XFBIN Import/DEVNOTES.md`, relevant `src/*.cpp` / `scripts/XfbinImport.mcr`
   - `CHANGELOG.md` for recent related changes

2. **Write a plan** in chat using this template:

```markdown
## Plan: [title]

### Goal
[One sentence]

### Approach
- [Bullet steps]

### Files
- [path] — [change]

### Risks / edge cases
- [e.g. hybrid clump+anm, spl1 post-process, Max 2025 menu callback]

### Testing
- [ ] PRUEFE_API / PRUEFE_SCRIPTS
- [ ] BAUE_ALLE + INSTALLIERE
- [ ] Max: import / sequence / re-import

### Version
- [ ] Needs 1.9.x bump + ProductCode + rebuild all DLUs

### Out of scope
- [what you will NOT do]
```

3. **Stop.** Ask: *“Approve this plan?”* Do not implement until the user confirms.

4. After approval: implement minimally, run verification from `xfbin-build-ship` skill.

## Maintainer rules (verbatim)

- Do **not** open PRs or push to GitHub without explicit user approval.
- Do **not** commit unless the user asks.
