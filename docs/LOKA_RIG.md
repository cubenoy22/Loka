# Loka rig orchestration

`scripts/rig/loka-rig.py` is the public host-side entry point for release-preflight
rigs. It resolves one requested ref to an exact commit, delegates platform
mechanisms to one registered adapter, and applies the shared result and cleanup
contract.

```sh
python3 scripts/rig/loka-rig.py run mavericks-10.9 \
  --ref <commit-sha> --mode flow --local-config <macos-local.ini>

python3 scripts/rig/loka-rig.py run toolbox-maciix \
  --ref <commit-sha> --mode flow --local-config <toolbox-local.ini>
```

The public command accepts only facts shared by every adapter: rig ID, ref,
mode, and an optional local mapping. VM names, SSH transport, MAME paths,
scenario internals, and test-only controls remain adapter-owned.

## Registered adapters

| Adapter | Tracked descriptor | Local mapping example | Runtime mechanism |
| --- | --- | --- | --- |
| macOS | `scripts/rig/macos/rigs/mavericks-10.9.ini` | `scripts/rig/macos/rigs/local.example.ini` | Parallels VM, target-local build and scenario |
| Toolbox | `scripts/rig/toolbox/rigs/toolbox-maciix.ini` | `scripts/rig/toolbox/rigs/local.example.ini` | detached checkout, Retro68 build, MAME seven-scenario rail |

The dispatcher identifies a rig only when exactly one adapter directory contains
its descriptor. An unknown or duplicate registration fails closed. Each adapter
parses its own strict descriptor and local-mapping schema; the common layer does
not turn platform build or launch commands into configuration data.

## Shared lifecycle

The common executor owns this sequence:

1. prepare an isolated exact-commit run;
2. build through the adapter;
3. complete the adapter's machine-verdict procedure;
4. collect finalized presentation and diagnostic artifacts;
5. finalize the common manifest;
6. clean up only after successful collection and manifest finalization;
7. rewrite the manifest with the observed post-cleanup retention state.

Failure before the cleanup commit point retains the adapter work area whenever
possible. Artifact manifests refuse symlinks and duplicate field names. Adapter
fields can extend the manifest but cannot redefine common facts.

## Result vocabulary

`run-manifest.txt` separates the overall run from the two evidence lanes:

- `result`: whether the whole requested run, including collection and cleanup,
  succeeded;
- `build_verification`: whether the requested commit built;
- `runtime_verification`: whether the deterministic adapter procedure passed;
- `machine_verdict`: `passed`, `failed-or-not-reached`, or `refused` when a
  machine ran successfully but the reference identity was not eligible to
  authorize a pixel comparison;
- `presentation_status`: whether finalized human evidence was collected;
- `recording_status`: `not-requested`, `manual`, `collected`, or `failed`;
- `target_retained`, `target_workdir`, and `next_diagnostic_command`: observed
  diagnostic retention facts.

A presentation, manifest, or cleanup failure after runtime completion reports
`result=failed` while preserving `machine_verdict=passed`. The later failure
must not rewrite an already-completed machine fact.

The Toolbox adapter stages one strict, atomic golden bundle. Its sole manifest
binds the scenario registry, every PNG digest, clean-build compiler/interface
and Retro68 provenance, and the emulator, verified ROM inventory, RAM, machine,
capture adapter, and pre-launch Boot.hd digest. The bundle identity is eligible
only when it matches `reference_identity_sha256` in the tracked rig descriptor;
`--update-golden` cannot edit or otherwise self-authorize that authority.

Every other manifest field is either immutable run identity, adapter-specific
description, or a SHA-256 entry for a finalized regular file in the archive.
