# Autopilot PR E gate record

## Scope and common sequence

The macOS and Toolbox vertical slices now prove the same host-owned sequence:

1. resolve one requested ref to an exact commit and prepare an isolated checkout;
2. run the adapter-specific preflight and build;
3. run the adapter-specific machine-verdict procedure;
4. collect only finalized artifacts and write one common result vocabulary;
5. clean up only after artifact collection and manifest finalization succeed.

PR E gives that sequence one owner. VM/SSH/Aqua, Retro68/MAME/HFS, scenario
selection, crop, settle, and golden comparison remain inside their adapters.

The outer command discovers a rig through one registered adapter descriptor and
passes only `rig ID + ref + mode + optional local config`. It does not expose VM
names, MAME options, scenario internals, or transport details.

## Common result vocabulary

Every completed manifest reports:

- adapter and rig identity;
- requested ref and resolved commit SHA;
- mode and overall result;
- failure stage/message;
- build verification, runtime verification, and machine verdict;
- presentation status and recording status as separate facts;
- target/workdir retention and the next diagnostic action;
- start/end time and hashes of finalized artifacts.

`machine_verdict` follows the runtime verification fact, not the overall result.
A later presentation, collection, manifest, or cleanup failure may therefore
produce `result=failed` with `machine_verdict=passed`. This preserves the two
lanes instead of rewriting an already-completed machine fact.

## Shape Review — Gate 1

### Ranked candidates and rulings

1. **Shared lifecycle executor + immutable result snapshot — adopt.** Both
   adapters already perform the five steps above and share success-only cleanup
   semantics. One executor removes duplicated failure/manifest/cleanup ordering,
   while an immutable snapshot keeps result fields from drifting.
2. **One top-level `loka-rig` dispatcher — adopt.** It owns only rig discovery
   and common arguments. Adapter-specific options remain on direct adapter test
   doors rather than leaking into public vocabulary.
3. **Toolbox specified-SHA adapter — adopt.** PR D intentionally left build/SHA
   provenance to PR E. The adapter owns a detached checkout, Retro68 build,
   rig-local golden staging, existing seven-scenario rail, and collection.
4. **Shared descriptor schema for every platform — reject for now.** The common
   fields are real, but macOS VM facts and Toolbox MAME facts are not. Each
   adapter keeps a strict schema; the dispatcher discovers descriptors without
   parsing their platform payload.
5. **Generic command arrays/config-driven pipelines — reject.** Build and launch
   steps are not data. Turning them into descriptor commands would hide
   ownership, quoting, security, and platform failure boundaries.
6. **Move settle/capture/golden logic into common core — reject.** These are
   Toolbox mechanism; macOS has a different target-local completion contract.
7. **One universal archive layout beyond the common manifest — reject.** The
   manifest vocabulary is shared; adapter artifacts retain meaningful native
   names. Consumers can enumerate hashes without forcing fake symmetry.

### Gate questions

- **Repeated doors:** the shared executor owns prepare/build/run/collect,
  failure recording, manifest finalization, success-only cleanup, and final
  reporting. Adapters provide mechanisms, not parallel orchestration policy.
- **Absent walls:** unknown/duplicate rigs, unsupported modes, invalid local
  paths, missing goldens, build/runtime/capture failure, manifest failure, and
  cleanup failure all fail closed. A machine pass remains recorded when a later
  lane fails.
- **Boxes and solid lines:** the common executor owns `RunProgress`; each adapter
  owns its checkout/target and artifacts until collection; `RunResult` is an
  immutable completed snapshot; the archive owns finalized files thereafter.
- **Next resident:** a new platform registers one descriptor directory and one
  adapter entry, then implements the narrow lifecycle protocol. It does not
  copy the executor or public parser.
- **Test-only API pressure:** this is repository tooling only. No shipping API,
  Node/Boundary state, native handle, callback, or `dangerously*` use is added.

## Complexity gate

Two risk flags: cross-platform tooling boundary and public tooling vocabulary.
The change does not touch shipping State/Flow/Boundary/Platform code, target UI
lifecycle, or cross-boundary mutable state. Focused lifecycle/order/failure
tests and one real run through each adapter are required before PR publication.

## Shape Review — Gate 2

### Added doors, fields, and call sites

- `scripts/loka-rig.py` adds `AdapterRegistration`, `find_adapter`, and
  `run_adapter`. They parallel the two existing adapter descriptor directories
  and direct `run_rig` doors; they add only exact-one discovery and dispatch.
