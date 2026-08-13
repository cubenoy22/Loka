# macOS Rig Protocol

This note fixes the ownership and transport decisions for the first macOS rig
vertical slice. It is intentionally macOS-specific. The shared `loka-rig`
executor owns only the cross-platform run sequence and result vocabulary;
VM, SSH, Aqua, and target transport remain here.

## Owner boxes

- The orchestrator host owns the detached source checkout, VM transition,
  target work directory, local evidence archive, and final manifest.
- The target VM owns the app process and files while they are being written.
- The TEST-only scenario runner owns content capture, SnapRecord output, and
  atomic ready/completion publication.
- Files cross from the target to the host only after the producing process has
  closed or atomically renamed them. A shared or live-mounted directory is not
  a control or completion channel.

If the VM was already running, the orchestrator borrows it and leaves it
running after a successful run. If the orchestrator started or resumed the VM,
it restores the prior stopped or suspended state only after artifact collection
and manifest finalization succeed. A failure before cleanup leaves the VM,
target work directory, and source checkout available for diagnosis. During
cleanup, the source checkout is removed last, so any cleanup failure retains it
along with the finalized local evidence archive; the manifest reports whether
the target work directory was retained or had already been removed.

## Inspect hold and release

`inspect` reaches the same deterministic settled scenario frame as `flow`,
publishes an atomic `ready` marker, and keeps the app's Main Thread event loop
alive. The orchestrator host owns the hold lifetime.

Release uses one target-local marker in the run directory:

1. The host waits until the finalized `ready` marker is visible over SSH.
2. After the human inspection point, the host uploads `release.tmp` and
   atomically renames it to `release` on the target.
3. The TEST-only runner polls for `release` from its existing Main Thread idle
   callback, publishes the ordinary atomic `complete` marker, and quits.
4. After the target-side runner has checked the finalized artifacts, it
   publishes atomic `verified`. The host treats `verified`, rather than an SSH
   channel close, as the machine-verdict handoff.

The release marker is a control request, not an artifact handoff. It is never
read through a shared folder, does not add a shipping API, and does not install
a signal handler or background thread. A missing release marker keeps the app
held until the host timeout; timeout is a failure and retains the target state.

## Modes and security wall

- `flow`: deterministic Flow/State scenario, settled artifacts, completion,
  then automatic quit.
- `inspect`: deterministic settled frame, ready/hold, host release, completion,
  then quit.
- `input`: not implemented by this slice. Descriptors must state whether a rig
  is disposable before a future adapter may enable it. Input mode must never
  run on the orchestrator host desktop.

Machine verdict and presentation evidence stay separate. Desktop captures are
rig-local human evidence and are not image oracles. SnapRecord and content
capture remain the machine lane.

## Failure retention

Every failure writes the last known stage and a next diagnostic command into
the local manifest when possible. Cleanup is success-only. A collection or
manifest failure is still a run failure, even if the scenario itself completed.
The manifest records whether the target work directory was never created,
retained, or removed; it does not infer retention from the overall result.

## Invocation

Run one exact commit through the tracked descriptor and a machine-local mapping:

```sh
python3 scripts/rig/loka-rig.py run mavericks-10.9 \
  --ref <commit-sha> --mode flow --local-config <local.ini>
```

Use `--mode inspect` for the host-owned hold/release path. Automated protocol
verification may call the direct macOS adapter with `--release-after-ready`;
ordinary public runs omit that test-only adapter control and release only after
the operator presses Enter.
