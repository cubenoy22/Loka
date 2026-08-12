# Autopilot PR D gate record

## Scope ruling

The 2026-08-12 implementation ruling makes automated movies optional. Manual
video remains acceptable, while the Toolbox presentation lane needs still
captures at the stable inspection points. The existing seven-scenario MAME
rail already produces and verifies one MAME-owned PNG at each such point.

PR D therefore aggregates those seven finalized PNGs. It does not add MAME
AVI/MNG recording, a visible hold/resume lifecycle, or another guest control
channel. The existing SnapRecord, completion marker, three-identical-frame
settle, crop, and rig-local golden remain the machine-verdict authority.

## Proposed ownership and commit boundary

- `run-scenario.sh` continues to own one scenario execution and its finalized
  cropped PNG under `build/mame-scenario/<scenario>/`.
- A tracked host-side scenario registry is the one enumeration used by CTest,
  the single-scenario validator, and the presentation rail.
- The presentation rail owns a new run-local `<run-id>.incomplete` directory.
  It copies only finalized scenario PNGs into that directory and hashes them.
- After every registered scenario passes and every PNG is present, the rail
  finalizes its manifest and renames the whole directory to `<run-id>`.
- Failure publishes no completed presentation run. The incomplete directory
  and the failing scenario work directory remain available for diagnosis.

## Shape Review — Gate 1

### Ranked candidates and rulings

1. **Reuse the seven settled MAME snapshots — adopt.** They are already taken
   through the emulator display seam at the scenario's stable completion point.
   A second screenshot mechanism would duplicate timing and crop decisions.
2. **One host-side scenario registry — adopt.** Adding a third handwritten
   seven-name list for the rail would create another manual step for every new
   scenario. CTest and the single-scenario validator should consume the same
   registry.
3. **Run-local incomplete directory followed by atomic rename — adopt.** A
   presentation run is a completed fact. A failed rerun must not destroy or
   partially replace an earlier completed archive.
4. **MAME AVI/MNG recording — reject for this scope.** Manual video is allowed,
   and key-point stills satisfy the presentation need without another recorder
   lifecycle or finalize failure mode.
5. **Visible hold/resume control — reject for this scope.** It adds a control
   door and emulator lifetime state without improving the requested still
   evidence. Existing per-scenario artifacts remain available for inspection.
6. **Make presentation captures a new machine oracle — reject.** The copied
   files are human evidence. Existing SnapRecord and pixel-golden checks remain
   authoritative and must pass before a capture can enter the completed archive.

### Gate questions

- **Repeated doors:** capture timing, settle, crop, and verdict stay in the
  existing single-scenario procedure. The rail only sequences and collects.
- **Absent walls:** an unknown scenario, failed scenario, missing PNG, hashing
  failure, manifest failure, or final rename all fail closed. No completed run
  directory is published.
- **Boxes and solid lines:** the scenario runner owns each source work directory;
  the rail owns one incomplete archive; the final rename transfers only a
  completed immutable archive to the reviewer.
- **Next resident:** a new scenario is inserted once in the registry and is then
  included by both CTest and the presentation rail.
- **Test-only API pressure:** no shipping API, Toolbox application hook, or
  guest-side state is added for observability.

## Complexity gate

One risk flag: multi-scenario tooling now has an explicit failure-atomic publish
boundary. There are no new shipping lifecycle paths, State/Flow changes,
cross-boundary pointers, callbacks, platform handles, mutable flags, or
`dangerously*` APIs.

## Shape Review — Gate 2

### Added shape and existing parallels

- `tests/toolbox/scrapbook-scenarios.txt` is the single scenario enumeration.
  It replaces the parallel lists previously embedded in CMake and
  `run-scenario.sh`; the presentation rail consumes that same fact.
- `run-presentation-rail.sh` is one host-side orchestration door. It parallels
  `run-scenario.sh` but does not copy its emulator, settle, crop, or verdict
  mechanism: it invokes that procedure and collects only its finalized PNG.
- `LOKA_TOOLBOX_PRESENTATION_RUN_ID` is the only new input. It names one archive
  for reproducible tests; its restricted filename grammar prevents it from
  becoming a path or traversal input.
- `<run-id>.incomplete` and `<run-id>` are the only new lifecycle states. The
  rail owns the former and one same-filesystem rename publishes the latter.
- `presentation-manifest.txt` parallels the existing artifact manifests only as
  a small immutable capture index: version, run ID, ordered capture hashes,
  scenario count, and result. It does not claim the source checkout SHA or
  replace the future common rig manifest.
- `ToolboxPresentationRailTest.sh` parallels the existing script-level harnesses
  and pins success, hash content, completed-run preservation, scenario failure,
  missing capture, and inherited-stdin isolation.
- CMake adds one script test, reads the registry for MAME test registration, and
  marks the registry as a configure dependency. No virtual, member field,
  shipping API, guest state, or native call site is added.
- `docs/MAME_DEVELOPMENT.md` documents the one command and the incomplete/final
  artifact boundary; it adds no second contract.

### Reclamation and configuration audit

- No shipping detach, retirement, reclamation, or teardown path changes.
- The only cleanup-like transition is the success-only directory rename. Every
  failure before it retains the incomplete archive; an existing completed run
  ID is refused before any write, and a focused test pins preservation.
- The CMake registration remains under the existing non-Windows, non-Apple,
  non-Classic test-tooling gate. In configurations where it is absent, no
  shipping safety check disappears; direct scripts keep their fail-closed
  validation.

### Claims and evidence audit

- Bounded spike: the existing `open-first-page` scenario produced, cropped, and
  golden-compared its MAME-owned PNG successfully.
- Pre-fix rail failure: the first real rail published after one scenario because
  the child inherited and consumed the registry stdin. The focused regression
  consumed stdin deliberately and failed with missing `beta.png` before the
  fix; redirecting each child stdin from `/dev/null` turned it green.
- Real rail after the fix: all seven scenarios passed, seven PNGs were visually
  inspected, and all seven manifest SHA-256 values matched the collected bytes.
- Local suite: 348/348 tests passed, including
  `scriptToolboxPresentationRail`.

### Findings deliberately left outside this PR

- Automated QuickTime and MAME movie capture remain parked by the user ruling;
  manual video is acceptable and movies are not a machine oracle.
- Visible MAME hold/resume remains unnecessary for key-point still evidence and
  would add a separate control lifecycle.
- The presentation manifest deliberately does not assert which Git SHA produced
  an already-built Retro68 binary. PR E's outer orchestrator is the correct
  owner for build/SHA provenance and the common run manifest.
