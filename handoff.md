# Handoff pointer

**Current handoff:** [docs/handoffs/2026-08-27-sequence-over-skip.md](docs/handoffs/2026-08-27-sequence-over-skip.md)

Regression: 1.9.4 sequence over-skips body anim packs (`bod1c`/`bod1l`/`bod1s`) because `FileLooksCinematic` is file-wide on Camera/Trail/Light. Spl1 crash mitigation was too coarse.

**Status:** fixed in branch `fix/spl1-sequence-and-reimport-crash` (1.9.5–1.9.8), PR #2 open against `DennisHerrm/XFBIN-Import-3ds-max`.

**For the upstream maintainer / their AI:** [docs/handoffs/2026-08-29-for-maintainer-ai.md](docs/handoffs/2026-08-29-for-maintainer-ai.md) — compares this PR with the maintainer's own build, lists root causes, and points the maintainer's AI at things to self-check (hybrid folder scan, per-clip isolation, builder hardening coverage, versioning). Advisory, not prescriptive.