- `RigAdapter` adds the common `prepare`, `build`, `run_runtime`, `collect`,
  `best_effort_collect`, `note_failure`, `finalize_manifest`, and
  `cleanup_success` doors. Each door parallels one existing macOS private stage
  and one existing Toolbox presentation-rail phase. The common executor is the
  only call site that orders them.
- `RunProgress` adds the mutable per-run fields `started_at`, `ended_at`,
  `failure_stage`, `failure_message`, `build_passed`, `runtime_passed`,
  `presentation_collected`, and `manifest_finalized`. They replace the former
  loose macOS bookkeeping fields and are owned by exactly one adapter run.
- Immutable `RunResult` adds the common manifest facts. It parallels the former
  macOS manifest vocabulary, separates presentation from machine evidence, and
  is constructed only at adapter manifest-finalization doors.
- `ToolboxRigRun` adds repository, descriptor, local mapping, requested ref,
  mode, progress, resolved SHA/run ID, archive, checkout, retention state,
  scenarios, and command-log fields. These parallel `MacOSRigRun`; checkout and
  archive ownership remain explicit and adapter-local.
- `RigDescriptor` and `LocalMapping` for Toolbox parallel the macOS split
  between tracked facts and machine-local paths. They intentionally do not
  share one platform-agnostic descriptor schema.
- `stage_goldens` and `_read_scenarios` parallel the existing presentation
  rail's registry/golden inputs. The detached checkout's registry is the single
  scenario source; failure-atomic `golden.tmp` staging commits only after every
  rig-local golden is present and regular.
- The CMake `scriptLokaRig` call site parallels existing script protocol tests.
  The top-level README and platform workflow notes point ordinary users at the
  common entry point; adapter-direct controls remain test-only.

No virtual C++ doors, app-facing fields, State/Flow/Boundary edges, native
handles, callbacks, or `dangerously*` calls were added.

### Reclamation and failure-path review

- Success reaches cleanup only after runtime, presentation collection, and a
  first finalized manifest. Both adapters then rewrite the manifest with the
  observed removed target state.
- Prepare, build, runtime, and collection failures retain any created target or
  checkout and attempt incomplete artifact collection. They never call cleanup.
- First-manifest failure never calls cleanup. This is pinned by
  `test_manifest_failure_prevents_cleanup`.
- Cleanup failure records `result=failed`, retains the target fact, and rewrites
  the manifest without erasing completed machine or presentation evidence. This
  is pinned by
  `test_cleanup_failure_rewrites_result_without_erasing_completed_evidence`.
- Toolbox has one removal path: common success calls adapter
  `cleanup_success`, which calls `git worktree remove`; no build/runtime loop
  removes a checkout. macOS retains its one existing success release path.
- Manifest files are failure-atomic (`run-manifest.txt.tmp` then replace), are
  excluded from their own hash census, refuse duplicate fields, and refuse
  symlink artifacts.

### Configuration and claim audit

- This is host inspection tooling; it does not ship and does not alter any
  target-language build configuration. Python bytecode creation was observed
  during real public-entry runs and disabled in all three entry doors so the
  repository remains clean.
- Pre-fix characterization failed with
  `ModuleNotFoundError: No module named 'loka_rig_common'`. The final focused
  tests report 10 common/Toolbox tests, 13 macOS protocol tests, and the Toolbox
  presentation-rail shell test passing.
- The full configured suite reports 349/349 passed.
- Toolbox runtime verification used commit
  `efd29d6186f6f51fca9992df27fc9b3dd3e0126e` and archive
  `build/loka-rig/archive/toolbox-maciix/20260812T050332Z-efd29d6186f6-flow-scrapbook`.
  Seven presentation PNGs were hashed and visually inspected; the success
  checkout was removed.
- Mavericks runtime verification used the same commit and archive
  `build/autopilot-pr-b-evidence/mavericks-10.9/20260812T050727Z-efd29d6186f6-flow-startup`.
  The atomic marker, settled app capture, desktop-after capture, and success
  cleanup were verified.
- A direct comparison confirmed both manifests contain the same 18 common
  result fields. Both report build/runtime/machine/presentation passed and
  `target_retained=0`.

### Deliberately left outside PR E

- Toolbox recording remains `manual`; automated MAME/QuickTime video is not a
  machine-verdict prerequisite.
- PR C remains parked. This PR does not add disposable input automation beyond
  the proved flow mode.
- Adapter registration is a small explicit table. A plugin/config discovery
  abstraction would add mechanism without deleting a present decision.
- Pre-existing retained macOS diagnostic worktrees are not owned by this PR and
  were not removed.
