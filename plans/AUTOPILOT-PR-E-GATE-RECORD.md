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
