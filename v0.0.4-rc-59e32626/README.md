# v0.0.4 release-candidate evidence — RC `59e32626`

Raw logs behind the release PR's matrix cells. The golden bundles themselves
live under `golden/` (`classic-maciix-2026-08-26/`, `macos-tahoe-2026-08-27/`,
`win32-x64-2026-08-27/`), each with its own PROVENANCE and SHA256SUMS.

- `macos-tahoe-bake-verify.log` — all ten macOS matrix cells baked then
  verified on tahoe at the RC; every cell audit byte-match + 0 differing pixels.
- `win32-omen-cell-*.log` — per-cell Win32 update/verify logs on omen at the RC
  (UTF-8-decoded from the PowerShell UTF-16 originals).
- `win32-omen-stage{A,B}-results.txt` — per-cell exit-code summaries.
- `win32-omen-496-*.txt` — the MineSweeper multistable-settle characterization
  behind #496 (repro frequencies and the captured incomplete board).
- `win32-omen-scrapbook-determinism.txt` — 8-run scrapbook startup content
  check (the ≤2-column #459 tie stays within compare tolerance).
- `ub2-assembly-rehearsal-manifest.txt` — the end-to-end macOS UB2 assembly
  rehearsal on tahoe at the same allowlist commit: the #489 standalone release
  stage (five loops + SimpleViewer, arm64+x86_64, per-arch verified by the
  stage), ditto-zipped and collected.
- `classic-assembly-rehearsal-manifest.txt` + `…-build.log.gz` — the
  end-to-end Classic release-assembly rehearsal at the allowlist commit
  `6e4c7378` (RC + allowlist file only): 68K and PPC Retro68 Release builds,
  12 entries collected, archive verified.
